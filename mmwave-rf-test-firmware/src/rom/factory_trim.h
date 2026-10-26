/**
 * @file factory_trim.h
 * @brief Factory trim data loader interface
 */

#ifndef FACTORY_TRIM_H
#define FACTORY_TRIM_H

#include "platform_types.h"

typedef struct {
    sint16  synth_trim[8];      /**< Synthesizer/VCO trim codes */
    sint16  tx_power_trim[3];   /**< Per-TX-channel power trim */
    sint16  rx_gain_trim[4];    /**< Per-RX-channel gain trim */
    sint16  pga_offset_trim[4]; /**< PGA DC offset trim per channel */
    sint16  adc_offset_trim[4]; /**< ADC offset trim per channel */
    sint16  temp_sensor_trim;   /**< Temperature sensor calibration */
    uint32  lot_id;
    uint16  wafer_id;
    uint16  die_xy;
    uint32  checksum;
} FactoryTrimData_t;

Status_t factory_trim_load(void);
Status_t factory_trim_apply(void);
const FactoryTrimData_t *factory_trim_get_data(void);
boolean factory_trim_is_valid(void);

#endif /* FACTORY_TRIM_H */
