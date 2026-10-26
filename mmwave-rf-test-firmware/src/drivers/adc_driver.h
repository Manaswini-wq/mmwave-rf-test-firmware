/**
 * @file adc_driver.h
 * @brief ADC subsystem driver interface
 */

#ifndef ADC_DRIVER_H
#define ADC_DRIVER_H

#include "platform_types.h"

Status_t adc_init(uint8 resolution, uint8 format, uint32 sample_rate);
Status_t adc_start_capture(uint32 num_samples);
Status_t adc_wait_done(uint32 timeout_us);
Status_t adc_read_data(sint16 *buffer, uint32 num_samples);
boolean  adc_is_busy(void);
void     adc_stop(void);
uint32   adc_get_sample_rate(void);

#endif /* ADC_DRIVER_H */
