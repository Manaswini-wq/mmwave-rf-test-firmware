/**
 * @file tx_power_flatness_cal.c
 * @brief TX power flatness calibration algorithm
 *
 * Iteratively adjusts per-frequency TX power DAC codes to achieve a flat
 * power response across the operating bandwidth.  The algorithm:
 *   1. Measure TX power at each frequency point
 *   2. Compute the mean power and per-point deviation
 *   3. Adjust DAC codes to reduce deviation (proportional correction)
 *   4. Repeat until ripple < target threshold or max iterations reached
 *   5. Store final correction coefficients
 */

#include "calibration/tx_power_flatness_cal.h"
#include "rf_tests/tx_power_test.h"
#include "drivers/tx_frontend.h"
#include "drivers/synth_driver.h"
#include "protocol/uart_cmd_handler.h"
#include "rf_test_cfg.h"
#include "test_limits.h"

#include <math.h>
#include <string.h>

/* ========================================================================= */
/*  Calibration state storage                                                 */
/* ========================================================================= */

/** Per-channel correction coefficients (DAC offset per frequency point) */
static sint16 g_cal_coeffs[RF_NUM_TX_CHANNELS][TX_FLAT_CAL_FREQ_POINTS];
static boolean g_cal_valid[RF_NUM_TX_CHANNELS] = {FALSE, FALSE, FALSE};

/** Temporary measurement buffers */
static float32 g_power_measured[TX_FLAT_CAL_FREQ_POINTS];
static sint16  g_dac_codes[TX_FLAT_CAL_FREQ_POINTS];

/* ========================================================================= */
/*  Internal helpers                                                          */
/* ========================================================================= */

/**
 * @brief Compute frequency for a given calibration point index
 */
static float32 cal_freq_for_index(uint32 idx)
{
    float32 step = (RF_FREQ_MAX_GHZ - RF_FREQ_MIN_GHZ) / (float32)(TX_FLAT_CAL_FREQ_POINTS - 1U);
    return RF_FREQ_MIN_GHZ + step * (float32)idx;
}

/**
 * @brief Measure TX power at all frequency points with current DAC settings
 */
static void measure_power_sweep(uint8 channel)
{
    for (uint32 i = 0U; i < TX_FLAT_CAL_FREQ_POINTS; i++) {
        float32 freq = cal_freq_for_index(i);

        /* Apply current DAC code for this frequency */
        tx_frontend_set_power(channel, (uint8)CLAMP(g_dac_codes[i], 0, TX_PWR_MAX_CODE));

        g_power_measured[i] = tx_power_test_measure(channel, freq);
    }
}

/**
 * @brief Compute peak-to-peak ripple and mean power from sweep data
 */
static float32 compute_ripple(float32 *mean_power)
{
    float32 min_pwr = 999.0f;
    float32 max_pwr = -999.0f;
    float64 sum = 0.0;

    for (uint32 i = 0U; i < TX_FLAT_CAL_FREQ_POINTS; i++) {
        if (g_power_measured[i] < min_pwr) min_pwr = g_power_measured[i];
        if (g_power_measured[i] > max_pwr) max_pwr = g_power_measured[i];
        sum += (float64)g_power_measured[i];
    }

    *mean_power = (float32)(sum / (float64)TX_FLAT_CAL_FREQ_POINTS);
    return (max_pwr - min_pwr);
}

/**
 * @brief Update DAC codes based on measured deviation from mean
 *
 * Applies proportional correction: if a frequency point is above the mean,
 * reduce its DAC code; if below, increase it.  The step size is proportional
 * to the error magnitude.
 */
static void update_dac_codes(float32 mean_power)
{
    for (uint32 i = 0U; i < TX_FLAT_CAL_FREQ_POINTS; i++) {
        float32 error_db = g_power_measured[i] - mean_power;

        /*
         * Approximate DAC code correction:
         *   ~0.5 dB per DAC code step (device-specific)
         */
        sint16 correction = (sint16)(error_db / 0.5f);

        g_dac_codes[i] -= correction;

        /* Clamp to valid range */
        if (g_dac_codes[i] < 0)    g_dac_codes[i] = 0;
        if (g_dac_codes[i] > (sint16)TX_PWR_MAX_CODE) g_dac_codes[i] = (sint16)TX_PWR_MAX_CODE;
    }
}

/* ========================================================================= */
/*  Public API                                                                */
/* ========================================================================= */

Status_t tx_power_flatness_cal_run(uint8 channel)
{
    if (channel >= RF_NUM_TX_CHANNELS) {
        return STATUS_INVALID_PARAM;
    }

    /* Initialize DAC codes to nominal value */
    for (uint32 i = 0U; i < TX_FLAT_CAL_FREQ_POINTS; i++) {
        g_dac_codes[i] = (sint16)TX_PWR_DEFAULT_CODE;
    }

    uart_cmd_send_string("TX Flatness Cal: starting\r\n");

    float32 ripple = 999.0f;
    float32 mean_power = 0.0f;
    uint32 iter;

    for (iter = 0U; iter < CAL_MAX_ITERATIONS; iter++) {
        /* Measure power at all frequency points */
        measure_power_sweep(channel);

        /* Compute ripple */
        ripple = compute_ripple(&mean_power);

        /* Report iteration progress */
        char buf[64];
        /* Simple integer-based reporting to avoid sprintf float */
        uart_cmd_send_string("  Iter ");
        buf[0] = '0' + (char)(iter + 1U);
        buf[1] = '\0';
        uart_cmd_send_string(buf);
        uart_cmd_send_string(": ripple = ");
        /* Convert ripple to integer tenths for display */
        sint32 ripple_tenths = (sint32)(ripple * 10.0f + 0.5f);
        buf[0] = '0' + (char)(ripple_tenths / 10);
        buf[1] = '.';
        buf[2] = '0' + (char)(ripple_tenths % 10);
        buf[3] = '\0';
        uart_cmd_send_string(buf);
        uart_cmd_send_string(" dB\r\n");

        /* Check convergence */
        if (ripple <= TX_FLAT_CAL_RIPPLE_TARGET) {
            break;
        }

        /* Update DAC codes for next iteration */
        update_dac_codes(mean_power);
    }

    /* Store calibration results */
    if (ripple <= TX_FLAT_CAL_RIPPLE_TARGET) {
        memcpy(g_cal_coeffs[channel], g_dac_codes,
               TX_FLAT_CAL_FREQ_POINTS * sizeof(sint16));
        g_cal_valid[channel] = TRUE;
        uart_cmd_send_string("  Result: PASS\r\n");
        return STATUS_OK;
    } else {
        g_cal_valid[channel] = FALSE;
        uart_cmd_send_string("  Result: FAIL (did not converge)\r\n");
        return STATUS_CAL_FAIL;
    }
}

Status_t tx_power_flatness_cal_get_coeffs(uint8 channel, sint16 *coeffs, uint32 num_pts)
{
    if (channel >= RF_NUM_TX_CHANNELS || coeffs == NULL_PTR) {
        return STATUS_INVALID_PARAM;
    }
    if (!g_cal_valid[channel]) {
        return STATUS_NOT_READY;
    }

    uint32 copy_len = MIN(num_pts, TX_FLAT_CAL_FREQ_POINTS);
    memcpy(coeffs, g_cal_coeffs[channel], copy_len * sizeof(sint16));

    return STATUS_OK;
}
