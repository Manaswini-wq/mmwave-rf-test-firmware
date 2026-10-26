/**
 * @file synth_driver.h
 * @brief Frequency synthesizer driver interface
 */

#ifndef SYNTH_DRIVER_H
#define SYNTH_DRIVER_H

#include "platform_types.h"

Status_t synth_init(void);
Status_t synth_set_frequency_ghz(float32 freq_ghz);
Status_t synth_enable_cw_mode(void);
Status_t synth_enable_chirp_mode(float32 start_ghz, float32 slope_mhz_us, float32 idle_us);
Status_t synth_wait_lock(uint32 timeout_us);
void     synth_disable(void);
boolean  synth_is_locked(void);
float32  synth_get_frequency_ghz(void);

#endif /* SYNTH_DRIVER_H */
