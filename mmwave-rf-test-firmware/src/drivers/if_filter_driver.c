/**
 * @file if_filter_driver.c
 * @brief IF bandpass filter driver for IWR6843
 *
 * Configures the on-chip intermediate-frequency (IF) bandpass filter
 * comprised of a high-pass filter (HPF) and low-pass filter (LPF).
 * The HPF rejects low-frequency signals from near objects; the LPF
 * sets the maximum IF bandwidth and anti-aliasing before ADC sampling.
 */

#include "drivers/if_filter_driver.h"
#include "iwr6843_regs.h"
#include "rf_test_cfg.h"

static boolean g_if_initialized = FALSE;

/* Look-up tables for corner frequencies (kHz) indexed by register code */
static const uint32 hpf_corners_khz[] = {175, 235, 350, 700};
static const uint32 lpf_corners_khz[] = {2500, 5000, 10000, 20000};

Status_t if_filter_init(void)
{
    /* Enable filter block */
    REG_WR32(IF_FILT_CTRL, IF_FILT_ENABLE);
    DELAY_US(5U);

    /* Default: HPF = 175 kHz, LPF = 5 MHz */
    REG_WR32(IF_FILT_HPF_CORNER, IF_HPF_175KHZ);
    REG_WR32(IF_FILT_LPF_CORNER, IF_LPF_5000KHZ);

    g_if_initialized = TRUE;
    return STATUS_OK;
}

Status_t if_filter_set_bandwidth(uint8 hpf_code, uint8 lpf_code)
{
    if (!g_if_initialized) return STATUS_NOT_INIT;
    if (hpf_code > IF_HPF_700KHZ || lpf_code > IF_LPF_20000KHZ) {
        return STATUS_INVALID_PARAM;
    }

    /* Verify HPF < LPF */
    if (hpf_corners_khz[hpf_code] >= lpf_corners_khz[lpf_code]) {
        return STATUS_INVALID_PARAM;
    }

    REG_WR32(IF_FILT_HPF_CORNER, (uint32)hpf_code);
    REG_WR32(IF_FILT_LPF_CORNER, (uint32)lpf_code);

    return STATUS_OK;
}

void if_filter_enable(void)
{
    REG_WR32(IF_FILT_CTRL, IF_FILT_ENABLE);
}

void if_filter_disable(void)
{
    REG_WR32(IF_FILT_CTRL, 0U);
}
