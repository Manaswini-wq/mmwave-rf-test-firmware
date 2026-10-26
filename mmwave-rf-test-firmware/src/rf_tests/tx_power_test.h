/**
 * @file tx_power_test.h
 * @brief TX power measurement test interface
 */

#ifndef TX_POWER_TEST_H
#define TX_POWER_TEST_H

#include "platform_types.h"

/**
 * @brief Measure TX output power on a given channel at a given frequency
 * @param channel   TX channel index (0-2)
 * @param freq_ghz  Test frequency in GHz
 * @return Measured TX power in dBm
 */
float32 tx_power_test_measure(uint8 channel, float32 freq_ghz);

/**
 * @brief Run full TX power test across frequency range
 * @param channel   TX channel index
 * @param results   Array to store per-frequency results
 * @param num_pts   Number of frequency points
 * @return STATUS_OK on success
 */
Status_t tx_power_test_sweep(uint8 channel, float32 *results, uint32 num_pts);

/**
 * @brief Measure TX power variation across all channels
 * @param freq_ghz  Test frequency
 * @return Maximum variation in dB
 */
float32 tx_power_test_channel_variation(float32 freq_ghz);

#endif /* TX_POWER_TEST_H */
