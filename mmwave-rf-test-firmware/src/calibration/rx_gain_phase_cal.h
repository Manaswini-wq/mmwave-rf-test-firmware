/**
 * @file rx_gain_phase_cal.h
 * @brief RX gain and phase calibration interface
 */

#ifndef RX_GAIN_PHASE_CAL_H
#define RX_GAIN_PHASE_CAL_H

#include "platform_types.h"

typedef struct {
    float32 gain_offset_db;     /**< Gain correction relative to reference */
    float32 phase_offset_deg;   /**< Phase correction relative to reference */
} RxCalCoeffs_t;

Status_t rx_gain_phase_cal_run(uint8 ref_channel);
Status_t rx_gain_phase_cal_get_coeffs(uint8 channel, RxCalCoeffs_t *coeffs);
Status_t rx_gain_phase_cal_apply(uint8 channel);

#endif /* RX_GAIN_PHASE_CAL_H */
