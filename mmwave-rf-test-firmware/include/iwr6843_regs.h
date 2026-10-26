/**
 * @file iwr6843_regs.h
 * @brief Register definitions for IWR6843 RF subsystem
 *
 * Memory-mapped register addresses for the analog RF front-end, frequency
 * synthesizer, data path, and control subsystems of the IWR6843 mmWave SoC.
 * Addresses derived from IWR6843 Technical Reference Manual.
 */

#ifndef IWR6843_REGS_H
#define IWR6843_REGS_H

/* ========================================================================= */
/*  Base addresses                                                            */
/* ========================================================================= */
#define RF_SUBSYS_BASE          0x02000000U
#define SYNTH_BASE              (RF_SUBSYS_BASE + 0x00001000U)
#define TX_FRONTEND_BASE        (RF_SUBSYS_BASE + 0x00002000U)
#define RX_FRONTEND_BASE        (RF_SUBSYS_BASE + 0x00003000U)
#define PGA_BASE                (RF_SUBSYS_BASE + 0x00004000U)
#define IF_FILTER_BASE          (RF_SUBSYS_BASE + 0x00005000U)
#define ADC_BASE                (RF_SUBSYS_BASE + 0x00006000U)
#define DAC_BASE                (RF_SUBSYS_BASE + 0x00007000U)
#define DMA_BASE                0x02100000U
#define UART0_BASE              0x02110000U
#define OTP_BASE                0x03000000U
#define GPIO_BASE               0x02120000U

/* ========================================================================= */
/*  Frequency Synthesizer registers                                           */
/* ========================================================================= */
#define SYNTH_CTRL              (SYNTH_BASE + 0x00U)
#define SYNTH_FREQ_WORD_LO      (SYNTH_BASE + 0x04U)
#define SYNTH_FREQ_WORD_HI      (SYNTH_BASE + 0x08U)
#define SYNTH_RAMP_SLOPE        (SYNTH_BASE + 0x0CU)
#define SYNTH_RAMP_START_FREQ   (SYNTH_BASE + 0x10U)
#define SYNTH_RAMP_END_FREQ     (SYNTH_BASE + 0x14U)
#define SYNTH_PLL_DIVIDER       (SYNTH_BASE + 0x18U)
#define SYNTH_PLL_CP_CTRL       (SYNTH_BASE + 0x1CU)
#define SYNTH_PLL_LPF_CTRL      (SYNTH_BASE + 0x20U)
#define SYNTH_PLL_STATUS        (SYNTH_BASE + 0x24U)
#define SYNTH_VCO_CTRL          (SYNTH_BASE + 0x28U)
#define SYNTH_VCO_BIAS          (SYNTH_BASE + 0x2CU)
#define SYNTH_LO_DIST_CTRL      (SYNTH_BASE + 0x30U)
#define SYNTH_CHIRP_CTRL        (SYNTH_BASE + 0x34U)
#define SYNTH_CHIRP_TIMER       (SYNTH_BASE + 0x38U)
#define SYNTH_CAL_CTRL          (SYNTH_BASE + 0x3CU)

/* Synthesizer control bit definitions */
#define SYNTH_CTRL_ENABLE       BIT(0)
#define SYNTH_CTRL_PLL_EN       BIT(1)
#define SYNTH_CTRL_VCO_EN       BIT(2)
#define SYNTH_CTRL_LO_EN        BIT(3)
#define SYNTH_CTRL_CW_MODE      BIT(4)
#define SYNTH_CTRL_RAMP_EN      BIT(5)
#define SYNTH_CTRL_RESET        BIT(31)

/* PLL status bits */
#define SYNTH_PLL_LOCKED        BIT(0)
#define SYNTH_PLL_VCO_TRIM_DONE BIT(1)
#define SYNTH_PLL_FREQ_OK       BIT(2)

/* ========================================================================= */
/*  TX Frontend registers                                                     */
/* ========================================================================= */
#define TX_CTRL                 (TX_FRONTEND_BASE + 0x00U)
#define TX_CH0_POWER            (TX_FRONTEND_BASE + 0x04U)
#define TX_CH1_POWER            (TX_FRONTEND_BASE + 0x08U)
#define TX_CH2_POWER            (TX_FRONTEND_BASE + 0x0CU)
#define TX_CH0_PHASE            (TX_FRONTEND_BASE + 0x10U)
#define TX_CH1_PHASE            (TX_FRONTEND_BASE + 0x14U)
#define TX_CH2_PHASE            (TX_FRONTEND_BASE + 0x18U)
#define TX_PA_BIAS              (TX_FRONTEND_BASE + 0x1CU)
#define TX_PA_EN                (TX_FRONTEND_BASE + 0x20U)
#define TX_LB_CTRL              (TX_FRONTEND_BASE + 0x24U)
#define TX_LB_ATTEN             (TX_FRONTEND_BASE + 0x28U)
#define TX_PS_CTRL              (TX_FRONTEND_BASE + 0x2CU)
#define TX_BIST_CTRL            (TX_FRONTEND_BASE + 0x30U)

/* TX control bits */
#define TX_CTRL_CH0_EN          BIT(0)
#define TX_CTRL_CH1_EN          BIT(1)
#define TX_CTRL_CH2_EN          BIT(2)
#define TX_CTRL_ALL_CH_EN       (TX_CTRL_CH0_EN | TX_CTRL_CH1_EN | TX_CTRL_CH2_EN)
#define TX_CTRL_TONE_EN         BIT(4)
#define TX_CTRL_LOOPBACK_EN     BIT(5)

/* TX power register fields */
#define TX_POWER_CODE_MASK      0x3FU
#define TX_POWER_CODE_SHIFT     0U
#define TX_POWER_BACKOFF_MASK   0x0700U
#define TX_POWER_BACKOFF_SHIFT  8U

/* ========================================================================= */
/*  RX Frontend registers                                                     */
/* ========================================================================= */
#define RX_CTRL                 (RX_FRONTEND_BASE + 0x00U)
#define RX_CH0_GAIN             (RX_FRONTEND_BASE + 0x04U)
#define RX_CH1_GAIN             (RX_FRONTEND_BASE + 0x08U)
#define RX_CH2_GAIN             (RX_FRONTEND_BASE + 0x0CU)
#define RX_CH3_GAIN             (RX_FRONTEND_BASE + 0x10U)
#define RX_LNA_CTRL             (RX_FRONTEND_BASE + 0x14U)
#define RX_MIXER_CTRL           (RX_FRONTEND_BASE + 0x18U)
#define RX_HPF_CTRL             (RX_FRONTEND_BASE + 0x1CU)
#define RX_LPF_CTRL             (RX_FRONTEND_BASE + 0x20U)
#define RX_DC_OFFSET_I          (RX_FRONTEND_BASE + 0x24U)
#define RX_DC_OFFSET_Q          (RX_FRONTEND_BASE + 0x28U)
#define RX_IQ_MISMATCH          (RX_FRONTEND_BASE + 0x2CU)

/* RX control bits */
#define RX_CTRL_CH0_EN          BIT(0)
#define RX_CTRL_CH1_EN          BIT(1)
#define RX_CTRL_CH2_EN          BIT(2)
#define RX_CTRL_CH3_EN          BIT(3)
#define RX_CTRL_ALL_CH_EN       0x0FU
#define RX_CTRL_LNA_BYPASS      BIT(4)
#define RX_CTRL_MIXER_EN        BIT(5)

/* LNA gain settings */
#define RX_LNA_GAIN_LOW         0x00U
#define RX_LNA_GAIN_MED         0x01U
#define RX_LNA_GAIN_HIGH        0x02U
#define RX_LNA_GAIN_MAX         0x03U

/* ========================================================================= */
/*  PGA (Programmable Gain Amplifier) registers                               */
/* ========================================================================= */
#define PGA_CTRL                (PGA_BASE + 0x00U)
#define PGA_CH0_GAIN            (PGA_BASE + 0x04U)
#define PGA_CH1_GAIN            (PGA_BASE + 0x08U)
#define PGA_CH2_GAIN            (PGA_BASE + 0x0CU)
#define PGA_CH3_GAIN            (PGA_BASE + 0x10U)
#define PGA_BIAS_CTRL           (PGA_BASE + 0x14U)
#define PGA_OFFSET_CH0          (PGA_BASE + 0x18U)
#define PGA_OFFSET_CH1          (PGA_BASE + 0x1CU)
#define PGA_OFFSET_CH2          (PGA_BASE + 0x20U)
#define PGA_OFFSET_CH3          (PGA_BASE + 0x24U)

#define PGA_CTRL_ENABLE         BIT(0)
#define PGA_GAIN_MASK           0x07U
#define PGA_GAIN_24DB           0x00U
#define PGA_GAIN_30DB           0x01U
#define PGA_GAIN_36DB           0x02U
#define PGA_GAIN_42DB           0x03U
#define PGA_GAIN_48DB           0x04U

/* ========================================================================= */
/*  IF Filter registers                                                       */
/* ========================================================================= */
#define IF_FILT_CTRL            (IF_FILTER_BASE + 0x00U)
#define IF_FILT_BW              (IF_FILTER_BASE + 0x04U)
#define IF_FILT_HPF_CORNER      (IF_FILTER_BASE + 0x08U)
#define IF_FILT_LPF_CORNER      (IF_FILTER_BASE + 0x0CU)

#define IF_FILT_ENABLE          BIT(0)
#define IF_HPF_175KHZ           0x00U
#define IF_HPF_235KHZ           0x01U
#define IF_HPF_350KHZ           0x02U
#define IF_HPF_700KHZ           0x03U
#define IF_LPF_2500KHZ          0x00U
#define IF_LPF_5000KHZ          0x01U
#define IF_LPF_10000KHZ         0x02U
#define IF_LPF_20000KHZ         0x03U

/* ========================================================================= */
/*  ADC registers                                                             */
/* ========================================================================= */
#define ADC_CTRL                (ADC_BASE + 0x00U)
#define ADC_CONFIG              (ADC_BASE + 0x04U)
#define ADC_SAMPLE_RATE         (ADC_BASE + 0x08U)
#define ADC_NUM_SAMPLES         (ADC_BASE + 0x0CU)
#define ADC_DATA_REG            (ADC_BASE + 0x10U)
#define ADC_STATUS              (ADC_BASE + 0x14U)
#define ADC_BUF_ADDR            (ADC_BASE + 0x18U)
#define ADC_BUF_SIZE            (ADC_BASE + 0x1CU)
#define ADC_INTR_CTRL           (ADC_BASE + 0x20U)

#define ADC_CTRL_ENABLE         BIT(0)
#define ADC_CTRL_START          BIT(1)
#define ADC_CTRL_CONTINUOUS     BIT(2)
#define ADC_CTRL_DMA_EN         BIT(3)
#define ADC_STATUS_DONE         BIT(0)
#define ADC_STATUS_OVERFLOW     BIT(1)

/* ADC resolution */
#define ADC_RES_12BIT           0x00U
#define ADC_RES_14BIT           0x01U
#define ADC_RES_16BIT           0x02U

/* ADC format */
#define ADC_FMT_REAL            0x00U
#define ADC_FMT_COMPLEX         0x01U
#define ADC_FMT_INTERLEAVED     0x02U

/* ========================================================================= */
/*  DAC registers                                                             */
/* ========================================================================= */
#define DAC_CTRL                (DAC_BASE + 0x00U)
#define DAC_DATA                (DAC_BASE + 0x04U)
#define DAC_CAL_CTRL            (DAC_BASE + 0x08U)
#define DAC_CAL_DATA            (DAC_BASE + 0x0CU)
#define DAC_STATUS              (DAC_BASE + 0x10U)

#define DAC_CTRL_ENABLE         BIT(0)
#define DAC_CTRL_UPDATE         BIT(1)

/* ========================================================================= */
/*  DMA registers                                                             */
/* ========================================================================= */
#define DMA_CTRL                (DMA_BASE + 0x00U)
#define DMA_STATUS              (DMA_BASE + 0x04U)
#define DMA_CH0_SRC             (DMA_BASE + 0x10U)
#define DMA_CH0_DST             (DMA_BASE + 0x14U)
#define DMA_CH0_SIZE            (DMA_BASE + 0x18U)
#define DMA_CH0_CTRL            (DMA_BASE + 0x1CU)
#define DMA_INTR_STATUS         (DMA_BASE + 0x40U)
#define DMA_INTR_CLEAR          (DMA_BASE + 0x44U)
#define DMA_CH_STRIDE           0x10U

#define DMA_CTRL_ENABLE         BIT(0)
#define DMA_CH_CTRL_START       BIT(0)
#define DMA_CH_CTRL_CIRCULAR    BIT(1)
#define DMA_CH_CTRL_INTR_EN     BIT(2)
#define DMA_CH_CTRL_DONE        BIT(31)

/* ========================================================================= */
/*  UART registers                                                            */
/* ========================================================================= */
#define UART_THR                (UART0_BASE + 0x00U)
#define UART_RBR                (UART0_BASE + 0x00U)
#define UART_IER                (UART0_BASE + 0x04U)
#define UART_FCR                (UART0_BASE + 0x08U)
#define UART_LCR                (UART0_BASE + 0x0CU)
#define UART_MCR                (UART0_BASE + 0x10U)
#define UART_LSR                (UART0_BASE + 0x14U)
#define UART_DLL                (UART0_BASE + 0x20U)
#define UART_DLH                (UART0_BASE + 0x24U)

#define UART_LSR_DR             BIT(0)  /* Data Ready */
#define UART_LSR_THRE           BIT(5)  /* THR Empty */
#define UART_LSR_TEMT           BIT(6)  /* Transmitter Empty */

/* ========================================================================= */
/*  OTP / Factory Trim registers                                              */
/* ========================================================================= */
#define OTP_CTRL                (OTP_BASE + 0x00U)
#define OTP_ADDR                (OTP_BASE + 0x04U)
#define OTP_DATA                (OTP_BASE + 0x08U)
#define OTP_STATUS              (OTP_BASE + 0x0CU)

#define OTP_CTRL_READ           BIT(0)
#define OTP_STATUS_DONE         BIT(0)
#define OTP_STATUS_ERROR        BIT(1)

/* OTP memory offsets for factory trim values */
#define OTP_TRIM_BASE_ADDR      0x0100U
#define OTP_TRIM_SYNTH_OFFSET   0x0000U
#define OTP_TRIM_TX_OFFSET      0x0020U
#define OTP_TRIM_RX_OFFSET      0x0040U
#define OTP_TRIM_PGA_OFFSET     0x0060U
#define OTP_TRIM_ADC_OFFSET     0x0080U
#define OTP_TRIM_TEMP_OFFSET    0x00A0U
#define OTP_TRIM_LOT_ID_OFFSET  0x00C0U
#define OTP_TRIM_WAFER_OFFSET   0x00D0U
#define OTP_TRIM_CHECKSUM       0x00FCU

/* ========================================================================= */
/*  GPIO registers                                                            */
/* ========================================================================= */
#define GPIO_DATA_OUT           (GPIO_BASE + 0x00U)
#define GPIO_DATA_IN            (GPIO_BASE + 0x04U)
#define GPIO_DIR                (GPIO_BASE + 0x08U)
#define GPIO_SET                (GPIO_BASE + 0x0CU)
#define GPIO_CLR                (GPIO_BASE + 0x10U)

#define GPIO_PIN_LED_PASS       BIT(0)
#define GPIO_PIN_LED_FAIL       BIT(1)
#define GPIO_PIN_TEST_TRIGGER   BIT(2)

#endif /* IWR6843_REGS_H */
