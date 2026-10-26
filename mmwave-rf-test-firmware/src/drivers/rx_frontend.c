/**
 * @file rx_frontend.c
 * @brief RX frontend driver for IWR6843
 *
 * Controls the receive signal chain: LNA, down-conversion mixer, HPF/LPF
 * (via IF filter driver), and per-channel phase offset for beamforming
 * calibration.  Each of the 4 RX channels has independent LNA gain,
 * DC offset, and phase-offset registers.
 */

#include "drivers/rx_frontend.h"
#include "iwr6843_regs.h"
#include "rf_test_cfg.h"

static boolean g_rx_initialized = FALSE;

/* Per-channel gain register addresses */
static const uint32 g_rx_gain_regs[RF_NUM_RX_CHANNELS] = {
    RX_CH0_GAIN, RX_CH1_GAIN, RX_CH2_GAIN, RX_CH3_GAIN
};

static const uint32 g_rx_ch_enable_bits[RF_NUM_RX_CHANNELS] = {
    RX_CTRL_CH0_EN, RX_CTRL_CH1_EN, RX_CTRL_CH2_EN, RX_CTRL_CH3_EN
};

/* ========================================================================= */
/*  Public API                                                                */
/* ========================================================================= */

Status_t rx_frontend_init(void)
{
    /* Disable all RX channels */
    REG_WR32(RX_CTRL, 0U);

    /* Configure LNA for all channels to default gain */
    for (uint8 ch = 0U; ch < RF_NUM_RX_CHANNELS; ch++) {
        REG_WR32(g_rx_gain_regs[ch], RX_LNA_GAIN_HIGH);
    }

    /* Configure LNA bias */
    REG_WR32(RX_LNA_CTRL, 0x0AU);  /* Optimized for NF at 60 GHz */

    /* Configure mixer */
    REG_WR32(RX_MIXER_CTRL, BIT(0));  /* Enable mixer with default bias */

    /* Zero DC offsets */
    for (uint8 ch = 0U; ch < RF_NUM_RX_CHANNELS; ch++) {
        uint32 i_reg = RX_DC_OFFSET_I + (uint32)ch * 0x08U;
        uint32 q_reg = RX_DC_OFFSET_Q + (uint32)ch * 0x08U;
        REG_WR32(i_reg, 0U);
        REG_WR32(q_reg, 0U);
    }

    /* Clear IQ mismatch correction */
    REG_WR32(RX_IQ_MISMATCH, 0U);

    g_rx_initialized = TRUE;
    return STATUS_OK;
}

void rx_frontend_enable_channel(uint8 channel)
{
    if (channel >= RF_NUM_RX_CHANNELS) return;

    uint32 ctrl = REG_RD32(RX_CTRL);
    ctrl |= g_rx_ch_enable_bits[channel];
    ctrl |= RX_CTRL_MIXER_EN;
    REG_WR32(RX_CTRL, ctrl);
}

void rx_frontend_disable_channel(uint8 channel)
{
    if (channel >= RF_NUM_RX_CHANNELS) return;

    uint32 ctrl = REG_RD32(RX_CTRL);
    ctrl &= ~g_rx_ch_enable_bits[channel];

    /* Disable mixer if no channels enabled */
    if ((ctrl & RX_CTRL_ALL_CH_EN) == 0U) {
        ctrl &= ~RX_CTRL_MIXER_EN;
    }

    REG_WR32(RX_CTRL, ctrl);
}

Status_t rx_frontend_set_lna_gain(uint8 channel, uint8 gain_code)
{
    if (!g_rx_initialized) return STATUS_NOT_INIT;
    if (channel >= RF_NUM_RX_CHANNELS) return STATUS_INVALID_PARAM;
    if (gain_code > RX_LNA_GAIN_MAX) return STATUS_INVALID_PARAM;

    REG_WR32(g_rx_gain_regs[channel], (uint32)gain_code);
    return STATUS_OK;
}

Status_t rx_frontend_set_phase_offset(uint8 channel, sint16 phase_tenths_deg)
{
    if (!g_rx_initialized) return STATUS_NOT_INIT;
    if (channel >= RF_NUM_RX_CHANNELS) return STATUS_INVALID_PARAM;

    /*
     * Phase offset register: signed 16-bit value representing
     * phase in tenths of degrees (-1800 to +1800 => -180.0 to +180.0 deg).
     * The hardware applies this as a digital rotation after mixing.
     */
    uint32 iq_reg = RX_IQ_MISMATCH + (uint32)channel * 4U;
    REG_WR32(iq_reg, (uint32)(phase_tenths_deg & 0xFFFF));

    return STATUS_OK;
}

void rx_frontend_enable_mixer(void)
{
    uint32 ctrl = REG_RD32(RX_CTRL);
    ctrl |= RX_CTRL_MIXER_EN;
    REG_WR32(RX_CTRL, ctrl);
}

void rx_frontend_disable_mixer(void)
{
    uint32 ctrl = REG_RD32(RX_CTRL);
    ctrl &= ~RX_CTRL_MIXER_EN;
    REG_WR32(RX_CTRL, ctrl);
}
