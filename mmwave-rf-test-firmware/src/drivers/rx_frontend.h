/**
 * @file rx_frontend.h
 * @brief RX frontend driver interface (LNA, mixer, phase offset)
 */

#ifndef RX_FRONTEND_H
#define RX_FRONTEND_H

#include "platform_types.h"

Status_t rx_frontend_init(void);
void     rx_frontend_enable_channel(uint8 channel);
void     rx_frontend_disable_channel(uint8 channel);
Status_t rx_frontend_set_lna_gain(uint8 channel, uint8 gain_code);
Status_t rx_frontend_set_phase_offset(uint8 channel, sint16 phase_tenths_deg);
void     rx_frontend_enable_mixer(void);
void     rx_frontend_disable_mixer(void);

#endif /* RX_FRONTEND_H */
