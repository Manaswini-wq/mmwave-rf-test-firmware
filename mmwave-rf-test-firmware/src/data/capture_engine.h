/**
 * @file capture_engine.h
 * @brief DMA-based data capture engine interface
 */

#ifndef CAPTURE_ENGINE_H
#define CAPTURE_ENGINE_H

#include "platform_types.h"

Status_t capture_engine_init(void);
Status_t capture_engine_start(uint8 channel, uint32 num_samples);
Status_t capture_engine_wait_done(void);
Status_t capture_engine_read_iq(sint16 *buf_i, sint16 *buf_q, uint32 num_samples);
boolean  capture_engine_is_busy(void);
void     capture_engine_abort(void);

#endif /* CAPTURE_ENGINE_H */
