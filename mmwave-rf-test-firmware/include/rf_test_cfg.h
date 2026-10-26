/**
 * @file rf_test_cfg.h
 * @brief Configuration parameters for RF production tests and calibration
 *
 * Defines default frequency bands, power levels, capture parameters, and
 * calibration iteration limits for the mmWave RF test firmware.
 */

#ifndef RF_TEST_CFG_H
#define RF_TEST_CFG_H

#include "platform_types.h"

/* ========================================================================= */
/*  Frequency configuration                                                   */
/* ========================================================================= */
#define RF_FREQ_MIN_GHZ             60.0f       /* Minimum RF frequency */
#define RF_FREQ_MAX_GHZ             64.0f       /* Maximum RF frequency */
#define RF_FREQ_CENTER_GHZ          62.0f       /* Default center frequency */
#define RF_FREQ_STEP_MHZ            100.0f      /* Frequency step for sweeps */

/* For AWR2243/AWR2944 77 GHz band */
#define RF_FREQ_77G_MIN_GHZ         76.0f
#define RF_FREQ_77G_MAX_GHZ         81.0f
#define RF_FREQ_77G_CENTER_GHZ      77.0f

/* Synthesizer parameters */
#define SYNTH_REF_CLK_MHZ           40.0f       /* Reference clock */
#define SYNTH_PLL_LOOP_BW_KHZ       300.0f      /* PLL loop bandwidth */
#define SYNTH_LOCK_TIMEOUT_US       500U        /* PLL lock timeout */
#define SYNTH_RAMP_SLOPE_MHZ_US     70.0f       /* Default chirp slope */

/* ========================================================================= */
/*  TX power test configuration                                               */
/* ========================================================================= */
#define TX_PWR_TEST_FREQ_GHZ        62.0f       /* TX power test frequency */
#define TX_PWR_DEFAULT_CODE         0x20U       /* Default TX power DAC code */
#define TX_PWR_MAX_CODE             0x3FU       /* Maximum TX power code */
#define TX_PWR_TEST_TONE_EN         TRUE        /* Enable CW tone for test */
#define TX_PWR_LOOPBACK_ATTEN_DB    30.0f       /* Loopback path attenuation */
#define TX_PWR_NUM_AVERAGES         16U         /* Number of measurements */
#define TX_PWR_SETTLE_TIME_US       100U        /* Settling time after config */

/* ========================================================================= */
/*  RX sensitivity test configuration                                         */
/* ========================================================================= */
#define RX_SENS_LNA_GAIN            RX_LNA_GAIN_MAX
#define RX_SENS_PGA_GAIN            PGA_GAIN_42DB
#define RX_SENS_CAPTURE_SAMPLES     1024U
#define RX_SENS_FFT_SIZE            1024U
#define RX_SENS_NUM_AVERAGES        32U

/* ========================================================================= */
/*  Phase noise test configuration                                            */
/* ========================================================================= */
#define PN_OFFSET_100KHZ            100000U     /* 100 kHz offset */
#define PN_OFFSET_1MHZ              1000000U    /* 1 MHz offset */
#define PN_OFFSET_10MHZ             10000000U   /* 10 MHz offset */
#define PN_FFT_SIZE                 4096U       /* FFT size for PN measurement */
#define PN_NUM_AVERAGES             64U
#define PN_WINDOW_TYPE              1U          /* 0=rect, 1=hanning, 2=blackman */

/* ========================================================================= */
/*  Frequency accuracy test configuration                                     */
/* ========================================================================= */
#define FREQ_ACC_CAPTURE_SAMPLES    4096U
#define FREQ_ACC_SAMPLE_RATE_MSPS   10.0f
#define FREQ_ACC_NUM_AVERAGES       8U

/* ========================================================================= */
/*  Noise figure test configuration                                           */
/* ========================================================================= */
#define NF_HOT_TEMP_K               290.0f      /* Hot source temperature */
#define NF_COLD_TEMP_K              77.0f       /* Cold source temperature (LN2) */
#define NF_ENR_DB                   15.0f       /* ENR of noise source */
#define NF_BW_MHZ                   5.0f        /* Measurement bandwidth */
#define NF_NUM_AVERAGES             32U

/* ========================================================================= */
/*  TX EVM test configuration                                                 */
/* ========================================================================= */
#define EVM_SYMBOL_RATE_MSPS        1.0f
#define EVM_NUM_SYMBOLS             256U
#define EVM_CAPTURE_OVERSAMPLE      4U

/* ========================================================================= */
/*  Calibration parameters                                                    */
/* ========================================================================= */
#define CAL_MAX_ITERATIONS          20U
#define CAL_CONVERGENCE_THRESHOLD   0.1f        /* dB or degree threshold */

/* TX flatness calibration */
#define TX_FLAT_CAL_FREQ_POINTS     20U         /* Number of frequency points */
#define TX_FLAT_CAL_RIPPLE_TARGET   0.5f        /* Target ripple in dB */
#define TX_FLAT_CAL_STEP_SIZE       1U          /* DAC code step size */

/* RX gain/phase calibration */
#define RX_GP_CAL_REF_CHANNEL       0U          /* Reference RX channel */
#define RX_GP_CAL_GAIN_TOL_DB       0.3f        /* Gain tolerance */
#define RX_GP_CAL_PHASE_TOL_DEG     2.0f        /* Phase tolerance */

/* LO leakage nulling */
#define LO_NULL_STEP_SIZE           1           /* DAC offset step */
#define LO_NULL_INITIAL_RANGE       64          /* Initial search range */
#define LO_NULL_TARGET_DBC          -40.0f      /* Target suppression */

/* Inter-channel calibration */
#define INTER_CH_TIME_TOL_PS        50.0f       /* Timing tolerance */
#define INTER_CH_GAIN_TOL_DB        0.2f        /* Gain tolerance */
#define INTER_CH_PHASE_TOL_DEG      1.0f        /* Phase tolerance */

/* ========================================================================= */
/*  ADC / Data capture                                                        */
/* ========================================================================= */
#define ADC_DEFAULT_RESOLUTION      ADC_RES_16BIT
#define ADC_DEFAULT_FORMAT          ADC_FMT_COMPLEX
#define ADC_DEFAULT_SAMPLE_RATE     12500000U   /* 12.5 MSPS */
#define ADC_BUFFER_SIZE_SAMPLES     4096U
#define DMA_PING_PONG_ENABLE        TRUE

/* ========================================================================= */
/*  UART / Protocol                                                           */
/* ========================================================================= */
#define UART_BAUD_RATE              921600U
#define UART_CMD_BUF_SIZE           256U
#define UART_RESP_BUF_SIZE          1024U
#define UART_TIMEOUT_MS             5000U

/* ========================================================================= */
/*  Test sequencing                                                           */
/* ========================================================================= */
typedef struct {
    uint32  test_id;
    uint8   channel;
    float32 frequency_ghz;
    uint32  num_averages;
    boolean enabled;
} TestConfig_t;

typedef struct {
    uint32      test_id;
    uint8       channel;
    float32     frequency_ghz;
    float32     measured_value;
    float32     lower_limit;
    float32     upper_limit;
    TestResult_t result;
    uint32      timestamp_ms;
} TestResultEntry_t;

#define MAX_TEST_RESULTS            64U

#endif /* RF_TEST_CFG_H */
