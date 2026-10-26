/**
 * @file tx_power_test.c
 * @brief TX power measurement test implementation
 *
 * Configures the TX chain to emit a continuous-wave (CW) test tone, enables
 * the internal loopback path, captures ADC samples, and computes the TX
 * output power in dBm.  The loopback path attenuation is compensated to
 * yield the power at the antenna port.
 */

#include "rf_tests/tx_power_test.h"
#include "drivers/synth_driver.h"
#include "drivers/tx_frontend.h"
#include "drivers/rx_frontend.h"
#include "drivers/adc_driver.h"
#include "drivers/pga_driver.h"
#include "data/capture_engine.h"
#include "rf_test_cfg.h"
#include "iwr6843_regs.h"

#include <math.h>

/* ========================================================================= */
/*  Private helpers                                                           */
/* ========================================================================= */

/**
 * @brief Compute power in dBm from complex ADC samples
 *
 * Calculates RMS power of I/Q samples, converts to dBm relative to
 * full-scale, and applies loopback attenuation compensation.
 */
static float32 compute_power_dbm(const sint16 *samples_i, const sint16 *samples_q,
                                  uint32 num_samples, float32 loopback_atten_db)
{
    float64 sum_sq = 0.0;
    uint32 i;

    for (i = 0U; i < num_samples; i++) {
        float64 vi = (float64)samples_i[i];
        float64 vq = (float64)samples_q[i];
        sum_sq += (vi * vi) + (vq * vq);
    }

    /* RMS voltage (normalized to ADC full-scale = 1.0) */
    float64 rms_sq = sum_sq / (float64)num_samples;
    float64 full_scale = 32768.0 * 32768.0;  /* 16-bit signed */

    /* Power relative to full-scale in dBFS */
    float64 power_dbfs = 10.0 * log10(rms_sq / full_scale + 1e-20);

    /*
     * Convert dBFS to dBm:
     *   ADC full-scale corresponds to approximately +4 dBm at the ADC input.
     *   Add loopback attenuation to get power at the TX PA output.
     */
    float32 adc_fullscale_dbm = 4.0f;
    float32 power_dbm = (float32)power_dbfs + adc_fullscale_dbm + loopback_atten_db;

    return power_dbm;
}

/**
 * @brief Average multiple power measurements for improved accuracy
 */
static float32 averaged_power_measurement(uint8 channel, uint32 num_averages)
{
    sint16 capture_buf_i[ADC_BUFFER_SIZE_SAMPLES];
    sint16 capture_buf_q[ADC_BUFFER_SIZE_SAMPLES];
    float64 power_sum = 0.0;
    uint32 n;

    for (n = 0U; n < num_averages; n++) {
        /* Trigger ADC capture */
        capture_engine_start(channel, ADC_BUFFER_SIZE_SAMPLES);
        capture_engine_wait_done();
        capture_engine_read_iq(capture_buf_i, capture_buf_q, ADC_BUFFER_SIZE_SAMPLES);

        float32 pwr = compute_power_dbm(capture_buf_i, capture_buf_q,
                                          ADC_BUFFER_SIZE_SAMPLES,
                                          TX_PWR_LOOPBACK_ATTEN_DB);
        /* Accumulate in linear domain for proper averaging */
        power_sum += pow(10.0, (double)pwr / 10.0);
    }

    /* Convert averaged linear power back to dBm */
    float32 avg_dbm = (float32)(10.0 * log10(power_sum / (double)num_averages));
    return avg_dbm;
}

/* ========================================================================= */
/*  Public API                                                                */
/* ========================================================================= */

float32 tx_power_test_measure(uint8 channel, float32 freq_ghz)
{
    /* Step 1: Configure synthesizer to CW at requested frequency */
    synth_set_frequency_ghz(freq_ghz);
    synth_enable_cw_mode();

    /* Wait for PLL lock */
    if (synth_wait_lock(SYNTH_LOCK_TIMEOUT_US) != STATUS_OK) {
        return -999.0f;  /* Indicate PLL lock failure */
    }

    /* Step 2: Configure TX chain */
    tx_frontend_set_power(channel, TX_PWR_DEFAULT_CODE);
    tx_frontend_enable_channel(channel);
    tx_frontend_enable_tone();

    /* Step 3: Enable internal loopback path (TX -> RX via on-chip coupler) */
    tx_frontend_enable_loopback(channel);

    /* Step 4: Configure RX path for loopback measurement */
    rx_frontend_enable_channel(channel % RF_NUM_RX_CHANNELS);
    pga_set_gain(channel % RF_NUM_RX_CHANNELS, PGA_GAIN_24DB);

    /* Allow signal chain to settle */
    DELAY_US(TX_PWR_SETTLE_TIME_US);

    /* Step 5: Perform averaged power measurement */
    float32 power_dbm = averaged_power_measurement(
        channel % RF_NUM_RX_CHANNELS, TX_PWR_NUM_AVERAGES);

    /* Step 6: Disable test mode and clean up */
    tx_frontend_disable_tone();
    tx_frontend_disable_loopback(channel);
    tx_frontend_disable_channel(channel);
    synth_disable();

    return power_dbm;
}

Status_t tx_power_test_sweep(uint8 channel, float32 *results, uint32 num_pts)
{
    if (results == NULL_PTR || num_pts == 0U) {
        return STATUS_INVALID_PARAM;
    }

    float32 freq_start = RF_FREQ_MIN_GHZ;
    float32 freq_step = (RF_FREQ_MAX_GHZ - RF_FREQ_MIN_GHZ) / (float32)(num_pts - 1U);

    for (uint32 i = 0U; i < num_pts; i++) {
        float32 freq = freq_start + freq_step * (float32)i;
        results[i] = tx_power_test_measure(channel, freq);

        if (results[i] < -900.0f) {
            return STATUS_HW_FAULT;  /* PLL lock failure */
        }
    }

    return STATUS_OK;
}

float32 tx_power_test_channel_variation(float32 freq_ghz)
{
    float32 powers[RF_NUM_TX_CHANNELS];
    float32 min_pwr = 999.0f;
    float32 max_pwr = -999.0f;

    for (uint8 ch = 0U; ch < RF_NUM_TX_CHANNELS; ch++) {
        powers[ch] = tx_power_test_measure(ch, freq_ghz);
        if (powers[ch] < min_pwr) min_pwr = powers[ch];
        if (powers[ch] > max_pwr) max_pwr = powers[ch];
    }

    return (max_pwr - min_pwr);
}
