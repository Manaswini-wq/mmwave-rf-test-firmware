/**
 * @file rom_boot_hooks.c
 * @brief ROM firmware boot hook implementation
 *
 * Interfaces with the IWR6843 on-chip ROM firmware that provides early
 * hardware initialization routines.  The ROM contains a function pointer
 * table at a fixed address that allows the application firmware to call
 * ROM-resident routines for clock setup, PLL initialization, and
 * peripheral reset sequencing.
 */

#include "rom/rom_boot_hooks.h"
#include "iwr6843_regs.h"
#include "rf_test_cfg.h"

/* ========================================================================= */
/*  ROM function table                                                        */
/* ========================================================================= */

/** ROM function pointer table base address */
#define ROM_FUNC_TABLE_BASE     0xFFFF0100U

/** ROM function IDs */
#define ROM_FUNC_CLK_INIT       0U
#define ROM_FUNC_PLL_INIT       1U
#define ROM_FUNC_PERIPH_RESET   2U
#define ROM_FUNC_EFUSE_READ     3U
#define ROM_FUNC_SECURE_BOOT    4U
#define ROM_FUNC_GET_VERSION    5U
#define ROM_FUNC_MAX            6U

/** ROM version register (in ROM address space) */
#define ROM_VERSION_ADDR        0xFFFF0000U

typedef Status_t (*RomFunc_t)(uint32 param);

/* ========================================================================= */
/*  Internal helpers                                                          */
/* ========================================================================= */

/**
 * @brief Get ROM function pointer by index
 */
static RomFunc_t get_rom_function(uint32 func_id)
{
    if (func_id >= ROM_FUNC_MAX) return NULL_PTR;

#ifndef HOST_BUILD
    volatile uint32 *func_table = (volatile uint32 *)ROM_FUNC_TABLE_BASE;
    uint32 func_addr = func_table[func_id];

    /* Validate pointer is within ROM space */
    if (func_addr < 0xFFFF0000U || func_addr > 0xFFFFFFFFU) {
        return NULL_PTR;
    }

    return (RomFunc_t)func_addr;
#else
    return NULL_PTR;
#endif
}

/* ========================================================================= */
/*  Public API                                                                */
/* ========================================================================= */

Status_t rom_boot_hooks_init(void)
{
    Status_t status;

#ifndef HOST_BUILD
    /* Step 1: Validate ROM version */
    uint32 rom_ver = REG_RD32(ROM_VERSION_ADDR);
    if ((rom_ver & 0xFFFF0000U) != 0x52460000U) {  /* "RF" magic */
        return STATUS_HW_FAULT;
    }

    /* Step 2: Call ROM clock initialization */
    RomFunc_t clk_init = get_rom_function(ROM_FUNC_CLK_INIT);
    if (clk_init != NULL_PTR) {
        status = clk_init(200U);  /* 200 MHz R5F core clock */
        if (status != STATUS_OK) return status;
    }

    /* Step 3: Call ROM PLL initialization */
    RomFunc_t pll_init = get_rom_function(ROM_FUNC_PLL_INIT);
    if (pll_init != NULL_PTR) {
        status = pll_init(0U);  /* Default PLL config */
        if (status != STATUS_OK) return status;
    }

    /* Step 4: Peripheral reset sequencing */
    RomFunc_t periph_rst = get_rom_function(ROM_FUNC_PERIPH_RESET);
    if (periph_rst != NULL_PTR) {
        status = periph_rst(0xFFFFFFFFU);  /* Reset all peripherals */
        if (status != STATUS_OK) return status;
    }
#else
    status = STATUS_OK;
#endif

    return STATUS_OK;
}

Status_t rom_call_function(uint32 func_id, uint32 param)
{
    RomFunc_t func = get_rom_function(func_id);
    if (func == NULL_PTR) return STATUS_INVALID_PARAM;
    return func(param);
}

uint32 rom_get_version(void)
{
#ifndef HOST_BUILD
    return REG_RD32(ROM_VERSION_ADDR);
#else
    return 0x52460100U;  /* Simulated version */
#endif
}
