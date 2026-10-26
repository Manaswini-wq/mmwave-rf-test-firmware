/**
 * @file tx_evm_test.c
 * @brief TX Error Vector Magnitude (EVM) measurement
 *
 * EVM quantifies the difference between the ideal transmitted signal and
 * the actual signal.  For a CW tone, EVM is dominated by phase noise,
 * amplitude noise, and spurious content.
 *
 * Method:
 *   1. Transmit CW tone via loopback
 *   2. Capture I/Q samples at the RX ADC
 *   3. Estimate ideal carrier (magnitude and phase from averaged phasor)
 *   4. Compute error vector between each sample and the ideal
 *   5. EVM = 20*log10(rms_error / rms_signal)
 */

#include "rf_tests/tx_evm_test.h"
#include "drivers/synth_driver.h"
#include "drivers/tx_frontend.h"
#include "drivers/rx_frontend.h"
#include "drivers/pga_driver.h"
#include "drivers/adc_driver.h"
#include "data/capture_engine.h"
#include "rf_test_cfg.h"
#include "iwr6843_regs.h"

#include <math.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

/* ========================================================================= */
/*  Internal helpers                                                          */
/* ========================================================================= */

/**
 * @brief Compute EVM from captured I/Q samples
 *
 * Estimates the ideal carrier as the mean phasor, then computes the
 * RMS error relative to the ideal.
 */
static float32 compute_evm_db(const sint16 *samples_i, const sint16 *samples_q,
                               uint32 num_samples)
{
    /* Step 1: Compute mean I and Q (ideal carrier estimate) */
    float64 mean_i = 0.0, mean_q = 0.0;
    for (uint32 n = 0U; n < num_samples; n++) {
        mean_i += (float64)samples_i[n];
        mean_q += (float64)samples_q[n];
    }
    mean_i /= (float64)num_samples;
    mean_q /= (float64)num_samples;

    /* Step 2: Compute RMS signal power and RMS error power */
    float64 signal_power = 0.0;
    float64 error_power  = 0.0;

    for (uint32 n = 0U; n < num_samples; n++) {
        float64 si = (float64)samples_i[n];
        float64 sq = (float64)samples_q[n];

        /* Signal power */
        signal_power += si * si + sq * sq;

        /* Error vector: sample - ideal */
        float64 ei = si - mean_i;
        float64 eq = sq - mean_q;
        error_power += ei * ei + eq * eq;
    }

    signal_power /= (float64)num_samples;
    error_power  /= (float64)num_samples;

    /* Step 3: EVM in dB */
    float32 evm_db = (float32)(10.0 * log10(error_power / (signal_power + 1e-20)));

    return evm_db;
}

/* ========================================================================= */
/*  Public API                                                                */
/* ========================================================================= */

float32 tx_evm_test_measure(uint8 channel, float32 freq_ghz)
{
    sint16 buf_i[EVM_NUM_SYMBOLS * EVM_CAPTURE_OVERSAMPLE];
    sint16 buf_q[EVM_NUM_SYMBOLS * EVM_CAPTURE_OVERSAMPLE];
    uint32 capture_len = EVM_NUM_SYMBOLS * EVM_CAPTURE_OVERSAMPLE;

    /* Configure CW tone via loopback */
    synth_set_frequency_ghz(freq_ghz);
    synth_enable_cw_mode();
    if (synth_wait_lock(SYNTH_LOCK_TIMEOUT_US) != STATUS_OK) {
        return -999.0f;
    }

    tx_frontend_set_power(channel, TX_PWR_DEFAULT_CODE);
    tx_frontend_enable_channel(channel);
    tx_frontend_enable_tone();
    tx_frontend_enable_loopback(channel);

    uint8 rx_ch = channel % RF_NUM_RX_CHANNELS;
    rx_frontend_enable_channel(rx_ch);
    pga_set_gain(rx_ch, PGA_GAIN_30DB);
    DELAY_US(200U);

    /* Averaged EVM measurement */
    float64 evm_sum = 0.0;
    uint32 num_avg = 8U;

    for (uint32 avg = 0U; avg < num_avg; avg++) {
        capture_engine_start(rx_ch, capture_len);
        capture_engine_wait_done();
        capture_engine_read_iq(buf_i, buf_q, capture_len);

        float32 evm = compute_evm_db(buf_i, buf_q, capture_len);
        evm_sum += (float64)evm;
    }

    /* Cleanup */
    tx_frontend_disable_tone();
    tx_frontend_disable_loopback(channel);
    tx_frontend_disable_channel(channel);
    rx_frontend_disable_channel(rx_ch);
    synth_disable();

    return (float32)(evm_sum / (float64)num_avg);
}
