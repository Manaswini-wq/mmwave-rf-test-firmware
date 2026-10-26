/**
 * @file platform_types.h
 * @brief Platform-specific type definitions for mmWave RF test firmware
 *
 * Provides standard integer types, boolean, return codes, and common macros
 * following AUTOSAR-style naming conventions for bare-metal embedded firmware.
 */

#ifndef PLATFORM_TYPES_H
#define PLATFORM_TYPES_H

#include <stdint.h>
#include <stddef.h>

/* ========================================================================= */
/*  Standard type aliases                                                     */
/* ========================================================================= */
typedef uint8_t     uint8;
typedef uint16_t    uint16;
typedef uint32_t    uint32;
typedef uint64_t    uint64;
typedef int8_t      sint8;
typedef int16_t     sint16;
typedef int32_t     sint32;
typedef int64_t     sint64;
typedef float       float32;
typedef double      float64;

typedef volatile uint8_t    vuint8;
typedef volatile uint16_t   vuint16;
typedef volatile uint32_t   vuint32;
typedef volatile int32_t    vsint32;

/* Boolean type */
typedef uint8_t     boolean;
#ifndef TRUE
#define TRUE        ((boolean)1U)
#endif
#ifndef FALSE
#define FALSE       ((boolean)0U)
#endif

/* Null pointer */
#ifndef NULL_PTR
#define NULL_PTR    ((void *)0)
#endif

/* ========================================================================= */
/*  Return / status codes                                                     */
/* ========================================================================= */
typedef sint32 Status_t;

#define STATUS_OK               ((Status_t)0)
#define STATUS_ERROR            ((Status_t)-1)
#define STATUS_TIMEOUT          ((Status_t)-2)
#define STATUS_INVALID_PARAM    ((Status_t)-3)
#define STATUS_HW_FAULT         ((Status_t)-4)
#define STATUS_NOT_READY        ((Status_t)-5)
#define STATUS_CAL_FAIL         ((Status_t)-6)
#define STATUS_LIMIT_FAIL       ((Status_t)-7)
#define STATUS_BUSY             ((Status_t)-8)
#define STATUS_NOT_INIT         ((Status_t)-9)

/* ========================================================================= */
/*  Test result type                                                          */
/* ========================================================================= */
typedef enum {
    TEST_RESULT_PASS     = 0,
    TEST_RESULT_FAIL     = 1,
    TEST_RESULT_MARGINAL = 2,
    TEST_RESULT_SKIPPED  = 3,
    TEST_RESULT_ERROR    = 4
} TestResult_t;

/* ========================================================================= */
/*  RF channel indices                                                        */
/* ========================================================================= */
#define RF_TX_CHANNEL_0     0U
#define RF_TX_CHANNEL_1     1U
#define RF_TX_CHANNEL_2     2U
#define RF_NUM_TX_CHANNELS  3U

#define RF_RX_CHANNEL_0     0U
#define RF_RX_CHANNEL_1     1U
#define RF_RX_CHANNEL_2     2U
#define RF_RX_CHANNEL_3     3U
#define RF_NUM_RX_CHANNELS  4U

/* ========================================================================= */
/*  Common macros                                                             */
/* ========================================================================= */

/** Register read/write helpers (32-bit MMIO) */
#define REG_WR32(addr, val)     (*(vuint32 *)(addr) = (uint32)(val))
#define REG_RD32(addr)          (*(vuint32 *)(addr))

/** Bit-field helpers */
#define BIT(n)                  (1U << (n))
#define BIT_SET(reg, mask)      ((reg) |= (mask))
#define BIT_CLR(reg, mask)      ((reg) &= ~(mask))
#define BIT_TST(reg, mask)      (((reg) & (mask)) != 0U)

/** Min / Max */
#define MIN(a, b)               (((a) < (b)) ? (a) : (b))
#define MAX(a, b)               (((a) > (b)) ? (a) : (b))
#define CLAMP(x, lo, hi)        (MIN(MAX((x), (lo)), (hi)))

/** Array element count */
#define ARRAY_SIZE(a)           (sizeof(a) / sizeof((a)[0]))

/** Alignment */
#define ALIGN_UP(x, a)          (((x) + ((a) - 1U)) & ~((a) - 1U))
#define ALIGN_DOWN(x, a)        ((x) & ~((a) - 1U))

/** Conversion macros */
#define MHZ_TO_HZ(f)            ((uint64)(f) * 1000000ULL)
#define GHZ_TO_HZ(f)            ((uint64)((f) * 1000000000.0))
#define DB_TO_LINEAR(db)        (powf(10.0f, (db) / 10.0f))
#define LINEAR_TO_DB(lin)       (10.0f * log10f(lin))

/** Delay helpers (placeholder - implement per-platform) */
#ifndef HOST_BUILD
#define DELAY_US(us)            platform_delay_us(us)
#define DELAY_MS(ms)            platform_delay_us((ms) * 1000U)
#else
#define DELAY_US(us)            ((void)0)
#define DELAY_MS(ms)            ((void)0)
#endif

/* ========================================================================= */
/*  Compiler attributes                                                       */
/* ========================================================================= */
#define SECTION(name)           __attribute__((section(name)))
#define PACKED                  __attribute__((packed))
#define ALIGNED(n)              __attribute__((aligned(n)))
#define WEAK                    __attribute__((weak))
#define NORETURN                __attribute__((noreturn))
#define INLINE                  static inline __attribute__((always_inline))

/* ========================================================================= */
/*  Function prototypes (platform layer)                                      */
/* ========================================================================= */
void platform_delay_us(uint32 us);
void platform_reset(void);
uint32 platform_get_tick_ms(void);

#endif /* PLATFORM_TYPES_H */
