/**
 * @file phase_noise_test.c
 * @brief Phase noise measurement at various offset frequencies
 *
 * Measures single-sideband phase noise L(f) by capturing the CW tone via
 * internal loopback, computing an FFT-based power spectral density (PSD),
 * and reading the PSD at the requested offset relative to the carrier power.
 * A Hanning window is applied before the FFT to reduce spectral leakage.
 */

#include "rf_tests/phase_noise_test.h"
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
/*  FFT helpers (simplified radix-2 DIT for power-of-2 sizes)                */
/* ========================================================================= */

static void apply_hanning_window(float32 *data, uint32 n)
{
    for (uint32 i = 0U; i < n; i++) {
        float32 w = 0.5f * (1.0f - cosf(2.0f * (float32)M_PI * (float32)i / (float32)(n - 1U)));
        data[i] *= w;
    }
}

/**
 * @brief Bit-reversal permutation for in-place FFT
 */
static void bit_reverse(float32 *re, float32 *im, uint32 n)
{
    uint32 j = 0U;
    for (uint32 i = 0U; i < n; i++) {
        if (i < j) {
            float32 tmp;
            tmp = re[i]; re[i] = re[j]; re[j] = tmp;
            tmp = im[i]; im[i] = im[j]; im[j] = tmp;
        }
        uint32 m = n >> 1;
        while (m >= 1U && j >= m) {
            j -= m;
            m >>= 1;
        }
        j += m;
    }
}

/**
 * @brief Simple radix-2 DIT FFT (in-place)
 */
static void fft_radix2(float32 *re, float32 *im, uint32 n)
{
    bit_reverse(re, im, n);

    for (uint32 len = 2U; len <= n; len <<= 1) {
        float32 angle = -2.0f * (float32)M_PI / (float32)len;
        float32 wre = cosf(angle);
        float32 wim = sinf(angle);

        for (uint32 i = 0U; i < n; i += len) {
            float32 cur_re = 1.0f, cur_im = 0.0f;
            for (uint32 j = 0U; j < len / 2U; j++) {
                uint32 u_idx = i + j;
                uint32 v_idx = i + j + len / 2U;
                float32 tre = cur_re * re[v_idx] - cur_im * im[v_idx];
                float32 tim = cur_re * im[v_idx] + cur_im * re[v_idx];
                re[v_idx] = re[u_idx] - tre;
                im[v_idx] = im[u_idx] - tim;
                re[u_idx] += tre;
                im[u_idx] += tim;
                float32 new_re = cur_re * wre - cur_im * wim;
                float32 new_im = cur_re * wim + cur_im * wre;
                cur_re = new_re;
                cur_im = new_im;
            }
        }
    }
}

/**
 * @brief Compute power spectral density from FFT output
 * @return PSD array in dB (relative to carrier)
 */
static void compute_psd(const float32 *fft_re, const float32 *fft_im,
                         float32 *psd_db, uint32 n, float32 carrier_power_lin)
{
    for (uint32 i = 0U; i < n / 2U; i++) {
        float32 mag_sq = fft_re[i] * fft_re[i] + fft_im[i] * fft_im[i];
        /* Normalize by N^2 for FFT scaling, then by carrier power */
        float32 psd_lin = mag_sq / ((float32)n * (float32)n) / (carrier_power_lin + 1e-20f);
        psd_db[i] = 10.0f * log10f(psd_lin + 1e-20f);
    }
}

/* ========================================================================= */
/*  Public API                                                                */
/* ========================================================================= */

float32 phase_noise_test_measure(uint8 channel, float32 freq_ghz, uint32 offset_hz)
{
    static float32 fft_re[PN_FFT_SIZE];
    static float32 fft_im[PN_FFT_SIZE];
    static float32 psd_db[PN_FFT_SIZE / 2U];
    sint16 buf_i[PN_FFT_SIZE];
    sint16 buf_q[PN_FFT_SIZE];

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
    DELAY_US(200U);

    /* Averaged PSD measurement */
    float64 avg_psd_at_offset = 0.0;

    for (uint32 avg = 0U; avg < PN_NUM_AVERAGES; avg++) {
        /* Capture I/Q data */
        capture_engine_start(channel % RF_NUM_RX_CHANNELS, PN_FFT_SIZE);
        capture_engine_wait_done();
        capture_engine_read_iq(buf_i, buf_q, PN_FFT_SIZE);

        /* Convert to float and apply window */
        for (uint32 i = 0U; i < PN_FFT_SIZE; i++) {
            fft_re[i] = (float32)buf_i[i] / 32768.0f;
            fft_im[i] = (float32)buf_q[i] / 32768.0f;
        }
        apply_hanning_window(fft_re, PN_FFT_SIZE);
        apply_hanning_window(fft_im, PN_FFT_SIZE);

        /* Compute FFT */
        fft_radix2(fft_re, fft_im, PN_FFT_SIZE);

        /* Find carrier bin (DC or strongest bin) */
        float32 carrier_power = fft_re[0] * fft_re[0] + fft_im[0] * fft_im[0];
        for (uint32 i = 1U; i < PN_FFT_SIZE / 2U; i++) {
            float32 p = fft_re[i] * fft_re[i] + fft_im[i] * fft_im[i];
            if (p > carrier_power) carrier_power = p;
        }
        carrier_power /= ((float32)PN_FFT_SIZE * (float32)PN_FFT_SIZE);

        compute_psd(fft_re, fft_im, psd_db, PN_FFT_SIZE, carrier_power);

        /* Find bin corresponding to offset frequency */
        float32 bin_spacing = (float32)ADC_DEFAULT_SAMPLE_RATE / (float32)PN_FFT_SIZE;
        uint32 offset_bin = (uint32)((float32)offset_hz / bin_spacing + 0.5f);
        if (offset_bin >= PN_FFT_SIZE / 2U) offset_bin = PN_FFT_SIZE / 2U - 1U;

        /* Normalize to 1 Hz bandwidth: subtract 10*log10(bin_spacing) */
        float32 pn_at_offset = psd_db[offset_bin] - 10.0f * log10f(bin_spacing);
        avg_psd_at_offset += (float64)pn_at_offset;
    }

    /* Cleanup */
    tx_frontend_disable_tone();
    tx_frontend_disable_loopback(channel);
    tx_frontend_disable_channel(channel);
    rx_frontend_disable_channel(channel % RF_NUM_RX_CHANNELS);
    synth_disable();

    return (float32)(avg_psd_at_offset / (float64)PN_NUM_AVERAGES);
}

Status_t phase_noise_test_standard(uint8 channel, float32 freq_ghz, float32 results[3])
{
    results[0] = phase_noise_test_measure(channel, freq_ghz, PN_OFFSET_100KHZ);
    results[1] = phase_noise_test_measure(channel, freq_ghz, PN_OFFSET_1MHZ);
    results[2] = phase_noise_test_measure(channel, freq_ghz, PN_OFFSET_10MHZ);

    if (results[0] < -900.0f || results[1] < -900.0f || results[2] < -900.0f) {
        return STATUS_HW_FAULT;
    }
    return STATUS_OK;
}
