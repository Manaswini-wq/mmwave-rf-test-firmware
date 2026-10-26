/**
 * @file factory_trim.c
 * @brief Factory trim data loader from OTP memory
 *
 * Reads factory calibration / trim data from the one-time-programmable (OTP)
 * memory.  The OTP contains per-device trim codes for the synthesizer, TX
 * power amplifiers, RX gain, PGA offsets, ADC offsets, and temperature
 * sensor that were programmed at wafer sort or final test.
 */

#include "rom/factory_trim.h"
#include "iwr6843_regs.h"
#include "rf_test_cfg.h"

#include <string.h>

/* ========================================================================= */
/*  Internal state                                                            */
/* ========================================================================= */
static FactoryTrimData_t g_trim_data;
static boolean g_trim_valid = FALSE;

/* ========================================================================= */
/*  Internal helpers                                                          */
/* ========================================================================= */

/**
 * @brief Read a 32-bit word from OTP memory
 */
static Status_t otp_read_word(uint32 offset, uint32 *value)
{
#ifndef HOST_BUILD
    /* Program OTP address */
    REG_WR32(OTP_ADDR, OTP_TRIM_BASE_ADDR + offset);

    /* Trigger read */
    REG_WR32(OTP_CTRL, OTP_CTRL_READ);

    /* Wait for completion */
    uint32 timeout = 1000U;
    while (timeout > 0U) {
        uint32 status = REG_RD32(OTP_STATUS);
        if (BIT_TST(status, OTP_STATUS_DONE)) {
            if (BIT_TST(status, OTP_STATUS_ERROR)) {
                return STATUS_HW_FAULT;
            }
            *value = REG_RD32(OTP_DATA);
            return STATUS_OK;
        }
        DELAY_US(1U);
        timeout--;
    }
    return STATUS_TIMEOUT;
#else
    /* Simulated OTP data for host-side testing */
    *value = 0U;
    return STATUS_OK;
#endif
}

/**
 * @brief Read an array of 16-bit trim values from OTP
 */
static Status_t otp_read_trim_array(uint32 base_offset, sint16 *data, uint32 count)
{
    for (uint32 i = 0U; i < count; i += 2U) {
        uint32 word;
        Status_t status = otp_read_word(base_offset + i * 2U, &word);
        if (status != STATUS_OK) return status;

        data[i] = (sint16)(word & 0xFFFFU);
        if (i + 1U < count) {
            data[i + 1U] = (sint16)((word >> 16) & 0xFFFFU);
        }
    }
    return STATUS_OK;
}

/**
 * @brief Compute checksum over trim data
 */
static uint32 compute_checksum(const FactoryTrimData_t *data)
{
    const uint8 *bytes = (const uint8 *)data;
    uint32 sum = 0U;
    uint32 len = sizeof(FactoryTrimData_t) - sizeof(uint32);  /* Exclude checksum field */

    for (uint32 i = 0U; i < len; i++) {
        sum += (uint32)bytes[i];
        sum = (sum << 1) | (sum >> 31);  /* Rotate left */
    }
    return sum;
}

/* ========================================================================= */
/*  Public API                                                                */
/* ========================================================================= */

Status_t factory_trim_load(void)
{
    Status_t status;

    memset(&g_trim_data, 0, sizeof(g_trim_data));
    g_trim_valid = FALSE;

    /* Read synth trim values */
    status = otp_read_trim_array(OTP_TRIM_SYNTH_OFFSET, g_trim_data.synth_trim, 8U);
    if (status != STATUS_OK) return status;

    /* Read TX power trim */
    status = otp_read_trim_array(OTP_TRIM_TX_OFFSET, g_trim_data.tx_power_trim, 3U);
    if (status != STATUS_OK) return status;

    /* Read RX gain trim */
    status = otp_read_trim_array(OTP_TRIM_RX_OFFSET, g_trim_data.rx_gain_trim, 4U);
    if (status != STATUS_OK) return status;

    /* Read PGA offset trim */
    status = otp_read_trim_array(OTP_TRIM_PGA_OFFSET, g_trim_data.pga_offset_trim, 4U);
    if (status != STATUS_OK) return status;

    /* Read ADC offset trim */
    status = otp_read_trim_array(OTP_TRIM_ADC_OFFSET, g_trim_data.adc_offset_trim, 4U);
    if (status != STATUS_OK) return status;

    /* Read temperature sensor trim */
    uint32 temp_word;
    status = otp_read_word(OTP_TRIM_TEMP_OFFSET, &temp_word);
    if (status != STATUS_OK) return status;
    g_trim_data.temp_sensor_trim = (sint16)(temp_word & 0xFFFFU);

    /* Read lot/wafer ID */
    uint32 lot_word;
    status = otp_read_word(OTP_TRIM_LOT_ID_OFFSET, &lot_word);
    if (status != STATUS_OK) return status;
    g_trim_data.lot_id = lot_word;

    uint32 wafer_word;
    status = otp_read_word(OTP_TRIM_WAFER_OFFSET, &wafer_word);
    if (status != STATUS_OK) return status;
    g_trim_data.wafer_id = (uint16)(wafer_word & 0xFFFFU);
    g_trim_data.die_xy   = (uint16)((wafer_word >> 16) & 0xFFFFU);

    /* Read and verify checksum */
    uint32 stored_checksum;
    status = otp_read_word(OTP_TRIM_CHECKSUM, &stored_checksum);
    if (status != STATUS_OK) return status;
    g_trim_data.checksum = stored_checksum;

    uint32 computed = compute_checksum(&g_trim_data);
    if (computed != stored_checksum) {
        /* Checksum mismatch - use defaults but flag as invalid */
        g_trim_valid = FALSE;
        return STATUS_OK;  /* Non-fatal: firmware can run with defaults */
    }

    g_trim_valid = TRUE;
    return STATUS_OK;
}

Status_t factory_trim_apply(void)
{
    if (!g_trim_valid) return STATUS_NOT_READY;

    /* Apply synth VCO trim */
    REG_WR32(SYNTH_VCO_BIAS, (uint32)g_trim_data.synth_trim[0] & 0xFFU);

    /* Apply TX power trim per channel */
    REG_WR32(TX_CH0_POWER, (uint32)(TX_PWR_DEFAULT_CODE + g_trim_data.tx_power_trim[0]));
    REG_WR32(TX_CH1_POWER, (uint32)(TX_PWR_DEFAULT_CODE + g_trim_data.tx_power_trim[1]));
    REG_WR32(TX_CH2_POWER, (uint32)(TX_PWR_DEFAULT_CODE + g_trim_data.tx_power_trim[2]));

    /* Apply PGA offset trims */
    for (uint8 ch = 0U; ch < RF_NUM_RX_CHANNELS; ch++) {
        uint32 offset_reg = PGA_OFFSET_CH0 + (uint32)ch * 4U;
        REG_WR32(offset_reg, (uint32)(g_trim_data.pga_offset_trim[ch] & 0xFFFF));
    }

    return STATUS_OK;
}

const FactoryTrimData_t *factory_trim_get_data(void)
{
    return &g_trim_data;
}

boolean factory_trim_is_valid(void)
{
    return g_trim_valid;
}
