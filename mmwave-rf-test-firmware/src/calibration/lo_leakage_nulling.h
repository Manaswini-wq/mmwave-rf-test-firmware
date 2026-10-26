/**
 * @file lo_leakage_nulling.h
 * @brief LO leakage nulling calibration interface
 */

#ifndef LO_LEAKAGE_NULLING_H
#define LO_LEAKAGE_NULLING_H

#include "platform_types.h"

/**
 * @brief Run LO leakage nulling calibration on a given RX channel
 * @param channel  RX channel (0-3)
 * @return STATUS_OK if LO leakage suppressed below target
 */
Status_t lo_leakage_nulling_run(uint8 channel);

/**
 * @brief Get current LO leakage level after calibration
 * @param channel  RX channel
 * @return LO leakage in dBc (relative to signal)
 */
float32 lo_leakage_nulling_get_level(uint8 channel);

#endif /* LO_LEAKAGE_NULLING_H */
