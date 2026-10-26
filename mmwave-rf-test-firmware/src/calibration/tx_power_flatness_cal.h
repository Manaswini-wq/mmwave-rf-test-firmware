/**
 * @file tx_power_flatness_cal.h
 * @brief TX power flatness calibration interface
 */

#ifndef TX_POWER_FLATNESS_CAL_H
#define TX_POWER_FLATNESS_CAL_H

#include "platform_types.h"

/**
 * @brief Run TX power flatness calibration on a given channel
 * @param channel  TX channel index (0-2)
 * @return STATUS_OK if calibration converged within limits
 */
Status_t tx_power_flatness_cal_run(uint8 channel);

/**
 * @brief Get calibration coefficients for a channel
 * @param channel  TX channel index
 * @param coeffs   Output array for per-frequency DAC correction codes
 * @param num_pts  Number of frequency points
 * @return STATUS_OK on success
 */
Status_t tx_power_flatness_cal_get_coeffs(uint8 channel, sint16 *coeffs, uint32 num_pts);

#endif /* TX_POWER_FLATNESS_CAL_H */
