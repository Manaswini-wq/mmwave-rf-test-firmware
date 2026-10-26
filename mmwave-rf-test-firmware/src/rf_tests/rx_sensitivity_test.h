/**
 * @file rx_sensitivity_test.h
 * @brief RX sensitivity and noise floor measurement test interface
 */

#ifndef RX_SENSITIVITY_TEST_H
#define RX_SENSITIVITY_TEST_H

#include "platform_types.h"

/**
 * @brief Measure RX noise floor on a given channel
 * @param channel   RX channel index (0-3)
 * @param freq_ghz  LO frequency in GHz
 * @return Noise floor in dBm
 */
float32 rx_sensitivity_test_measure(uint8 channel, float32 freq_ghz);

/**
 * @brief Measure RX chain gain using loopback
 * @param channel   RX channel index
 * @param freq_ghz  Frequency in GHz
 * @return Measured gain in dB
 */
float32 rx_sensitivity_test_gain(uint8 channel, float32 freq_ghz);

#endif /* RX_SENSITIVITY_TEST_H */
