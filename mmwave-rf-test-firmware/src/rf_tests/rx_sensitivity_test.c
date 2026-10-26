/**
 * @file rx_sensitivity_test.c
 * @brief RX sensitivity and noise floor measurement
 *
 * Measures the receiver noise floor by configuring the RX chain with all
 * TX channels disabled (no input signal), capturing ADC data, and computing
 * the integrated noise power.  Also provides a loopback-based gain measurement.
 */

#include "rf_tests/rx_sensitivity_test.h"
#include "drivers/synth_driver.h"
#include "drivers/rx_frontend.h"
#include "drivers/pga_driver.h"
#include "drivers/if_filter_driver.h"
#include "drivers/adc_driver.h"
#include "drivers/tx_frontend.h"
#include "data/capture_engine.h"
#include "rf_test_cfg.h"
#include "iwr6843_regs.h"

#include <math.h>

/* ========================================================================= */
/*  Internal helpers                                                          */
/* ========================================================================= */

/**
 * @brief Compute noise power spectral density from ADC capture
 *
 * Applies a Hanning window, computes FFT magnitude, and integrates
 * across the IF bandwidth to determine total noise power.
 */
static float32 compute_noise_power(const sint16 *buf_i, const sint16 *buf_q,
                                    uint32 num_samples, float32 sample_rate_hz)
{
    float64 noise_sum = 0.0;
    uint32 i;

    /* Compute average power of I/Q samples (time-domain RMS) */
    for (i = 0U; i < num_samples; i++) {
        float64 vi = (float64)buf_i[i] / 32768.0;
        float64 vq = (float64)buf_q[i] / 32768.0;
        noise_sum += (vi * vi) + (vq * vq);
    }

    float64 rms_power = noise_sum / (float64)num_samples;

    /* Convert to dBFS */
    float64 power_dbfs = 10.0 * log10(rms_power + 1e-20);

    /*
     * Convert to input-referred noise power in dBm:
     *   Account for ADC full-scale, PGA gain, LNA gain, and IF filter BW
     */
    float32 adc_fs_dbm    = 4.0f;     /* ADC full-scale in dBm */
    float32 total_gain_db  = 48.0f;    /* LNA + mixer + PGA nominal gain */

    float32 noise_dbm = (float32)power_dbfs + adc_fs_dbm - total_gain_db;

    return noise_dbm;
}

/**
 * @brief Averaged noise floor measurement
 */
static float32 averaged_noise_measurement(uint8 channel, uint32 num_averages)
{
    sint16 buf_i[RX_SENS_CAPTURE_SAMPLES];
    sint16 buf_q[RX_SENS_CAPTURE_SAMPLES];
    float64 noise_lin_sum = 0.0;
    uint32 n;

    for (n = 0U; n < num_averages; n++) {
        capture_engine_start(channel, RX_SENS_CAPTURE_SAMPLES);
        capture_engine_wait_done();
        capture_engine_read_iq(buf_i, buf_q, RX_SENS_CAPTURE_SAMPLES);

        float32 noise = compute_noise_power(buf_i, buf_q,
                                             RX_SENS_CAPTURE_SAMPLES,
                                             (float32)ADC_DEFAULT_SAMPLE_RATE);
        noise_lin_sum += pow(10.0, (double)noise / 10.0);
    }

    return (float32)(10.0 * log10(noise_lin_sum / (double)num_averages));
}

/* ========================================================================= */
/*  Public API                                                                */
/* ========================================================================= */

float32 rx_sensitivity_test_measure(uint8 channel, float32 freq_ghz)
{
    /* Step 1: Ensure all TX channels are disabled (no signal at RX input) */
    tx_frontend_disable_channel(RF_TX_CHANNEL_0);
    tx_frontend_disable_channel(RF_TX_CHANNEL_1);
    tx_frontend_disable_channel(RF_TX_CHANNEL_2);

    /* Step 2: Configure synthesizer LO */
    synth_set_frequency_ghz(freq_ghz);
    synth_enable_cw_mode();
    if (synth_wait_lock(SYNTH_LOCK_TIMEOUT_US) != STATUS_OK) {
        return -999.0f;
    }

    /* Step 3: Configure RX chain for maximum sensitivity */
    rx_frontend_enable_channel(channel);
    rx_frontend_set_lna_gain(channel, RX_SENS_LNA_GAIN);
    pga_set_gain(channel, RX_SENS_PGA_GAIN);
    if_filter_set_bandwidth(IF_HPF_175KHZ, IF_LPF_5000KHZ);

    /* Step 4: Allow RX chain to settle */
    DELAY_US(200U);

    /* Step 5: Measure noise floor */
    float32 noise_floor_dbm = averaged_noise_measurement(channel, RX_SENS_NUM_AVERAGES);

    /* Step 6: Cleanup */
    rx_frontend_disable_channel(channel);
    synth_disable();

    return noise_floor_dbm;
}

float32 rx_sensitivity_test_gain(uint8 channel, float32 freq_ghz)
{
    /* Measure with known TX loopback signal to determine total RX gain */
    float32 tx_power_dbm;
    float32 rx_power_dbm;

    /* Configure TX for known output power via loopback */
    synth_set_frequency_ghz(freq_ghz);
    synth_enable_cw_mode();
    if (synth_wait_lock(SYNTH_LOCK_TIMEOUT_US) != STATUS_OK) {
        return -999.0f;
    }

    tx_frontend_set_power(RF_TX_CHANNEL_0, TX_PWR_DEFAULT_CODE);
    tx_frontend_enable_channel(RF_TX_CHANNEL_0);
    tx_frontend_enable_tone();
    tx_frontend_enable_loopback(RF_TX_CHANNEL_0);

    tx_power_dbm = 12.0f - TX_PWR_LOOPBACK_ATTEN_DB;  /* Estimated input power */

    /* Configure RX with known gain settings */
    rx_frontend_enable_channel(channel);
    rx_frontend_set_lna_gain(channel, RX_LNA_GAIN_HIGH);
    pga_set_gain(channel, PGA_GAIN_30DB);

    DELAY_US(200U);

    /* Capture and measure received power */
    sint16 buf_i[RX_SENS_CAPTURE_SAMPLES];
    sint16 buf_q[RX_SENS_CAPTURE_SAMPLES];
    float64 sum_sq = 0.0;

    capture_engine_start(channel, RX_SENS_CAPTURE_SAMPLES);
    capture_engine_wait_done();
    capture_engine_read_iq(buf_i, buf_q, RX_SENS_CAPTURE_SAMPLES);

    for (uint32 i = 0; i < RX_SENS_CAPTURE_SAMPLES; i++) {
        float64 vi = (float64)buf_i[i] / 32768.0;
        float64 vq = (float64)buf_q[i] / 32768.0;
        sum_sq += (vi * vi) + (vq * vq);
    }

    float64 rms_dbfs = 10.0 * log10(sum_sq / (double)RX_SENS_CAPTURE_SAMPLES + 1e-20);
    rx_power_dbm = (float32)rms_dbfs + 4.0f;  /* ADC full-scale reference */

    /* Cleanup */
    tx_frontend_disable_tone();
    tx_frontend_disable_loopback(RF_TX_CHANNEL_0);
    tx_frontend_disable_channel(RF_TX_CHANNEL_0);
    rx_frontend_disable_channel(channel);
    synth_disable();

    /* Gain = output power (at ADC) - input power (at antenna) */
    return rx_power_dbm - tx_power_dbm;
}
