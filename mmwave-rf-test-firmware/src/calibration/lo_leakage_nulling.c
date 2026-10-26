/**
 * @file lo_leakage_nulling.c
 * @brief LO leakage nulling calibration algorithm
 *
 * LO leakage appears as a DC offset in the baseband I/Q signal due to
 * imperfect isolation between the LO and mixer ports.  This calibration
 * iteratively adjusts the I and Q DC offset DACs to minimize the DC
 * component (LO feedthrough) in the captured ADC data.
 *
 * Algorithm (successive approximation):
 *   1. Set initial search range for I and Q offset DACs
 *   2. Binary search: try center +/- step, keep value with lower DC power
 *   3. Halve step and repeat until convergence
 *   4. Fine-tune with gradient descent around the optimum
 *   5. Verify final LO leakage level
 */

#include "calibration/lo_leakage_nulling.h"
#include "drivers/rx_frontend.h"
#include "drivers/synth_driver.h"
#include "drivers/pga_driver.h"
#include "drivers/adc_driver.h"
#include "data/capture_engine.h"
#include "protocol/uart_cmd_handler.h"
#include "rf_test_cfg.h"
#include "iwr6843_regs.h"

#include <math.h>

/* ========================================================================= */
/*  Calibration state                                                         */
/* ========================================================================= */
#define LO_CAL_SAMPLES  512U

typedef struct {
    sint16  i_offset;       /**< Optimal I DC offset DAC code */
    sint16  q_offset;       /**< Optimal Q DC offset DAC code */
    float32 leakage_dbc;    /**< Measured LO leakage in dBc */
    boolean valid;
} LoCalState_t;

static LoCalState_t g_lo_cal[RF_NUM_RX_CHANNELS];

/* ========================================================================= */
/*  Internal helpers                                                          */
/* ========================================================================= */

/**
 * @brief Measure DC component power relative to total signal power
 *
 * The DC bin of the I/Q signal represents LO leakage.  We compute
 * the ratio of DC power to total RMS power and express it in dBc.
 */
static float32 measure_lo_leakage(uint8 channel)
{
    sint16 buf_i[LO_CAL_SAMPLES];
    sint16 buf_q[LO_CAL_SAMPLES];

    capture_engine_start(channel, LO_CAL_SAMPLES);
    capture_engine_wait_done();
    capture_engine_read_iq(buf_i, buf_q, LO_CAL_SAMPLES);

    /* Compute DC component (mean) */
    float64 dc_i = 0.0, dc_q = 0.0;
    float64 total_power = 0.0;

    for (uint32 k = 0U; k < LO_CAL_SAMPLES; k++) {
        float64 vi = (float64)buf_i[k];
        float64 vq = (float64)buf_q[k];
        dc_i += vi;
        dc_q += vq;
        total_power += vi * vi + vq * vq;
    }

    dc_i /= (float64)LO_CAL_SAMPLES;
    dc_q /= (float64)LO_CAL_SAMPLES;
    total_power /= (float64)LO_CAL_SAMPLES;

    float64 dc_power = dc_i * dc_i + dc_q * dc_q;

    /* dBc = 10*log10(dc_power / total_power) */
    float32 leakage_dbc = (float32)(10.0 * log10(dc_power / (total_power + 1e-20) + 1e-20));

    return leakage_dbc;
}

/**
 * @brief Apply I/Q DC offset values to the hardware registers
 */
static void apply_dc_offsets(uint8 channel, sint16 i_offset, sint16 q_offset)
{
    uint32 i_reg = RX_DC_OFFSET_I + (uint32)channel * 0x08U;
    uint32 q_reg = RX_DC_OFFSET_Q + (uint32)channel * 0x08U;
    REG_WR32(i_reg, (uint32)(i_offset & 0xFFFF));
    REG_WR32(q_reg, (uint32)(q_offset & 0xFFFF));
    DELAY_US(10U);
}

/* ========================================================================= */
/*  Public API                                                                */
/* ========================================================================= */

Status_t lo_leakage_nulling_run(uint8 channel)
{
    if (channel >= RF_NUM_RX_CHANNELS) {
        return STATUS_INVALID_PARAM;
    }

    /* Setup RX chain */
    synth_set_frequency_ghz(RF_FREQ_CENTER_GHZ);
    synth_enable_cw_mode();
    if (synth_wait_lock(SYNTH_LOCK_TIMEOUT_US) != STATUS_OK) {
        return STATUS_HW_FAULT;
    }
    rx_frontend_enable_channel(channel);
    pga_set_gain(channel, PGA_GAIN_30DB);
    DELAY_US(200U);

    uart_cmd_send_string("LO Nulling Cal: channel ");
    char ch_str = '0' + (char)channel;
    uart_cmd_send_string(&ch_str);
    uart_cmd_send_string("\r\n");

    /* Phase 1: Binary search on I offset */
    sint16 best_i = 0, best_q = 0;
    float32 best_leakage = 0.0f;

    /* Coarse search: I axis */
    sint16 step = (sint16)LO_NULL_INITIAL_RANGE;
    sint16 current_i = 0;
    sint16 current_q = 0;

    while (step >= 1) {
        float32 leak_neg, leak_pos;

        /* Try negative step */
        apply_dc_offsets(channel, current_i - step, current_q);
        leak_neg = measure_lo_leakage(channel);

        /* Try positive step */
        apply_dc_offsets(channel, current_i + step, current_q);
        leak_pos = measure_lo_leakage(channel);

        /* Keep the better one */
        if (leak_neg < leak_pos) {
            current_i -= step;
        } else {
            current_i += step;
        }

        step >>= 1;  /* Halve the step */
    }
    best_i = current_i;

    /* Coarse search: Q axis */
    step = (sint16)LO_NULL_INITIAL_RANGE;
    while (step >= 1) {
        float32 leak_neg, leak_pos;

        apply_dc_offsets(channel, best_i, current_q - step);
        leak_neg = measure_lo_leakage(channel);

        apply_dc_offsets(channel, best_i, current_q + step);
        leak_pos = measure_lo_leakage(channel);

        if (leak_neg < leak_pos) {
            current_q -= step;
        } else {
            current_q += step;
        }

        step >>= 1;
    }
    best_q = current_q;

    /* Phase 2: Fine gradient descent around the optimum */
    for (uint32 fine = 0U; fine < 10U; fine++) {
        apply_dc_offsets(channel, best_i, best_q);
        best_leakage = measure_lo_leakage(channel);

        /* Try all 4 neighbors */
        sint16 candidates_i[] = {(sint16)(best_i - 1), (sint16)(best_i + 1), best_i, best_i};
        sint16 candidates_q[] = {best_q, best_q, (sint16)(best_q - 1), (sint16)(best_q + 1)};
        boolean improved = FALSE;

        for (uint32 c = 0U; c < 4U; c++) {
            apply_dc_offsets(channel, candidates_i[c], candidates_q[c]);
            float32 leak = measure_lo_leakage(channel);
            if (leak < best_leakage) {
                best_leakage = leak;
                best_i = candidates_i[c];
                best_q = candidates_q[c];
                improved = TRUE;
            }
        }

        if (!improved) break;
    }

    /* Apply final optimal offsets */
    apply_dc_offsets(channel, best_i, best_q);

    /* Store results */
    g_lo_cal[channel].i_offset = best_i;
    g_lo_cal[channel].q_offset = best_q;
    g_lo_cal[channel].leakage_dbc = best_leakage;
    g_lo_cal[channel].valid = TRUE;

    /* Cleanup */
    rx_frontend_disable_channel(channel);
    synth_disable();

    if (best_leakage <= LO_NULL_TARGET_DBC) {
        uart_cmd_send_string("  Result: PASS\r\n");
        return STATUS_OK;
    } else {
        uart_cmd_send_string("  Result: FAIL\r\n");
        return STATUS_CAL_FAIL;
    }
}

float32 lo_leakage_nulling_get_level(uint8 channel)
{
    if (channel >= RF_NUM_RX_CHANNELS || !g_lo_cal[channel].valid) {
        return 0.0f;
    }
    return g_lo_cal[channel].leakage_dbc;
}
