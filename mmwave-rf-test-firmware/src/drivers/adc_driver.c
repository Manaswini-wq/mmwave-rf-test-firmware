/**
 * @file adc_driver.c
 * @brief ADC subsystem driver for IWR6843
 *
 * Configures and controls the high-speed sigma-delta ADC.  Supports
 * 12/14/16-bit resolution in real or complex (I/Q) output formats.
 * Data is typically transferred via DMA to L3 SRAM buffers.
 */

#include "drivers/adc_driver.h"
#include "iwr6843_regs.h"
#include "rf_test_cfg.h"

/* ========================================================================= */
/*  Internal state                                                            */
/* ========================================================================= */
static boolean g_adc_initialized = FALSE;
static uint32  g_sample_rate = 0U;
static uint8   g_resolution = ADC_RES_16BIT;
static uint8   g_format = ADC_FMT_COMPLEX;

/* ========================================================================= */
/*  Public API                                                                */
/* ========================================================================= */

Status_t adc_init(uint8 resolution, uint8 format, uint32 sample_rate)
{
    if (resolution > ADC_RES_16BIT || format > ADC_FMT_INTERLEAVED) {
        return STATUS_INVALID_PARAM;
    }
    if (sample_rate == 0U || sample_rate > 37500000U) {  /* Max 37.5 MSPS */
        return STATUS_INVALID_PARAM;
    }

    /* Reset ADC */
    REG_WR32(ADC_CTRL, 0U);
    DELAY_US(10U);

    /* Configure resolution and format */
    uint32 config = ((uint32)resolution << 0) | ((uint32)format << 4);
    REG_WR32(ADC_CONFIG, config);

    /* Configure sample rate divider
     * ADC clock = 200 MHz (system clock)
     * Divider = 200 MHz / sample_rate */
    uint32 divider = 200000000U / sample_rate;
    REG_WR32(ADC_SAMPLE_RATE, divider);

    /* Configure ADC buffer address (default to ADC_BUF memory) */
    REG_WR32(ADC_BUF_ADDR, 0x52000000U);
    REG_WR32(ADC_BUF_SIZE, ADC_BUFFER_SIZE_SAMPLES * 4U);  /* 4 bytes per complex sample */

    /* Enable DMA trigger */
    REG_WR32(ADC_INTR_CTRL, BIT(0));  /* Enable capture-done interrupt */

    g_sample_rate = sample_rate;
    g_resolution  = resolution;
    g_format      = format;
    g_adc_initialized = TRUE;

    return STATUS_OK;
}

Status_t adc_start_capture(uint32 num_samples)
{
    if (!g_adc_initialized) return STATUS_NOT_INIT;

    /* Set number of samples */
    REG_WR32(ADC_NUM_SAMPLES, num_samples);

    /* Clear status bits */
    REG_WR32(ADC_STATUS, 0U);

    /* Start capture: enable + start + DMA */
    uint32 ctrl = ADC_CTRL_ENABLE | ADC_CTRL_START | ADC_CTRL_DMA_EN;
    REG_WR32(ADC_CTRL, ctrl);

    return STATUS_OK;
}

Status_t adc_wait_done(uint32 timeout_us)
{
    uint32 elapsed = 0U;

    while (elapsed < timeout_us) {
        uint32 status = REG_RD32(ADC_STATUS);
        if (BIT_TST(status, ADC_STATUS_DONE)) {
            /* Check for overflow */
            if (BIT_TST(status, ADC_STATUS_OVERFLOW)) {
                return STATUS_ERROR;
            }
            return STATUS_OK;
        }
        DELAY_US(1U);
        elapsed++;
    }

    return STATUS_TIMEOUT;
}

Status_t adc_read_data(sint16 *buffer, uint32 num_samples)
{
    if (!g_adc_initialized || buffer == NULL_PTR) {
        return STATUS_INVALID_PARAM;
    }

    /* Read from ADC buffer memory */
    volatile sint16 *adc_buf = (volatile sint16 *)0x52000000U;
    for (uint32 i = 0U; i < num_samples; i++) {
        buffer[i] = adc_buf[i];
    }

    return STATUS_OK;
}

boolean adc_is_busy(void)
{
    return !BIT_TST(REG_RD32(ADC_STATUS), ADC_STATUS_DONE);
}

void adc_stop(void)
{
    REG_WR32(ADC_CTRL, 0U);
}

uint32 adc_get_sample_rate(void)
{
    return g_sample_rate;
}
