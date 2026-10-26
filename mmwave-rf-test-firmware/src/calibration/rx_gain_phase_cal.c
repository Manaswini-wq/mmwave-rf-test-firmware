/**
 * @file rx_gain_phase_cal.c
 * @brief RX gain and phase calibration algorithm
 *
 * Calibrates per-channel gain and phase offsets relative to a reference
 * channel.  Uses a TX loopback CW tone to illuminate all RX channels
 * simultaneously, then measures the amplitude and phase of each channel's
 * response.  Correction factors are computed and applied to the PGA gain
 * and phase-shift registers.
 *
 * Algorithm:
 *   1. Enable TX loopback tone on a known TX channel
 *   2. Capture I/Q data on all RX channels
 *   3. Compute magnitude and phase of each channel's response
 *   4. Compute gain and phase offsets relative to reference channel
 *   5. Apply corrections and verify
 */

#include "calibration/rx_gain_phase_cal.h"
#include "drivers/synth_driver.h"
#include "drivers/tx_frontend.h"
#include "drivers/rx_frontend.h"
#include "drivers/pga_driver.h"
#include "drivers/adc_driver.h"
#include "data/capture_engine.h"
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
static RxCalCoeffs_t g_rx_cal[RF_NUM_RX_CHANNELS];
static boolean g_rx_cal_valid[RF_NUM_RX_CHANNELS] = {FALSE};

#define CAL_CAPTURE_LEN  1024U

/* ========================================================================= */
/*  Internal helpers                                                          */
/* ========================================================================= */

/**
 * @brief Compute mean magnitude and phase from I/Q capture
 */
static void compute_channel_response(const sint16 *buf_i, const sint16 *buf_q,
                                      uint32 n, float32 *mag_db, float32 *phase_deg)
{
    float64 sum_i = 0.0, sum_q = 0.0;

    for (uint32 k = 0U; k < n; k++) {
        sum_i += (float64)buf_i[k];
        sum_q += (float64)buf_q[k];
    }

    float64 mean_i = sum_i / (float64)n;
    float64 mean_q = sum_q / (float64)n;

    float64 magnitude = sqrt(mean_i * mean_i + mean_q * mean_q);
    *mag_db = (float32)(20.0 * log10(magnitude / 32768.0 + 1e-20));
    *phase_deg = (float32)(atan2(mean_q, mean_i) * 180.0 / M_PI);
}

/**
 * @brief Measure response of all RX channels
 */
static void measure_all_channels(float32 *gains_db, float32 *phases_deg)
{
    sint16 buf_i[CAL_CAPTURE_LEN];
    sint16 buf_q[CAL_CAPTURE_LEN];

    for (uint8 ch = 0U; ch < RF_NUM_RX_CHANNELS; ch++) {
        rx_frontend_enable_channel(ch);
        pga_set_gain(ch, PGA_GAIN_30DB);
    }
    DELAY_US(200U);

    for (uint8 ch = 0U; ch < RF_NUM_RX_CHANNELS; ch++) {
        capture_engine_start(ch, CAL_CAPTURE_LEN);
        capture_engine_wait_done();
        capture_engine_read_iq(buf_i, buf_q, CAL_CAPTURE_LEN);

        compute_channel_response(buf_i, buf_q, CAL_CAPTURE_LEN,
                                  &gains_db[ch], &phases_deg[ch]);
    }
}

/**
 * @brief Normalize phase to [-180, 180] degrees
 */
static float32 normalize_phase(float32 phase_deg)
{
    while (phase_deg > 180.0f)  phase_deg -= 360.0f;
    while (phase_deg < -180.0f) phase_deg += 360.0f;
    return phase_deg;
}

/* ========================================================================= */
/*  Public API                                                                */
/* ========================================================================= */

Status_t rx_gain_phase_cal_run(uint8 ref_channel)
{
    if (ref_channel >= RF_NUM_RX_CHANNELS) {
        return STATUS_INVALID_PARAM;
    }

    float32 gains_db[RF_NUM_RX_CHANNELS];
    float32 phases_deg[RF_NUM_RX_CHANNELS];

    /* Setup TX loopback tone */
    synth_set_frequency_ghz(RF_FREQ_CENTER_GHZ);
    synth_enable_cw_mode();
    if (synth_wait_lock(SYNTH_LOCK_TIMEOUT_US) != STATUS_OK) {
        return STATUS_HW_FAULT;
    }

    tx_frontend_set_power(RF_TX_CHANNEL_0, TX_PWR_DEFAULT_CODE);
    tx_frontend_enable_channel(RF_TX_CHANNEL_0);
    tx_frontend_enable_tone();
    tx_frontend_enable_loopback(RF_TX_CHANNEL_0);

    uart_cmd_send_string("RX Gain/Phase Cal: measuring...\r\n");

    /* Iterative calibration */
    for (uint32 iter = 0U; iter < CAL_MAX_ITERATIONS; iter++) {
        measure_all_channels(gains_db, phases_deg);

        /* Compute offsets relative to reference */
        float32 ref_gain = gains_db[ref_channel];
        float32 ref_phase = phases_deg[ref_channel];

        float32 max_gain_err = 0.0f;
        float32 max_phase_err = 0.0f;

        for (uint8 ch = 0U; ch < RF_NUM_RX_CHANNELS; ch++) {
            g_rx_cal[ch].gain_offset_db = gains_db[ch] - ref_gain;
            g_rx_cal[ch].phase_offset_deg = normalize_phase(phases_deg[ch] - ref_phase);

            float32 ge = fabsf(g_rx_cal[ch].gain_offset_db);
            float32 pe = fabsf(g_rx_cal[ch].phase_offset_deg);
            if (ge > max_gain_err) max_gain_err = ge;
            if (pe > max_phase_err) max_phase_err = pe;
        }

        /* Apply PGA gain corrections */
        for (uint8 ch = 0U; ch < RF_NUM_RX_CHANNELS; ch++) {
            if (ch == ref_channel) continue;

            /* Adjust PGA gain register to compensate gain offset */
            sint16 pga_adjustment = (sint16)(g_rx_cal[ch].gain_offset_db / 6.0f);
            uint8 current_gain = pga_get_gain(ch);
            uint8 new_gain = (uint8)CLAMP((sint16)current_gain - pga_adjustment,
                                           0, PGA_GAIN_48DB);
            pga_set_gain(ch, new_gain);

            /* Apply phase correction via RX frontend phase register */
            rx_frontend_set_phase_offset(ch,
                (sint16)(-g_rx_cal[ch].phase_offset_deg * 10.0f));
        }

        /* Check convergence */
        if (max_gain_err <= RX_GP_CAL_GAIN_TOL_DB &&
            max_phase_err <= RX_GP_CAL_PHASE_TOL_DEG) {
            uart_cmd_send_string("  Converged at iteration ");
            char c = '0' + (char)(iter + 1U);
            uart_cmd_send_string(&c);
            uart_cmd_send_string("\r\n");
            break;
        }
    }

    /* Verify final results */
    measure_all_channels(gains_db, phases_deg);
    boolean all_pass = TRUE;
    for (uint8 ch = 0U; ch < RF_NUM_RX_CHANNELS; ch++) {
        g_rx_cal[ch].gain_offset_db = gains_db[ch] - gains_db[ref_channel];
        g_rx_cal[ch].phase_offset_deg = normalize_phase(
            phases_deg[ch] - phases_deg[ref_channel]);
        g_rx_cal_valid[ch] = TRUE;

        if (fabsf(g_rx_cal[ch].gain_offset_db) > RX_GP_CAL_GAIN_TOL_DB ||
            fabsf(g_rx_cal[ch].phase_offset_deg) > RX_GP_CAL_PHASE_TOL_DEG) {
            all_pass = FALSE;
        }
    }

    /* Cleanup */
    tx_frontend_disable_tone();
    tx_frontend_disable_loopback(RF_TX_CHANNEL_0);
    tx_frontend_disable_channel(RF_TX_CHANNEL_0);
    for (uint8 ch = 0U; ch < RF_NUM_RX_CHANNELS; ch++) {
        rx_frontend_disable_channel(ch);
    }
    synth_disable();

    return all_pass ? STATUS_OK : STATUS_CAL_FAIL;
}

Status_t rx_gain_phase_cal_get_coeffs(uint8 channel, RxCalCoeffs_t *coeffs)
{
    if (channel >= RF_NUM_RX_CHANNELS || coeffs == NULL_PTR) {
        return STATUS_INVALID_PARAM;
    }
    if (!g_rx_cal_valid[channel]) {
        return STATUS_NOT_READY;
    }
    *coeffs = g_rx_cal[channel];
    return STATUS_OK;
}

Status_t rx_gain_phase_cal_apply(uint8 channel)
{
    if (channel >= RF_NUM_RX_CHANNELS || !g_rx_cal_valid[channel]) {
        return STATUS_NOT_READY;
    }
    /* Re-apply stored corrections */
    sint16 pga_adj = (sint16)(g_rx_cal[channel].gain_offset_db / 6.0f);
    uint8 base_gain = PGA_GAIN_30DB;
    pga_set_gain(channel, (uint8)CLAMP((sint16)base_gain - pga_adj, 0, PGA_GAIN_48DB));
    rx_frontend_set_phase_offset(channel,
        (sint16)(-g_rx_cal[channel].phase_offset_deg * 10.0f));
    return STATUS_OK;
}
