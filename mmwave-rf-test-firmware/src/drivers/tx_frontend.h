/**
 * @file tx_frontend.h
 * @brief TX frontend driver interface (PA, phase shifter, loopback)
 */

#ifndef TX_FRONTEND_H
#define TX_FRONTEND_H

#include "platform_types.h"

Status_t tx_frontend_init(void);
void     tx_frontend_enable_channel(uint8 channel);
void     tx_frontend_disable_channel(uint8 channel);
Status_t tx_frontend_set_power(uint8 channel, uint8 power_code);
Status_t tx_frontend_set_phase(uint8 channel, uint16 phase_code);
void     tx_frontend_enable_tone(void);
void     tx_frontend_disable_tone(void);
void     tx_frontend_enable_loopback(uint8 channel);
void     tx_frontend_disable_loopback(uint8 channel);

#endif /* TX_FRONTEND_H */
