/**
 * @file freq_accuracy_test.c
 * @brief Frequency accuracy measurement
 *
 * Measures the frequency error of the synthesizer by capturing a CW tone
 * via internal loopback, performing an FFT to locate the tone's spectral
 * peak, and computing the difference between the measured and expected
 * IF frequency.  Parabolic interpolation is used for sub-bin accuracy.
 */

#include "rf_tests/freq_accuracy_test.h"
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
 * @brief Find peak bin in magnitude spectrum using parabolic interpolation
 *
 * Given the magnitude array, locates the maximum bin and refines the
 * frequency estimate using the three-point parabolic interpolation:
 *   delta = 0.5 * (left - right) / (left - 2*center + right)
 *   refined_bin = peak_bin + delta
 */
static float64 find_peak_frequency(const float32 *mag_sq, uint32 n_bins,
                                    float64 bin_spacing_hz)
{
    /* Find coarse peak */
    uint32 peak_bin = 1U;
    float32 peak_val = mag_sq[1];

    for (uint32 i = 2U; i < n_bins - 1U; i++) {
        if (mag_sq[i] > peak_val) {
            peak_val = mag_sq[i];
            peak_bin = i;
        }
    }

    /* Parabolic interpolation for sub-bin accuracy */
    float64 left   = (float64)mag_sq[peak_bin - 1U];
    float64 center = (float64)mag_sq[peak_bin];
    float64 right  = (float64)mag_sq[peak_bin + 1U];

    /* Use log-magnitude for better parabolic fit */
    left   = 10.0 * log10(left + 1e-20);
    center = 10.0 * log10(center + 1e-20);
    right  = 10.0 * log10(right + 1e-20);

    float64 delta = 0.5 * (left - right) / (left - 2.0 * center + right + 1e-20);
    float64 refined_bin = (float64)peak_bin + delta;

    return refined_bin * bin_spacing_hz;
}

/**
 * @brief Simple magnitude-squared computation from I/Q
 */
static void compute_magnitude_sq(const float32 *re, const float32 *im,
                                  float32 *mag_sq, uint32 n)
{
    for (uint32 i = 0U; i < n; i++) {
        mag_sq[i] = re[i] * re[i] + im[i] * im[i];
    }
}

/* Minimal bit-reverse + radix-2 FFT (shared with phase_noise_test.c in production) */
static void bit_rev_fft(float32 *re, float32 *im, uint32 n)
{
    uint32 j = 0U;
    for (uint32 i = 0U; i < n; i++) {
        if (i < j) {
            float32 t;
            t = re[i]; re[i] = re[j]; re[j] = t;
            t = im[i]; im[i] = im[j]; im[j] = t;
        }
        uint32 m = n >> 1;
        while (m >= 1U && j >= m) { j -= m; m >>= 1; }
        j += m;
    }
    for (uint32 len = 2U; len <= n; len <<= 1) {
        float32 a = -2.0f * (float32)M_PI / (float32)len;
        float32 wr = cosf(a), wi = sinf(a);
        for (uint32 i = 0U; i < n; i += len) {
            float32 cr = 1.0f, ci = 0.0f;
            for (uint32 k = 0U; k < len / 2U; k++) {
                uint32 u = i + k, v = u + len / 2U;
                float32 tr = cr * re[v] - ci * im[v];
                float32 ti = cr * im[v] + ci * re[v];
                re[v] = re[u] - tr; im[v] = im[u] - ti;
                re[u] += tr; im[u] += ti;
                float32 nr = cr * wr - ci * wi;
                ci = cr * wi + ci * wr;
                cr = nr;
            }
        }
    }
}

/* ========================================================================= */
/*  Public API                                                                */
/* ========================================================================= */

float32 freq_accuracy_test_measure(uint8 channel, float32 freq_ghz)
{
    static float32 fft_re[FREQ_ACC_CAPTURE_SAMPLES];
    static float32 fft_im[FREQ_ACC_CAPTURE_SAMPLES];
    static float32 mag_sq[FREQ_ACC_CAPTURE_SAMPLES / 2U];
    sint16 buf_i[FREQ_ACC_CAPTURE_SAMPLES];
    sint16 buf_q[FREQ_ACC_CAPTURE_SAMPLES];

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

    rx_frontend_enable_channel(channel % RF_NUM_RX_CHANNELS);
    pga_set_gain(channel % RF_NUM_RX_CHANNELS, PGA_GAIN_30DB);
    DELAY_US(300U);

    /* Averaged frequency measurement */
    float64 freq_error_sum = 0.0;

    for (uint32 avg = 0U; avg < FREQ_ACC_NUM_AVERAGES; avg++) {
        capture_engine_start(channel % RF_NUM_RX_CHANNELS, FREQ_ACC_CAPTURE_SAMPLES);
        capture_engine_wait_done();
        capture_engine_read_iq(buf_i, buf_q, FREQ_ACC_CAPTURE_SAMPLES);

        for (uint32 i = 0U; i < FREQ_ACC_CAPTURE_SAMPLES; i++) {
            fft_re[i] = (float32)buf_i[i] / 32768.0f;
            fft_im[i] = (float32)buf_q[i] / 32768.0f;
        }

        bit_rev_fft(fft_re, fft_im, FREQ_ACC_CAPTURE_SAMPLES);
        compute_magnitude_sq(fft_re, fft_im, mag_sq, FREQ_ACC_CAPTURE_SAMPLES / 2U);

        float64 bin_spacing = (float64)ADC_DEFAULT_SAMPLE_RATE / (float64)FREQ_ACC_CAPTURE_SAMPLES;
        float64 measured_if_hz = find_peak_frequency(mag_sq,
                                                      FREQ_ACC_CAPTURE_SAMPLES / 2U,
                                                      bin_spacing);

        /*
         * The expected IF frequency should be ~0 Hz for a perfect down-conversion.
         * Any offset represents a frequency error in the synthesizer.
         * Convert to kHz for reporting.
         */
        freq_error_sum += measured_if_hz / 1000.0;
    }

    /* Cleanup */
    tx_frontend_disable_tone();
    tx_frontend_disable_loopback(channel);
    tx_frontend_disable_channel(channel);
    rx_frontend_disable_channel(channel % RF_NUM_RX_CHANNELS);
    synth_disable();

    return (float32)(freq_error_sum / (float64)FREQ_ACC_NUM_AVERAGES);
}
