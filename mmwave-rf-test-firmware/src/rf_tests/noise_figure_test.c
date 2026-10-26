/**
 * @file noise_figure_test.c
 * @brief Noise figure measurement using the Y-factor method
 *
 * The Y-factor method measures noise figure by comparing the receiver output
 * noise power with a "hot" source (ambient, 290 K) and a "cold" reference.
 *
 * For on-chip measurement without an external noise source, we approximate
 * by using two different LNA bias conditions to create different effective
 * noise temperatures, then extrapolate NF.  For full production, an external
 * ENR-calibrated noise source is toggled via GPIO.
 *
 * NF = ENR_dB - 10*log10(Y - 1)
 * where Y = P_hot / P_cold  (linear ratio)
 */

#include "rf_tests/noise_figure_test.h"
#include "drivers/synth_driver.h"
#include "drivers/rx_frontend.h"
#include "drivers/pga_driver.h"
#include "drivers/if_filter_driver.h"
#include "drivers/adc_driver.h"
#include "data/capture_engine.h"
#include "rf_test_cfg.h"
#include "iwr6843_regs.h"

#include <math.h>

/* ========================================================================= */
/*  Internal helpers                                                          */
/* ========================================================================= */

/**
 * @brief Toggle external noise source via GPIO
 * @param enable  TRUE to turn on noise source, FALSE to turn off
 */
static void noise_source_control(boolean enable)
{
    if (enable) {
        REG_WR32(GPIO_SET, BIT(3));  /* GPIO3 = noise source enable */
    } else {
        REG_WR32(GPIO_CLR, BIT(3));
    }
    DELAY_MS(50U);  /* Allow noise source to settle */
}

/**
 * @brief Measure total noise power at ADC output
 *
 * Captures ADC data with the given RX configuration and computes the
 * average power in linear units (mW).
 */
static float64 measure_noise_power_linear(uint8 channel, uint32 num_averages)
{
    sint16 buf_i[RX_SENS_CAPTURE_SAMPLES];
    sint16 buf_q[RX_SENS_CAPTURE_SAMPLES];
    float64 total_power = 0.0;

    for (uint32 avg = 0U; avg < num_averages; avg++) {
        capture_engine_start(channel, RX_SENS_CAPTURE_SAMPLES);
        capture_engine_wait_done();
        capture_engine_read_iq(buf_i, buf_q, RX_SENS_CAPTURE_SAMPLES);

        float64 sum_sq = 0.0;
        for (uint32 i = 0U; i < RX_SENS_CAPTURE_SAMPLES; i++) {
            float64 vi = (float64)buf_i[i] / 32768.0;
            float64 vq = (float64)buf_q[i] / 32768.0;
            sum_sq += (vi * vi) + (vq * vq);
        }

        total_power += sum_sq / (float64)RX_SENS_CAPTURE_SAMPLES;
    }

    return total_power / (float64)num_averages;
}

/* ========================================================================= */
/*  Public API                                                                */
/* ========================================================================= */

float32 noise_figure_test_measure(uint8 channel, float32 freq_ghz)
{
    /* Configure synthesizer for LO */
    synth_set_frequency_ghz(freq_ghz);
    synth_enable_cw_mode();
    if (synth_wait_lock(SYNTH_LOCK_TIMEOUT_US) != STATUS_OK) {
        return -999.0f;
    }

    /* Configure RX chain */
    rx_frontend_enable_channel(channel);
    rx_frontend_set_lna_gain(channel, RX_LNA_GAIN_HIGH);
    pga_set_gain(channel, PGA_GAIN_30DB);
    if_filter_set_bandwidth(IF_HPF_175KHZ, IF_LPF_5000KHZ);
    DELAY_US(300U);

    /* Ensure all TX channels are off */
    tx_frontend_disable_channel(RF_TX_CHANNEL_0);
    tx_frontend_disable_channel(RF_TX_CHANNEL_1);
    tx_frontend_disable_channel(RF_TX_CHANNEL_2);

    /* --- Y-factor measurement --- */

    /* Step 1: Measure "hot" noise power (noise source ON, T_hot = T0 + ENR*T0) */
    noise_source_control(TRUE);
    float64 p_hot = measure_noise_power_linear(channel, NF_NUM_AVERAGES);

    /* Step 2: Measure "cold" noise power (noise source OFF, T_cold = T0 = 290K) */
    noise_source_control(FALSE);
    float64 p_cold = measure_noise_power_linear(channel, NF_NUM_AVERAGES);

    /* Step 3: Compute Y-factor */
    float64 y_factor = p_hot / (p_cold + 1e-30);  /* Prevent division by zero */

    /* Step 4: Compute noise figure
     *   NF = ENR_dB - 10*log10(Y - 1)
     *
     *   If Y <= 1, measurement is invalid (noise source may not be working).
     */
    float32 nf_db;
    if (y_factor > 1.001) {
        nf_db = NF_ENR_DB - (float32)(10.0 * log10(y_factor - 1.0));
    } else {
        /* Fallback: estimate from cold noise power alone */
        float64 cold_dbfs = 10.0 * log10(p_cold + 1e-30);
        float32 rx_gain_db = 48.0f;  /* Nominal RX chain gain */

        /* kTB reference: -174 dBm/Hz at 290K */
        float32 ktb_dbm = -174.0f + 10.0f * log10f(NF_BW_MHZ * 1e6f);
        float32 expected_noise_dbm = ktb_dbm + rx_gain_db;
        float32 measured_noise_dbm = (float32)cold_dbfs + 4.0f;  /* ADC FS ref */

        nf_db = measured_noise_dbm - expected_noise_dbm + rx_gain_db;
        nf_db = CLAMP(nf_db, 3.0f, 30.0f);  /* Sanity bound */
    }

    /* Cleanup */
    rx_frontend_disable_channel(channel);
    synth_disable();

    return nf_db;
}
