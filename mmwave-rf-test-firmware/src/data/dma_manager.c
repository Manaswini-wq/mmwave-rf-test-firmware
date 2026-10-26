/**
 * @file dma_manager.c
 * @brief DMA manager for high-speed ADC data transfers
 *
 * Manages the on-chip DMA controller channels for transferring ADC
 * capture data from the ADC buffer memory to L3 SRAM processing buffers.
 * Supports ping-pong buffer configuration and per-channel timing offsets
 * for inter-channel calibration.
 */

#include "data/dma_manager.h"
#include "iwr6843_regs.h"
#include "rf_test_cfg.h"

#define DMA_MAX_CHANNELS  4U

static boolean g_dma_initialized = FALSE;

/* Per-channel timing offsets (picoseconds) for inter-channel calibration */
static sint32 g_timing_offsets[RF_NUM_RX_CHANNELS] = {0};

/* ========================================================================= */
/*  Internal helpers                                                          */
/* ========================================================================= */

static uint32 dma_ch_reg(uint8 dma_ch, uint32 base_offset)
{
    return DMA_CH0_SRC + (uint32)dma_ch * DMA_CH_STRIDE + base_offset - DMA_CH0_SRC + DMA_CH0_SRC;
}

/* ========================================================================= */
/*  Public API                                                                */
/* ========================================================================= */

Status_t dma_manager_init(void)
{
    /* Reset DMA controller */
    REG_WR32(DMA_CTRL, 0U);
    DELAY_US(5U);

    /* Clear all interrupt status */
    REG_WR32(DMA_INTR_CLEAR, 0xFFFFFFFFU);

    /* Enable DMA controller */
    REG_WR32(DMA_CTRL, DMA_CTRL_ENABLE);

    g_dma_initialized = TRUE;
    return STATUS_OK;
}

Status_t dma_manager_configure(uint8 dma_ch, uint32 src_addr, uint32 dst_addr,
                                uint32 size_bytes)
{
    if (!g_dma_initialized) return STATUS_NOT_INIT;
    if (dma_ch >= DMA_MAX_CHANNELS) return STATUS_INVALID_PARAM;

    uint32 base = DMA_CH0_SRC + (uint32)dma_ch * DMA_CH_STRIDE;

    /* Program source and destination addresses */
    REG_WR32(base + 0x00U, src_addr);    /* Source address */
    REG_WR32(base + 0x04U, dst_addr);    /* Destination address */
    REG_WR32(base + 0x08U, size_bytes);  /* Transfer size */

    /* Configure: interrupt enable, single-shot mode */
    uint32 ctrl = DMA_CH_CTRL_INTR_EN;
    REG_WR32(base + 0x0CU, ctrl);

    return STATUS_OK;
}

Status_t dma_manager_start(uint8 dma_ch)
{
    if (!g_dma_initialized) return STATUS_NOT_INIT;
    if (dma_ch >= DMA_MAX_CHANNELS) return STATUS_INVALID_PARAM;

    uint32 ctrl_reg = DMA_CH0_CTRL + (uint32)dma_ch * DMA_CH_STRIDE;
    uint32 ctrl = REG_RD32(ctrl_reg);
    ctrl |= DMA_CH_CTRL_START;
    REG_WR32(ctrl_reg, ctrl);

    return STATUS_OK;
}

Status_t dma_manager_stop(uint8 dma_ch)
{
    if (dma_ch >= DMA_MAX_CHANNELS) return STATUS_INVALID_PARAM;

    uint32 ctrl_reg = DMA_CH0_CTRL + (uint32)dma_ch * DMA_CH_STRIDE;
    REG_WR32(ctrl_reg, 0U);

    return STATUS_OK;
}

Status_t dma_manager_wait_done(uint8 dma_ch, uint32 timeout_us)
{
    if (dma_ch >= DMA_MAX_CHANNELS) return STATUS_INVALID_PARAM;

    uint32 ctrl_reg = DMA_CH0_CTRL + (uint32)dma_ch * DMA_CH_STRIDE;
    uint32 elapsed = 0U;

    while (elapsed < timeout_us) {
        if (BIT_TST(REG_RD32(ctrl_reg), DMA_CH_CTRL_DONE)) {
            /* Clear interrupt */
            REG_WR32(DMA_INTR_CLEAR, BIT(dma_ch));
            return STATUS_OK;
        }
        DELAY_US(1U);
        elapsed++;
    }

    return STATUS_TIMEOUT;
}

boolean dma_manager_is_done(uint8 dma_ch)
{
    if (dma_ch >= DMA_MAX_CHANNELS) return TRUE;
    uint32 ctrl_reg = DMA_CH0_CTRL + (uint32)dma_ch * DMA_CH_STRIDE;
    return BIT_TST(REG_RD32(ctrl_reg), DMA_CH_CTRL_DONE);
}

void dma_manager_set_timing_offset(uint8 channel, sint32 offset_ps)
{
    if (channel < RF_NUM_RX_CHANNELS) {
        g_timing_offsets[channel] = offset_ps;
        /*
         * In a real implementation, this would program a fine-delay
         * register in the ADC sampling clock distribution to adjust
         * the sampling instant for inter-channel alignment.
         */
    }
}
