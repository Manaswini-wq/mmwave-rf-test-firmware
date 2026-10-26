/**
 * @file pga_driver.h
 * @brief Programmable Gain Amplifier driver interface
 */

#ifndef PGA_DRIVER_H
#define PGA_DRIVER_H

#include "platform_types.h"

Status_t pga_init(void);
Status_t pga_set_gain(uint8 channel, uint8 gain_code);
uint8    pga_get_gain(uint8 channel);
Status_t pga_set_offset(uint8 channel, sint16 offset);
void     pga_enable(void);
void     pga_disable(void);

#endif /* PGA_DRIVER_H */
