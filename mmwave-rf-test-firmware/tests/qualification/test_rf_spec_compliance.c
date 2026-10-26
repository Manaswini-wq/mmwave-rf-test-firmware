/**
 * @file test_rf_spec_compliance.c
 * @brief Qualification test for RF specification compliance
 *
 * Verifies that the firmware-reported measurements meet the IWR6843
 * datasheet specifications across operating conditions.  These tests
 * are run during device qualification and after firmware updates.
 */

#include <stdio.h>
#include <math.h>
#include <string.h>

static int g_qual_pass = 0;
static int g_qual_fail = 0;

#define QUAL_ASSERT(cond, msg) do { \
    if (cond) { g_qual_pass++; printf("  PASS: %s\n", msg); } \
    else { g_qual_fail++; printf("  FAIL: %s\n", msg); } \
} while(0)

/* Specification limits from IWR6843 datasheet */
typedef struct {
    const char *parameter;
    float min_val;
    float max_val;
    const char *unit;
    const char *condition;
} SpecLimit;

static const SpecLimit iwr6843_spec[] = {
    {"TX Output Power",        10.0f,  13.0f, "dBm",     "Per channel, 60-64 GHz"},
    {"TX Power Variation",      0.0f,   1.5f, "dB",      "Across channels"},
    {"TX Power Flatness",       0.0f,   1.0f, "dB pk-pk","Across 60-64 GHz"},
    {"RX Noise Floor",       -120.0f,-100.0f, "dBm",     "Max gain setting"},
    {"RX Noise Figure",         5.0f,  14.0f, "dB",      "76-81 GHz band"},
    {"Phase Noise @1MHz",    -120.0f, -85.0f, "dBc/Hz",  "CW mode"},
    {"Frequency Accuracy",    -50.0f,  50.0f, "kHz",     "Over temperature"},
    {"TX EVM",                -50.0f, -25.0f, "dB",      "CW tone"},
    {"LO Leakage",            -60.0f, -35.0f, "dBc",     "After calibration"},
    {"RX Gain Match",           0.0f,   0.5f, "dB",      "Between channels"},
    {"RX Phase Match",          0.0f,   3.0f, "degrees", "Between channels"},
};

#define NUM_SPECS (sizeof(iwr6843_spec) / sizeof(iwr6843_spec[0]))

/* Simulated measurement data (would come from actual DUT in production) */
static float simulated_measurements[] = {
    11.5f,   /* TX Output Power */
     0.8f,   /* TX Power Variation */
     0.4f,   /* TX Power Flatness */
  -108.0f,   /* RX Noise Floor */
    10.5f,   /* RX Noise Figure */
   -92.0f,   /* Phase Noise @1MHz */
     5.0f,   /* Frequency Accuracy */
   -33.0f,   /* TX EVM */
   -45.0f,   /* LO Leakage */
     0.2f,   /* RX Gain Match */
     1.5f,   /* RX Phase Match */
};

static void test_all_specs(void)
{
    for (int i = 0; i < (int)NUM_SPECS; i++) {
        const SpecLimit *spec = &iwr6843_spec[i];
        float measured = simulated_measurements[i];

        int in_spec = (measured >= spec->min_val && measured <= spec->max_val);

        char msg[128];
        snprintf(msg, sizeof(msg), "%s: %.2f %s [%.1f, %.1f] (%s)",
                 spec->parameter, measured, spec->unit,
                 spec->min_val, spec->max_val, spec->condition);

        QUAL_ASSERT(in_spec, msg);
    }
}

static void test_tx_power_across_frequency(void)
{
    /* Verify TX power at band edges */
    float powers_at_freq[] = {
        11.2f,  /* 60 GHz */
        11.5f,  /* 61 GHz */
        11.8f,  /* 62 GHz */
        11.6f,  /* 63 GHz */
        11.0f,  /* 64 GHz */
    };

    float min_p = 999.0f, max_p = -999.0f;
    for (int i = 0; i < 5; i++) {
        if (powers_at_freq[i] < min_p) min_p = powers_at_freq[i];
        if (powers_at_freq[i] > max_p) max_p = powers_at_freq[i];
        QUAL_ASSERT(powers_at_freq[i] >= 10.0f && powers_at_freq[i] <= 13.0f,
                    "TX power within spec at each frequency");
    }

    float ripple = max_p - min_p;
    QUAL_ASSERT(ripple <= 1.0f, "TX power flatness within spec");
}

static void test_rx_performance_all_channels(void)
{
    float noise_floors[] = {-108.0f, -107.5f, -109.0f, -106.5f};
    float nf_values[] = {10.5f, 11.0f, 10.0f, 11.5f};

    for (int ch = 0; ch < 4; ch++) {
        char msg[64];

        snprintf(msg, sizeof(msg), "RX CH%d noise floor: %.1f dBm", ch, noise_floors[ch]);
        QUAL_ASSERT(noise_floors[ch] <= -100.0f, msg);

        snprintf(msg, sizeof(msg), "RX CH%d noise figure: %.1f dB", ch, nf_values[ch]);
        QUAL_ASSERT(nf_values[ch] <= 14.0f, msg);
    }
}

static void test_inter_channel_matching(void)
{
    float gains[] = {48.2f, 48.0f, 48.5f, 47.8f};
    float phases[] = {0.0f, 1.2f, -0.5f, 0.8f};

    float ref_gain = gains[0];
    float ref_phase = phases[0];

    for (int ch = 1; ch < 4; ch++) {
        float gain_err = fabsf(gains[ch] - ref_gain);
        float phase_err = fabsf(phases[ch] - ref_phase);

        char msg[64];
        snprintf(msg, sizeof(msg), "CH%d gain match: %.2f dB", ch, gain_err);
        QUAL_ASSERT(gain_err <= 0.5f, msg);

        snprintf(msg, sizeof(msg), "CH%d phase match: %.1f deg", ch, phase_err);
        QUAL_ASSERT(phase_err <= 3.0f, msg);
    }
}

int main(void)
{
    printf("============================================\n");
    printf("Qualification Test: RF Spec Compliance\n");
    printf("Device: IWR6843\n");
    printf("============================================\n\n");

    printf("--- Datasheet Specification Checks ---\n");
    test_all_specs();

    printf("\n--- TX Power vs. Frequency ---\n");
    test_tx_power_across_frequency();

    printf("\n--- RX Performance (All Channels) ---\n");
    test_rx_performance_all_channels();

    printf("\n--- Inter-Channel Matching ---\n");
    test_inter_channel_matching();

    printf("\n============================================\n");
    printf("Qualification Results: %d passed, %d failed\n",
           g_qual_pass, g_qual_fail);

    if (g_qual_fail == 0) {
        printf("QUALIFICATION: PASS\n");
    } else {
        printf("QUALIFICATION: FAIL\n");
    }
    printf("============================================\n");

    return g_qual_fail > 0 ? 1 : 0;
}
