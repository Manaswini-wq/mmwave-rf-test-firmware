/**
 * @file inter_channel_cal.h
 * @brief Inter-channel calibration interface
 */

#ifndef INTER_CHANNEL_CAL_H
#define INTER_CHANNEL_CAL_H

#include "platform_types.h"

typedef struct {
    float32 time_offset_ps;     /**< Timing offset in picoseconds */
    float32 gain_offset_db;     /**< Gain offset in dB */
    float32 phase_offset_deg;   /**< Phase offset in degrees */
} InterChCalResult_t;

Status_t inter_channel_cal_run(void);
Status_t inter_channel_cal_get(uint8 channel, InterChCalResult_t *result);

#endif /* INTER_CHANNEL_CAL_H */
