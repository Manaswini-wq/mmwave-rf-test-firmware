/**
 * @file tx_evm_test.h
 * @brief TX Error Vector Magnitude (EVM) measurement interface
 */

#ifndef TX_EVM_TEST_H
#define TX_EVM_TEST_H

#include "platform_types.h"

/**
 * @brief Measure TX EVM
 * @param channel   TX channel (0-2)
 * @param freq_ghz  Carrier frequency in GHz
 * @return EVM in dB (negative values indicate better quality)
 */
float32 tx_evm_test_measure(uint8 channel, float32 freq_ghz);

#endif /* TX_EVM_TEST_H */
