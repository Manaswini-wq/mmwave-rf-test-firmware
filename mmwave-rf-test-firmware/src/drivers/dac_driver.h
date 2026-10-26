/**
 * @file dac_driver.h
 * @brief DAC output driver interface
 */

#ifndef DAC_DRIVER_H
#define DAC_DRIVER_H

#include "platform_types.h"

Status_t dac_init(void);
Status_t dac_write(uint16 value);
Status_t dac_write_calibration(uint16 value);
void     dac_enable(void);
void     dac_disable(void);

#endif /* DAC_DRIVER_H */
