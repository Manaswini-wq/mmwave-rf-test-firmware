/**
 * @file test_limits.h
 * @brief Pass/fail limits for all RF production test parameters
 *
 * Defines minimum, maximum, and typical values for each RF test. Limits
 * are based on IWR6843 datasheet specifications and production requirements.
 * Separate limit sets are provided for production, characterization, and
 * evaluation modes.
 */

#ifndef TEST_LIMITS_H
#define TEST_LIMITS_H

#include "platform_types.h"

/* ========================================================================= */
/*  Limit structure                                                           */
/* ========================================================================= */
typedef struct {
    float32 lower;          /**< Lower pass/fail limit */
    float32 upper;          /**< Upper pass/fail limit */
    float32 typical;        /**< Typical / nominal value */
    const char *unit;       /**< Unit string for reporting */
} TestLimit_t;

/* Inline limit check */
#define LIMIT_CHECK(val, lim) \
    (((val) >= (lim).lower && (val) <= (lim).upper) ? TEST_RESULT_PASS : TEST_RESULT_FAIL)

#define LIMIT_CHECK_MARGINAL(val, lim, margin) \
    (((val) >= (lim).lower && (val) <= (lim).upper) ? \
     ((((val) >= ((lim).lower + (margin))) && ((val) <= ((lim).upper - (margin)))) ? \
      TEST_RESULT_PASS : TEST_RESULT_MARGINAL) : TEST_RESULT_FAIL)

/* ========================================================================= */
/*  TX Power limits (per channel)                                             */
/* ========================================================================= */
#define TX_POWER_LIMIT_MIN_DBM      10.0f
#define TX_POWER_LIMIT_MAX_DBM      13.0f
#define TX_POWER_LIMIT_TYP_DBM      12.0f

static const TestLimit_t TX_POWER_LIMITS = {
    .lower   = TX_POWER_LIMIT_MIN_DBM,
    .upper   = TX_POWER_LIMIT_MAX_DBM,
    .typical = TX_POWER_LIMIT_TYP_DBM,
    .unit    = "dBm"
};

/* TX power variation across channels */
#define TX_POWER_VARIATION_MAX_DB   1.5f

static const TestLimit_t TX_POWER_VARIATION_LIMITS = {
    .lower   = 0.0f,
    .upper   = TX_POWER_VARIATION_MAX_DB,
    .typical = 0.5f,
    .unit    = "dB"
};

/* ========================================================================= */
/*  TX Power Flatness limits                                                  */
/* ========================================================================= */
#define TX_FLATNESS_RIPPLE_MAX_DB   1.0f

static const TestLimit_t TX_FLATNESS_LIMITS = {
    .lower   = 0.0f,
    .upper   = TX_FLATNESS_RIPPLE_MAX_DB,
    .typical = 0.3f,
    .unit    = "dB pk-pk"
};

/* ========================================================================= */
/*  RX Sensitivity / Noise Floor limits                                       */
/* ========================================================================= */
#define RX_NOISE_FLOOR_MAX_DBM      (-100.0f)

static const TestLimit_t RX_NOISE_FLOOR_LIMITS = {
    .lower   = -120.0f,
    .upper   = RX_NOISE_FLOOR_MAX_DBM,
    .typical = -108.0f,
    .unit    = "dBm"
};

/* RX gain limits */
#define RX_GAIN_MIN_DB              40.0f
#define RX_GAIN_MAX_DB              55.0f

static const TestLimit_t RX_GAIN_LIMITS = {
    .lower   = RX_GAIN_MIN_DB,
    .upper   = RX_GAIN_MAX_DB,
    .typical = 48.0f,
    .unit    = "dB"
};

/* ========================================================================= */
/*  Phase Noise limits                                                        */
/* ========================================================================= */
static const TestLimit_t PN_100KHZ_LIMITS = {
    .lower   = -120.0f,
    .upper   = -75.0f,
    .typical = -82.0f,
    .unit    = "dBc/Hz"
};

static const TestLimit_t PN_1MHZ_LIMITS = {
    .lower   = -120.0f,
    .upper   = -85.0f,
    .typical = -95.0f,
    .unit    = "dBc/Hz"
};

static const TestLimit_t PN_10MHZ_LIMITS = {
    .lower   = -130.0f,
    .upper   = -100.0f,
    .typical = -110.0f,
    .unit    = "dBc/Hz"
};

/* ========================================================================= */
/*  Frequency Accuracy limits                                                 */
/* ========================================================================= */
#define FREQ_ACC_MAX_ERROR_KHZ      50.0f

static const TestLimit_t FREQ_ACCURACY_LIMITS = {
    .lower   = -FREQ_ACC_MAX_ERROR_KHZ,
    .upper   = FREQ_ACC_MAX_ERROR_KHZ,
    .typical = 0.0f,
    .unit    = "kHz"
};

/* ========================================================================= */
/*  Noise Figure limits                                                       */
/* ========================================================================= */
#define NF_MAX_DB                   14.0f

static const TestLimit_t NOISE_FIGURE_LIMITS = {
    .lower   = 5.0f,
    .upper   = NF_MAX_DB,
    .typical = 10.0f,
    .unit    = "dB"
};

/* ========================================================================= */
/*  TX EVM limits                                                             */
/* ========================================================================= */
#define TX_EVM_MAX_DB               (-25.0f)

static const TestLimit_t TX_EVM_LIMITS = {
    .lower   = -50.0f,
    .upper   = TX_EVM_MAX_DB,
    .typical = -32.0f,
    .unit    = "dB"
};

/* ========================================================================= */
/*  Calibration convergence limits                                            */
/* ========================================================================= */
static const TestLimit_t CAL_TX_FLATNESS_LIMITS = {
    .lower   = 0.0f,
    .upper   = 0.5f,
    .typical = 0.2f,
    .unit    = "dB pk-pk"
};

static const TestLimit_t CAL_RX_GAIN_MATCH_LIMITS = {
    .lower   = 0.0f,
    .upper   = 0.3f,
    .typical = 0.1f,
    .unit    = "dB"
};

static const TestLimit_t CAL_RX_PHASE_MATCH_LIMITS = {
    .lower   = 0.0f,
    .upper   = 2.0f,
    .typical = 0.5f,
    .unit    = "degrees"
};

static const TestLimit_t CAL_LO_LEAKAGE_LIMITS = {
    .lower   = -60.0f,
    .upper   = -35.0f,
    .typical = -45.0f,
    .unit    = "dBc"
};

/* ========================================================================= */
/*  Limit table (indexed by test ID)                                          */
/* ========================================================================= */
typedef enum {
    TEST_ID_TX_POWER        = 0,
    TEST_ID_TX_VARIATION    = 1,
    TEST_ID_TX_FLATNESS     = 2,
    TEST_ID_RX_NOISE_FLOOR  = 3,
    TEST_ID_RX_GAIN         = 4,
    TEST_ID_PHASE_NOISE_100K= 5,
    TEST_ID_PHASE_NOISE_1M  = 6,
    TEST_ID_PHASE_NOISE_10M = 7,
    TEST_ID_FREQ_ACCURACY   = 8,
    TEST_ID_NOISE_FIGURE    = 9,
    TEST_ID_TX_EVM          = 10,
    TEST_ID_MAX             = 11
} TestId_t;

/** Look up limits by test ID. Returns pointer to TestLimit_t or NULL. */
static inline const TestLimit_t *test_limits_get(TestId_t id) {
    static const TestLimit_t *table[] = {
        [TEST_ID_TX_POWER]         = &TX_POWER_LIMITS,
        [TEST_ID_TX_VARIATION]     = &TX_POWER_VARIATION_LIMITS,
        [TEST_ID_TX_FLATNESS]      = &TX_FLATNESS_LIMITS,
        [TEST_ID_RX_NOISE_FLOOR]   = &RX_NOISE_FLOOR_LIMITS,
        [TEST_ID_RX_GAIN]          = &RX_GAIN_LIMITS,
        [TEST_ID_PHASE_NOISE_100K] = &PN_100KHZ_LIMITS,
        [TEST_ID_PHASE_NOISE_1M]   = &PN_1MHZ_LIMITS,
        [TEST_ID_PHASE_NOISE_10M]  = &PN_10MHZ_LIMITS,
        [TEST_ID_FREQ_ACCURACY]    = &FREQ_ACCURACY_LIMITS,
        [TEST_ID_NOISE_FIGURE]     = &NOISE_FIGURE_LIMITS,
        [TEST_ID_TX_EVM]           = &TX_EVM_LIMITS,
    };
    if (id >= TEST_ID_MAX) return NULL_PTR;
    return table[id];
}

#endif /* TEST_LIMITS_H */
