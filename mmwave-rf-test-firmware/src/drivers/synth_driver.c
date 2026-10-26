/**
 * @file synth_driver.c
 * @brief Frequency synthesizer driver for IWR6843 RF PLL
 *
 * Controls the on-chip fractional-N PLL frequency synthesizer.  Programs
 * the VCO, charge pump, loop filter, and frequency dividers to generate
 * the desired LO frequency for both CW and FMCW chirp modes.
 */

#include "drivers/synth_driver.h"
#include "iwr6843_regs.h"
#include "rf_test_cfg.h"

/* ========================================================================= */
/*  Internal state                                                            */
/* ========================================================================= */
static boolean g_synth_initialized = FALSE;
static float32 g_current_freq_ghz  = 0.0f;

/* ========================================================================= */
/*  Internal helpers                                                          */
/* ========================================================================= */

/**
 * @brief Compute PLL frequency word from desired frequency
 *
 * Freq_word = (freq_ghz * 2^24) / ref_clk_ghz
 * For 40 MHz ref clock: ref_clk_ghz = 0.040
 */
static uint64 compute_freq_word(float32 freq_ghz)
{
    float64 ref_ghz = (float64)SYNTH_REF_CLK_MHZ / 1000.0;
    float64 word    = ((float64)freq_ghz / ref_ghz) * (float64)(1ULL << 24);
    return (uint64)word;
}

/**
 * @brief Program charge pump current based on loop bandwidth
 */
static void configure_charge_pump(void)
{
    /* CP current for 300 kHz loop BW with typical VCO gain */
    uint32 cp_code = 0x0CU;  /* ~150 uA */
    REG_WR32(SYNTH_PLL_CP_CTRL, cp_code);
}

/**
 * @brief Configure PLL loop filter coefficients
 */
static void configure_loop_filter(void)
{
    /*
     * 3rd-order loop filter:
     *   C1 = 22 pF, C2 = 470 pF, C3 = 4.7 pF
     *   R2 = 6.8 kOhm, R3 = 22 kOhm
     *   Register encoding for on-chip programmable filter.
     */
    uint32 lpf_cfg = (0x03U << 0) |   /* C1 select */
                     (0x05U << 4) |   /* C2 select */
                     (0x02U << 8) |   /* R2 select */
                     (0x04U << 12);   /* R3 select */
    REG_WR32(SYNTH_PLL_LPF_CTRL, lpf_cfg);
}

/**
 * @brief Configure VCO bias and calibration
 */
static void configure_vco(void)
{
    /* Set VCO bias current */
    REG_WR32(SYNTH_VCO_BIAS, 0x08U);  /* Mid-range bias */

    /* Enable VCO auto-calibration */
    REG_WR32(SYNTH_VCO_CTRL, BIT(0) | BIT(1));  /* Enable + auto-cal */
    DELAY_US(50U);
}

/* ========================================================================= */
/*  Public API                                                                */
/* ========================================================================= */

Status_t synth_init(void)
{
    /* Reset the synthesizer block */
    REG_WR32(SYNTH_CTRL, SYNTH_CTRL_RESET);
    DELAY_US(10U);
    REG_WR32(SYNTH_CTRL, 0U);
    DELAY_US(10U);

    /* Configure sub-blocks */
    configure_charge_pump();
    configure_loop_filter();
    configure_vco();

    /* Configure LO distribution network */
    REG_WR32(SYNTH_LO_DIST_CTRL, 0x0FU);  /* Enable LO to all TX/RX paths */

    g_synth_initialized = TRUE;
    g_current_freq_ghz  = 0.0f;

    return STATUS_OK;
}

Status_t synth_set_frequency_ghz(float32 freq_ghz)
{
    if (!g_synth_initialized) return STATUS_NOT_INIT;

    uint64 freq_word = compute_freq_word(freq_ghz);

    /* Program frequency word (48-bit split across two registers) */
    REG_WR32(SYNTH_FREQ_WORD_LO, (uint32)(freq_word & 0xFFFFFFFFULL));
    REG_WR32(SYNTH_FREQ_WORD_HI, (uint32)((freq_word >> 32) & 0xFFFFU));

    g_current_freq_ghz = freq_ghz;
    return STATUS_OK;
}

Status_t synth_enable_cw_mode(void)
{
    if (!g_synth_initialized) return STATUS_NOT_INIT;

    uint32 ctrl = SYNTH_CTRL_ENABLE | SYNTH_CTRL_PLL_EN |
                  SYNTH_CTRL_VCO_EN | SYNTH_CTRL_LO_EN |
                  SYNTH_CTRL_CW_MODE;
    REG_WR32(SYNTH_CTRL, ctrl);

    return STATUS_OK;
}

Status_t synth_enable_chirp_mode(float32 start_ghz, float32 slope_mhz_us, float32 idle_us)
{
    if (!g_synth_initialized) return STATUS_NOT_INIT;

    /* Program chirp parameters */
    synth_set_frequency_ghz(start_ghz);

    /* Slope register: MHz/us encoded as fixed-point */
    uint32 slope_code = (uint32)(slope_mhz_us * 256.0f);
    REG_WR32(SYNTH_RAMP_SLOPE, slope_code);

    /* Chirp timing */
    uint32 idle_cycles = (uint32)(idle_us * (SYNTH_REF_CLK_MHZ));
    REG_WR32(SYNTH_CHIRP_TIMER, idle_cycles);

    /* Enable chirp/ramp mode */
    uint32 ctrl = SYNTH_CTRL_ENABLE | SYNTH_CTRL_PLL_EN |
                  SYNTH_CTRL_VCO_EN | SYNTH_CTRL_LO_EN |
                  SYNTH_CTRL_RAMP_EN;
    REG_WR32(SYNTH_CTRL, ctrl);

    return STATUS_OK;
}

Status_t synth_wait_lock(uint32 timeout_us)
{
    uint32 elapsed = 0U;

    while (elapsed < timeout_us) {
        if (BIT_TST(REG_RD32(SYNTH_PLL_STATUS), SYNTH_PLL_LOCKED)) {
            return STATUS_OK;
        }
        DELAY_US(1U);
        elapsed++;
    }

    return STATUS_TIMEOUT;
}

void synth_disable(void)
{
    REG_WR32(SYNTH_CTRL, 0U);
    g_current_freq_ghz = 0.0f;
}

boolean synth_is_locked(void)
{
    return BIT_TST(REG_RD32(SYNTH_PLL_STATUS), SYNTH_PLL_LOCKED);
}

float32 synth_get_frequency_ghz(void)
{
    return g_current_freq_ghz;
}
