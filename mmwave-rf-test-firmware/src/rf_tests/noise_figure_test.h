/**
 * @file noise_figure_test.h
 * @brief Noise figure measurement test interface (Y-factor method)
 */

#ifndef NOISE_FIGURE_TEST_H
#define NOISE_FIGURE_TEST_H

#include "platform_types.h"

/**
 * @brief Measure receiver noise figure using Y-factor method
 * @param channel   RX channel index (0-3)
 * @param freq_ghz  LO frequency in GHz
 * @return Noise figure in dB
 */
float32 noise_figure_test_measure(uint8 channel, float32 freq_ghz);

#endif /* NOISE_FIGURE_TEST_H */
