/**
 * @file rom_boot_hooks.h
 * @brief ROM firmware boot hook interface
 */

#ifndef ROM_BOOT_HOOKS_H
#define ROM_BOOT_HOOKS_H

#include "platform_types.h"

Status_t rom_boot_hooks_init(void);
Status_t rom_call_function(uint32 func_id, uint32 param);
uint32   rom_get_version(void);

#endif /* ROM_BOOT_HOOKS_H */
