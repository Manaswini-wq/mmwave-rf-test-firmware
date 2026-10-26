/**
 * @file phase_noise_test.h
 * @brief Phase noise measurement test interface
 */

#ifndef PHASE_NOISE_TEST_H
#define PHASE_NOISE_TEST_H

#include "platform_types.h"

/**
 * @brief Measure phase noise at a given offset frequency
 * @param channel    TX channel (0-2)
 * @param freq_ghz   Carrier frequency in GHz
 * @param offset_hz  Offset frequency in Hz
 * @return Phase noise in dBc/Hz
 */
float32 phase_noise_test_measure(uint8 channel, float32 freq_ghz, uint32 offset_hz);

/**
 * @brief Measure phase noise at standard offsets (100k, 1M, 10M)
 * @param channel    TX channel
 * @param freq_ghz   Carrier frequency
 * @param results    Array of 3 floats [100kHz, 1MHz, 10MHz]
 * @return STATUS_OK on success
 */
Status_t phase_noise_test_standard(uint8 channel, float32 freq_ghz, float32 results[3]);

#endif /* PHASE_NOISE_TEST_H */
