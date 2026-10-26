/**
 * @file capture_engine.c
 * @brief DMA-based high-speed data capture engine
 *
 * Manages the ADC data capture pipeline using DMA transfers in a
 * ping-pong buffer scheme.  The capture engine coordinates with the
 * ADC driver and DMA manager to perform seamless high-throughput
 * data acquisition from the IF ADC into L3 SRAM buffers.
 */

#include "data/capture_engine.h"
#include "data/dma_manager.h"
#include "drivers/adc_driver.h"
#include "iwr6843_regs.h"
#include "rf_test_cfg.h"

#include <string.h>

/* ========================================================================= */
/*  Internal state                                                            */
/* ========================================================================= */

/** Ping-pong capture buffers in L3 SRAM */
static sint16 g_capture_ping[ADC_BUFFER_SIZE_SAMPLES * 2U] SECTION(".l3.adc_capture_ping");
static sint16 g_capture_pong[ADC_BUFFER_SIZE_SAMPLES * 2U] SECTION(".l3.adc_capture_pong");

typedef enum {
    CAPTURE_IDLE,
    CAPTURE_PING,
    CAPTURE_PONG,
    CAPTURE_DONE
} CaptureState_t;

static CaptureState_t g_state = CAPTURE_IDLE;
static uint32 g_capture_samples = 0U;
static uint8  g_capture_channel = 0U;
static boolean g_initialized = FALSE;

/* ========================================================================= */
/*  Public API                                                                */
/* ========================================================================= */

Status_t capture_engine_init(void)
{
    memset(g_capture_ping, 0, sizeof(g_capture_ping));
    memset(g_capture_pong, 0, sizeof(g_capture_pong));

    g_state = CAPTURE_IDLE;
    g_capture_samples = 0U;
    g_initialized = TRUE;

    return STATUS_OK;
}

Status_t capture_engine_start(uint8 channel, uint32 num_samples)
{
    if (!g_initialized) return STATUS_NOT_INIT;
    if (num_samples > ADC_BUFFER_SIZE_SAMPLES) return STATUS_INVALID_PARAM;
    if (g_state != CAPTURE_IDLE && g_state != CAPTURE_DONE) {
        return STATUS_BUSY;
    }

    g_capture_channel = channel;
    g_capture_samples = num_samples;
    g_state = CAPTURE_PING;

    /* Configure DMA to transfer from ADC buffer to ping buffer
     * Each complex sample = 2 x sint16 (I, Q) = 4 bytes */
    uint32 transfer_bytes = num_samples * 4U;

    dma_manager_configure(0U,                     /* DMA channel 0 */
                           0x52000000U,            /* Source: ADC buffer */
                           (uint32)g_capture_ping, /* Dest: ping buffer */
                           transfer_bytes);
    dma_manager_start(0U);

    /* Start ADC capture */
    adc_start_capture(num_samples);

    return STATUS_OK;
}

Status_t capture_engine_wait_done(void)
{
    if (!g_initialized) return STATUS_NOT_INIT;

    /* Wait for ADC capture completion */
    Status_t status = adc_wait_done(100000U);  /* 100 ms timeout */
    if (status != STATUS_OK) return status;

    /* Wait for DMA transfer completion */
    status = dma_manager_wait_done(0U, 10000U);
    if (status != STATUS_OK) return status;

    g_state = CAPTURE_DONE;
    return STATUS_OK;
}

Status_t capture_engine_read_iq(sint16 *buf_i, sint16 *buf_q, uint32 num_samples)
{
    if (!g_initialized || g_state != CAPTURE_DONE) {
        return STATUS_NOT_READY;
    }
    if (buf_i == NULL_PTR || buf_q == NULL_PTR) {
        return STATUS_INVALID_PARAM;
    }

    uint32 read_count = MIN(num_samples, g_capture_samples);
    const sint16 *src = g_capture_ping;

    /* De-interleave I/Q samples
     * Buffer format: I0, Q0, I1, Q1, I2, Q2, ... */
    for (uint32 i = 0U; i < read_count; i++) {
        buf_i[i] = src[i * 2U];
        buf_q[i] = src[i * 2U + 1U];
    }

    g_state = CAPTURE_IDLE;
    return STATUS_OK;
}

boolean capture_engine_is_busy(void)
{
    return (g_state == CAPTURE_PING || g_state == CAPTURE_PONG);
}

void capture_engine_abort(void)
{
    adc_stop();
    dma_manager_stop(0U);
    g_state = CAPTURE_IDLE;
}
