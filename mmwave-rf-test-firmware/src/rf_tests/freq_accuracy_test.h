/**
 * @file freq_accuracy_test.h
 * @brief Frequency accuracy measurement test interface
 */

#ifndef FREQ_ACCURACY_TEST_H
#define FREQ_ACCURACY_TEST_H

#include "platform_types.h"

/**
 * @brief Measure frequency error of the synthesizer output
 * @param channel   TX channel
 * @param freq_ghz  Programmed frequency in GHz
 * @return Frequency error in kHz (positive = high, negative = low)
 */
float32 freq_accuracy_test_measure(uint8 channel, float32 freq_ghz);

#endif /* FREQ_ACCURACY_TEST_H */
