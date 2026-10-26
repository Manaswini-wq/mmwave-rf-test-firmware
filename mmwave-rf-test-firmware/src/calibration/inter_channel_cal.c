/**
 * @file inter_channel_cal.c
 * @brief Inter-channel timing, gain, and phase calibration
 *
 * Calibrates the relative timing, gain, and phase offsets between all RX
 * channels.  This is critical for beamforming and angle-of-arrival
 * estimation accuracy.
 *
 * Method:
 *   1. Inject a wideband chirp via TX loopback to all RX channels
 *   2. Capture the IF response on each channel
 *   3. Cross-correlate each channel with the reference to determine
 *      timing offsets (sub-sample resolution via parabolic interpolation)
 *   4. Compare magnitude and phase at the chirp center frequency
 *   5. Apply corrections to DMA timing registers and PGA/phase registers
 */

#include "calibration/inter_channel_cal.h"
#include "drivers/synth_driver.h"
#include "drivers/tx_frontend.h"
#include "drivers/rx_frontend.h"
#include "drivers/pga_driver.h"
#include "drivers/adc_driver.h"
#include "data/capture_engine.h"
#include "data/dma_manager.h"
#include "protocol/uart_cmd_handler.h"
#include "rf_test_cfg.h"

#include <math.h>
#include <string.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

/* ========================================================================= */
/*  Calibration state                                                         */
/* ========================================================================= */
#define XCORR_SAMPLES  1024U

static InterChCalResult_t g_inter_cal[RF_NUM_RX_CHANNELS];
static boolean g_inter_cal_valid = FALSE;

/* ========================================================================= */
/*  Internal helpers                                                          */
/* ========================================================================= */

/**
 * @brief Cross-correlate two real signals and find peak offset
 *
 * Returns the lag (in samples) where correlation is maximized.
 * Uses parabolic interpolation for sub-sample accuracy.
 */
static float64 cross_correlate_peak(const sint16 *ref, const sint16 *sig,
                                     uint32 n, sint32 max_lag)
{
    float64 best_corr = -1e30;
    sint32  best_lag = 0;

    for (sint32 lag = -max_lag; lag <= max_lag; lag++) {
        float64 corr = 0.0;
        uint32 count = 0U;

        for (uint32 i = 0U; i < n; i++) {
            sint32 j = (sint32)i + lag;
            if (j >= 0 && j < (sint32)n) {
                corr += (float64)ref[i] * (float64)sig[j];
                count++;
            }
        }
        corr /= (float64)(count > 0U ? count : 1U);

        if (corr > best_corr) {
            best_corr = corr;
            best_lag = lag;
        }
    }

    /* Sub-sample parabolic interpolation */
    float64 refined_lag = (float64)best_lag;
    if (best_lag > -max_lag && best_lag < max_lag) {
        /* Recompute correlation at best_lag-1 and best_lag+1 */
        float64 corr_left = 0.0, corr_right = 0.0;
        uint32 cl = 0, cr = 0;

        for (uint32 i = 0U; i < n; i++) {
            sint32 jl = (sint32)i + best_lag - 1;
            sint32 jr = (sint32)i + best_lag + 1;
            if (jl >= 0 && jl < (sint32)n) { corr_left += (float64)ref[i] * (float64)sig[jl]; cl++; }
            if (jr >= 0 && jr < (sint32)n) { corr_right += (float64)ref[i] * (float64)sig[jr]; cr++; }
        }
        if (cl > 0) corr_left /= (float64)cl;
        if (cr > 0) corr_right /= (float64)cr;

        float64 denom = corr_left - 2.0 * best_corr + corr_right;
        if (fabs(denom) > 1e-10) {
            refined_lag += 0.5 * (corr_left - corr_right) / denom;
        }
    }

    return refined_lag;
}

/**
 * @brief Compute mean phase of I/Q samples
 */
static float32 compute_mean_phase(const sint16 *buf_i, const sint16 *buf_q, uint32 n)
{
    float64 si = 0.0, sq = 0.0;
    for (uint32 k = 0U; k < n; k++) {
        si += (float64)buf_i[k];
        sq += (float64)buf_q[k];
    }
    return (float32)(atan2(sq, si) * 180.0 / M_PI);
}

static float32 compute_rms_magnitude(const sint16 *buf_i, const sint16 *buf_q, uint32 n)
{
    float64 sum = 0.0;
    for (uint32 k = 0U; k < n; k++) {
        sum += (float64)buf_i[k] * (float64)buf_i[k] + (float64)buf_q[k] * (float64)buf_q[k];
    }
    return (float32)(20.0 * log10(sqrt(sum / (float64)n) / 32768.0 + 1e-20));
}

/* ========================================================================= */
/*  Public API                                                                */
/* ========================================================================= */

Status_t inter_channel_cal_run(void)
{
    sint16 ref_i[XCORR_SAMPLES], ref_q[XCORR_SAMPLES];
    sint16 ch_i[XCORR_SAMPLES], ch_q[XCORR_SAMPLES];

    /* Setup CW tone via loopback */
    synth_set_frequency_ghz(RF_FREQ_CENTER_GHZ);
    synth_enable_cw_mode();
    if (synth_wait_lock(SYNTH_LOCK_TIMEOUT_US) != STATUS_OK) {
        return STATUS_HW_FAULT;
    }

    tx_frontend_set_power(RF_TX_CHANNEL_0, TX_PWR_DEFAULT_CODE);
    tx_frontend_enable_channel(RF_TX_CHANNEL_0);
    tx_frontend_enable_tone();
    tx_frontend_enable_loopback(RF_TX_CHANNEL_0);

    /* Enable all RX channels */
    for (uint8 ch = 0U; ch < RF_NUM_RX_CHANNELS; ch++) {
        rx_frontend_enable_channel(ch);
        pga_set_gain(ch, PGA_GAIN_30DB);
    }
    DELAY_US(300U);

    uart_cmd_send_string("Inter-Channel Cal: measuring...\r\n");

    /* Capture reference channel */
    uint8 ref_ch = RX_GP_CAL_REF_CHANNEL;
    capture_engine_start(ref_ch, XCORR_SAMPLES);
    capture_engine_wait_done();
    capture_engine_read_iq(ref_i, ref_q, XCORR_SAMPLES);

    float32 ref_mag = compute_rms_magnitude(ref_i, ref_q, XCORR_SAMPLES);
    float32 ref_phase = compute_mean_phase(ref_i, ref_q, XCORR_SAMPLES);

    /* Calibrate each channel relative to reference */
    for (uint8 ch = 0U; ch < RF_NUM_RX_CHANNELS; ch++) {
        if (ch == ref_ch) {
            g_inter_cal[ch].time_offset_ps  = 0.0f;
            g_inter_cal[ch].gain_offset_db  = 0.0f;
            g_inter_cal[ch].phase_offset_deg = 0.0f;
            continue;
        }

        capture_engine_start(ch, XCORR_SAMPLES);
        capture_engine_wait_done();
        capture_engine_read_iq(ch_i, ch_q, XCORR_SAMPLES);

        /* Timing offset via cross-correlation */
        float64 lag_samples = cross_correlate_peak(ref_i, ch_i, XCORR_SAMPLES, 16);
        float64 sample_period_ps = 1e12 / (float64)ADC_DEFAULT_SAMPLE_RATE;
        g_inter_cal[ch].time_offset_ps = (float32)(lag_samples * sample_period_ps);

        /* Gain offset */
        float32 ch_mag = compute_rms_magnitude(ch_i, ch_q, XCORR_SAMPLES);
        g_inter_cal[ch].gain_offset_db = ch_mag - ref_mag;

        /* Phase offset */
        float32 ch_phase = compute_mean_phase(ch_i, ch_q, XCORR_SAMPLES);
        float32 pdiff = ch_phase - ref_phase;
        while (pdiff > 180.0f) pdiff -= 360.0f;
        while (pdiff < -180.0f) pdiff += 360.0f;
        g_inter_cal[ch].phase_offset_deg = pdiff;

        /* Apply corrections */
        dma_manager_set_timing_offset(ch, (sint32)(-g_inter_cal[ch].time_offset_ps));
        rx_frontend_set_phase_offset(ch, (sint16)(-pdiff * 10.0f));
    }

    g_inter_cal_valid = TRUE;

    /* Cleanup */
    tx_frontend_disable_tone();
    tx_frontend_disable_loopback(RF_TX_CHANNEL_0);
    tx_frontend_disable_channel(RF_TX_CHANNEL_0);
    for (uint8 ch = 0U; ch < RF_NUM_RX_CHANNELS; ch++) {
        rx_frontend_disable_channel(ch);
    }
    synth_disable();

    /* Verify all channels within tolerance */
    boolean pass = TRUE;
    for (uint8 ch = 0U; ch < RF_NUM_RX_CHANNELS; ch++) {
        if (fabsf(g_inter_cal[ch].time_offset_ps) > INTER_CH_TIME_TOL_PS ||
            fabsf(g_inter_cal[ch].gain_offset_db) > INTER_CH_GAIN_TOL_DB ||
            fabsf(g_inter_cal[ch].phase_offset_deg) > INTER_CH_PHASE_TOL_DEG) {
            pass = FALSE;
        }
    }

    uart_cmd_send_string(pass ? "  Result: PASS\r\n" : "  Result: FAIL\r\n");
    return pass ? STATUS_OK : STATUS_CAL_FAIL;
}

Status_t inter_channel_cal_get(uint8 channel, InterChCalResult_t *result)
{
    if (channel >= RF_NUM_RX_CHANNELS || result == NULL_PTR || !g_inter_cal_valid) {
        return STATUS_NOT_READY;
    }
    *result = g_inter_cal[channel];
    return STATUS_OK;
}
