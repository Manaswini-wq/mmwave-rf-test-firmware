/**
 * @file dac_driver.c
 * @brief DAC output driver for IWR6843
 *
 * Controls the on-chip DAC used for calibration signal injection, DC offset
 * correction, and test tone generation.  The DAC has 12-bit resolution with
 * a full-scale output of approximately 1.2V.
 */

#include "drivers/dac_driver.h"
#include "iwr6843_regs.h"
#include "rf_test_cfg.h"

static boolean g_dac_initialized = FALSE;

Status_t dac_init(void)
{
    /* Reset DAC */
    REG_WR32(DAC_CTRL, 0U);
    DELAY_US(5U);

    /* Enable DAC with default calibration mode off */
    REG_WR32(DAC_CAL_CTRL, 0U);
    REG_WR32(DAC_DATA, 0x0800U);  /* Mid-scale default */

    /* Enable DAC */
    REG_WR32(DAC_CTRL, DAC_CTRL_ENABLE);
    DELAY_US(10U);

    g_dac_initialized = TRUE;
    return STATUS_OK;
}

Status_t dac_write(uint16 value)
{
    if (!g_dac_initialized) return STATUS_NOT_INIT;

    REG_WR32(DAC_DATA, (uint32)(value & 0x0FFFU));
    REG_WR32(DAC_CTRL, DAC_CTRL_ENABLE | DAC_CTRL_UPDATE);

    /* Wait for DAC to settle */
    DELAY_US(2U);

    return STATUS_OK;
}

Status_t dac_write_calibration(uint16 value)
{
    if (!g_dac_initialized) return STATUS_NOT_INIT;

    /* Write to calibration DAC register (separate path) */
    REG_WR32(DAC_CAL_DATA, (uint32)(value & 0x0FFFU));
    REG_WR32(DAC_CAL_CTRL, BIT(0));  /* Trigger cal DAC update */
    DELAY_US(2U);

    return STATUS_OK;
}

void dac_enable(void)
{
    REG_WR32(DAC_CTRL, DAC_CTRL_ENABLE);
}

void dac_disable(void)
{
    REG_WR32(DAC_CTRL, 0U);
    REG_WR32(DAC_DATA, 0U);
}
