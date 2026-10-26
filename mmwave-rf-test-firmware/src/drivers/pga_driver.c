/**
 * @file pga_driver.c
 * @brief Programmable Gain Amplifier (PGA) driver
 *
 * Controls the per-channel PGA in the receive IF chain.  Each RX channel
 * has an independent PGA with selectable gain (24-48 dB in 6 dB steps)
 * and a programmable DC offset correction DAC.
 */

#include "drivers/pga_driver.h"
#include "iwr6843_regs.h"
#include "rf_test_cfg.h"

/* ========================================================================= */
/*  Internal state                                                            */
/* ========================================================================= */
static uint8 g_pga_gain[RF_NUM_RX_CHANNELS] = {PGA_GAIN_30DB, PGA_GAIN_30DB,
                                                  PGA_GAIN_30DB, PGA_GAIN_30DB};
static boolean g_pga_initialized = FALSE;

/* Gain register addresses per channel */
static const uint32 g_gain_regs[RF_NUM_RX_CHANNELS] = {
    PGA_CH0_GAIN, PGA_CH1_GAIN, PGA_CH2_GAIN, PGA_CH3_GAIN
};

static const uint32 g_offset_regs[RF_NUM_RX_CHANNELS] = {
    PGA_OFFSET_CH0, PGA_OFFSET_CH1, PGA_OFFSET_CH2, PGA_OFFSET_CH3
};

/* ========================================================================= */
/*  Public API                                                                */
/* ========================================================================= */

Status_t pga_init(void)
{
    /* Enable PGA block */
    REG_WR32(PGA_CTRL, PGA_CTRL_ENABLE);
    DELAY_US(5U);

    /* Set default bias */
    REG_WR32(PGA_BIAS_CTRL, 0x04U);  /* Mid-range bias current */

    /* Set default gain and zero offset on all channels */
    for (uint8 ch = 0U; ch < RF_NUM_RX_CHANNELS; ch++) {
        REG_WR32(g_gain_regs[ch], PGA_GAIN_30DB & PGA_GAIN_MASK);
        REG_WR32(g_offset_regs[ch], 0U);
        g_pga_gain[ch] = PGA_GAIN_30DB;
    }

    g_pga_initialized = TRUE;
    return STATUS_OK;
}

Status_t pga_set_gain(uint8 channel, uint8 gain_code)
{
    if (!g_pga_initialized) return STATUS_NOT_INIT;
    if (channel >= RF_NUM_RX_CHANNELS) return STATUS_INVALID_PARAM;
    if (gain_code > PGA_GAIN_48DB) return STATUS_INVALID_PARAM;

    REG_WR32(g_gain_regs[channel], (uint32)(gain_code & PGA_GAIN_MASK));
    g_pga_gain[channel] = gain_code;

    return STATUS_OK;
}

uint8 pga_get_gain(uint8 channel)
{
    if (channel >= RF_NUM_RX_CHANNELS) return 0U;
    return g_pga_gain[channel];
}

Status_t pga_set_offset(uint8 channel, sint16 offset)
{
    if (!g_pga_initialized) return STATUS_NOT_INIT;
    if (channel >= RF_NUM_RX_CHANNELS) return STATUS_INVALID_PARAM;

    REG_WR32(g_offset_regs[channel], (uint32)(offset & 0xFFFF));
    return STATUS_OK;
}

void pga_enable(void)
{
    BIT_SET(*(vuint32 *)PGA_CTRL, PGA_CTRL_ENABLE);
}

void pga_disable(void)
{
    BIT_CLR(*(vuint32 *)PGA_CTRL, PGA_CTRL_ENABLE);
}
