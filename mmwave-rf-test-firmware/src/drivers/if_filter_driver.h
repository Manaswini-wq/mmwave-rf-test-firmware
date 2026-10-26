/**
 * @file if_filter_driver.h
 * @brief IF bandpass filter driver interface
 */

#ifndef IF_FILTER_DRIVER_H
#define IF_FILTER_DRIVER_H

#include "platform_types.h"

Status_t if_filter_init(void);
Status_t if_filter_set_bandwidth(uint8 hpf_code, uint8 lpf_code);
void     if_filter_enable(void);
void     if_filter_disable(void);

#endif /* IF_FILTER_DRIVER_H */
