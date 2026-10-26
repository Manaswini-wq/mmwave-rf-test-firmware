/**
 * @file dma_manager.h
 * @brief DMA manager interface for high-speed data transfers
 */

#ifndef DMA_MANAGER_H
#define DMA_MANAGER_H

#include "platform_types.h"

Status_t dma_manager_init(void);
Status_t dma_manager_configure(uint8 dma_ch, uint32 src_addr, uint32 dst_addr, uint32 size_bytes);
Status_t dma_manager_start(uint8 dma_ch);
Status_t dma_manager_stop(uint8 dma_ch);
Status_t dma_manager_wait_done(uint8 dma_ch, uint32 timeout_us);
boolean  dma_manager_is_done(uint8 dma_ch);
void     dma_manager_set_timing_offset(uint8 channel, sint32 offset_ps);

#endif /* DMA_MANAGER_H */
