/**
 * @file tx_frontend.c
 * @brief TX frontend driver for IWR6843
 *
 * Controls the transmit signal chain including the power amplifier (PA),
 * per-channel phase shifters, internal loopback path, and CW test tone
 * generation.  Each of the 3 TX channels has independent power and
 * phase control registers.
 */

#include "drivers/tx_frontend.h"
#include "iwr6843_regs.h"
#include "rf_test_cfg.h"

static boolean g_tx_initialized = FALSE;

/* Per-channel power and phase register addresses */
static const uint32 g_tx_power_regs[RF_NUM_TX_CHANNELS] = {
    TX_CH0_POWER, TX_CH1_POWER, TX_CH2_POWER
};
static const uint32 g_tx_phase_regs[RF_NUM_TX_CHANNELS] = {
    TX_CH0_PHASE, TX_CH1_PHASE, TX_CH2_PHASE
};
static const uint32 g_tx_ch_enable_bits[RF_NUM_TX_CHANNELS] = {
    TX_CTRL_CH0_EN, TX_CTRL_CH1_EN, TX_CTRL_CH2_EN
};

/* ========================================================================= */
/*  Public API                                                                */
/* ========================================================================= */

Status_t tx_frontend_init(void)
{
    /* Disable all TX channels */
    REG_WR32(TX_CTRL, 0U);

    /* Set default PA bias for class-AB operation */
    REG_WR32(TX_PA_BIAS, 0x0CU);

    /* Set default power and phase on all channels */
    for (uint8 ch = 0U; ch < RF_NUM_TX_CHANNELS; ch++) {
        REG_WR32(g_tx_power_regs[ch], TX_PWR_DEFAULT_CODE);
        REG_WR32(g_tx_phase_regs[ch], 0U);  /* 0 degrees */
    }

    /* Disable loopback */
    REG_WR32(TX_LB_CTRL, 0U);
    REG_WR32(TX_LB_ATTEN, 0x1EU);  /* ~30 dB loopback attenuation */

    g_tx_initialized = TRUE;
    return STATUS_OK;
}

void tx_frontend_enable_channel(uint8 channel)
{
    if (channel >= RF_NUM_TX_CHANNELS) return;

    uint32 ctrl = REG_RD32(TX_CTRL);
    ctrl |= g_tx_ch_enable_bits[channel];
    REG_WR32(TX_CTRL, ctrl);

    /* Enable PA for this channel */
    uint32 pa_en = REG_RD32(TX_PA_EN);
    pa_en |= BIT(channel);
    REG_WR32(TX_PA_EN, pa_en);
}

void tx_frontend_disable_channel(uint8 channel)
{
    if (channel >= RF_NUM_TX_CHANNELS) return;

    uint32 ctrl = REG_RD32(TX_CTRL);
    ctrl &= ~g_tx_ch_enable_bits[channel];
    REG_WR32(TX_CTRL, ctrl);

    uint32 pa_en = REG_RD32(TX_PA_EN);
    pa_en &= ~BIT(channel);
    REG_WR32(TX_PA_EN, pa_en);
}

Status_t tx_frontend_set_power(uint8 channel, uint8 power_code)
{
    if (!g_tx_initialized) return STATUS_NOT_INIT;
    if (channel >= RF_NUM_TX_CHANNELS) return STATUS_INVALID_PARAM;
    if (power_code > TX_PWR_MAX_CODE) return STATUS_INVALID_PARAM;

    uint32 reg_val = ((uint32)power_code & TX_POWER_CODE_MASK) << TX_POWER_CODE_SHIFT;
    REG_WR32(g_tx_power_regs[channel], reg_val);

    return STATUS_OK;
}

Status_t tx_frontend_set_phase(uint8 channel, uint16 phase_code)
{
    if (!g_tx_initialized) return STATUS_NOT_INIT;
    if (channel >= RF_NUM_TX_CHANNELS) return STATUS_INVALID_PARAM;

    /* Phase code: 0-4095 maps to 0-360 degrees (12-bit resolution) */
    REG_WR32(g_tx_phase_regs[channel], (uint32)(phase_code & 0x0FFFU));

    return STATUS_OK;
}

void tx_frontend_enable_tone(void)
{
    uint32 ctrl = REG_RD32(TX_CTRL);
    ctrl |= TX_CTRL_TONE_EN;
    REG_WR32(TX_CTRL, ctrl);
}

void tx_frontend_disable_tone(void)
{
    uint32 ctrl = REG_RD32(TX_CTRL);
    ctrl &= ~TX_CTRL_TONE_EN;
    REG_WR32(TX_CTRL, ctrl);
}

void tx_frontend_enable_loopback(uint8 channel)
{
    if (channel >= RF_NUM_TX_CHANNELS) return;

    /* Enable loopback path from TX channel to RX */
    uint32 lb = BIT(channel) | TX_CTRL_LOOPBACK_EN;
    REG_WR32(TX_LB_CTRL, lb);
}

void tx_frontend_disable_loopback(uint8 channel)
{
    (void)channel;
    REG_WR32(TX_LB_CTRL, 0U);
}
