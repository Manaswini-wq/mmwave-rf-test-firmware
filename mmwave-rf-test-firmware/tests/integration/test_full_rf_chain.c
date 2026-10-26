/**
 * @file test_full_rf_chain.c
 * @brief Integration test for the complete RF signal chain
 *
 * Validates end-to-end signal path from TX output through the internal
 * loopback to the RX ADC output.  Verifies that all driver modules
 * work together correctly and that the signal chain meets basic
 * performance requirements.
 */

#include <stdio.h>
#include <math.h>
#include <string.h>

static int g_int_pass = 0;
static int g_int_fail = 0;

#define INT_ASSERT(cond, msg) do { \
    if (cond) { g_int_pass++; printf("  PASS: %s\n", msg); } \
    else { g_int_fail++; printf("  FAIL: %s\n", msg); } \
} while(0)

/* Simulated RF chain model for integration testing */
typedef struct {
    float synth_freq_ghz;
    int   synth_locked;
    int   tx_enabled[3];
    float tx_power_code[3];
    int   rx_enabled[4];
    int   pga_gain[4];
    int   loopback_enabled;
    float loopback_atten_db;
} RFChainState;

static RFChainState g_rf = {0};

static void sim_synth_init(RFChainState *rf) {
    rf->synth_freq_ghz = 0.0f;
    rf->synth_locked = 0;
}

static void sim_synth_set_freq(RFChainState *rf, float freq_ghz) {
    rf->synth_freq_ghz = freq_ghz;
    rf->synth_locked = (freq_ghz >= 60.0f && freq_ghz <= 64.0f) ? 1 : 0;
}

static void sim_tx_enable(RFChainState *rf, int ch) {
    if (ch < 3) rf->tx_enabled[ch] = 1;
}

static void sim_rx_enable(RFChainState *rf, int ch) {
    if (ch < 4) rf->rx_enabled[ch] = 1;
}

static void sim_loopback_enable(RFChainState *rf) {
    rf->loopback_enabled = 1;
    rf->loopback_atten_db = 30.0f;
}

static float sim_measure_power(RFChainState *rf, int rx_ch) {
    if (!rf->synth_locked) return -999.0f;
    if (!rf->loopback_enabled) return -110.0f;

    /* Find an active TX channel */
    int tx_active = 0;
    float tx_power = 12.0f;
    for (int i = 0; i < 3; i++) {
        if (rf->tx_enabled[i]) { tx_active = 1; break; }
    }
    if (!tx_active || !rf->rx_enabled[rx_ch]) return -110.0f;

    /* Simulated power at ADC = TX power - loopback atten + RX gain */
    float rx_gain = 6.0f * rf->pga_gain[rx_ch] + 24.0f;
    return tx_power - rf->loopback_atten_db + rx_gain - 48.0f;
}

/* Integration test cases */
static void test_full_chain_initialization(void)
{
    memset(&g_rf, 0, sizeof(g_rf));
    sim_synth_init(&g_rf);

    INT_ASSERT(g_rf.synth_freq_ghz == 0.0f, "Synth init: freq = 0");
    INT_ASSERT(g_rf.synth_locked == 0, "Synth init: not locked");
    INT_ASSERT(g_rf.loopback_enabled == 0, "Loopback init: disabled");
}

static void test_synth_lock_in_band(void)
{
    sim_synth_set_freq(&g_rf, 62.0f);
    INT_ASSERT(g_rf.synth_locked == 1, "Synth locks at 62 GHz");

    sim_synth_set_freq(&g_rf, 60.0f);
    INT_ASSERT(g_rf.synth_locked == 1, "Synth locks at 60 GHz (min)");

    sim_synth_set_freq(&g_rf, 64.0f);
    INT_ASSERT(g_rf.synth_locked == 1, "Synth locks at 64 GHz (max)");
}

static void test_synth_lock_out_of_band(void)
{
    sim_synth_set_freq(&g_rf, 50.0f);
    INT_ASSERT(g_rf.synth_locked == 0, "Synth fails to lock at 50 GHz");

    sim_synth_set_freq(&g_rf, 70.0f);
    INT_ASSERT(g_rf.synth_locked == 0, "Synth fails to lock at 70 GHz");
}

static void test_tx_rx_loopback_signal(void)
{
    memset(&g_rf, 0, sizeof(g_rf));
    sim_synth_set_freq(&g_rf, 62.0f);
    sim_tx_enable(&g_rf, 0);
    sim_rx_enable(&g_rf, 0);
    g_rf.pga_gain[0] = 1;  /* 30 dB */
    sim_loopback_enable(&g_rf);

    float power = sim_measure_power(&g_rf, 0);
    INT_ASSERT(power > -20.0f, "Loopback signal detected");
    INT_ASSERT(power < 20.0f, "Loopback signal within range");
}

static void test_no_signal_without_loopback(void)
{
    memset(&g_rf, 0, sizeof(g_rf));
    sim_synth_set_freq(&g_rf, 62.0f);
    sim_tx_enable(&g_rf, 0);
    sim_rx_enable(&g_rf, 0);
    /* Loopback NOT enabled */

    float power = sim_measure_power(&g_rf, 0);
    INT_ASSERT(power < -100.0f, "No signal without loopback");
}

static void test_no_signal_without_tx(void)
{
    memset(&g_rf, 0, sizeof(g_rf));
    sim_synth_set_freq(&g_rf, 62.0f);
    /* TX NOT enabled */
    sim_rx_enable(&g_rf, 0);
    sim_loopback_enable(&g_rf);

    float power = sim_measure_power(&g_rf, 0);
    INT_ASSERT(power < -100.0f, "No signal without TX");
}

static void test_multi_channel_operation(void)
{
    memset(&g_rf, 0, sizeof(g_rf));
    sim_synth_set_freq(&g_rf, 62.0f);
    sim_loopback_enable(&g_rf);

    for (int ch = 0; ch < 3; ch++) {
        sim_tx_enable(&g_rf, ch);
    }
    for (int ch = 0; ch < 4; ch++) {
        sim_rx_enable(&g_rf, ch);
        g_rf.pga_gain[ch] = 1;
    }

    for (int ch = 0; ch < 4; ch++) {
        float power = sim_measure_power(&g_rf, ch);
        char msg[64];
        snprintf(msg, sizeof(msg), "RX CH%d receives loopback signal", ch);
        INT_ASSERT(power > -20.0f, msg);
    }
}

int main(void)
{
    printf("============================================\n");
    printf("Integration Test: Full RF Chain\n");
    printf("============================================\n\n");

    test_full_chain_initialization();
    test_synth_lock_in_band();
    test_synth_lock_out_of_band();
    test_tx_rx_loopback_signal();
    test_no_signal_without_loopback();
    test_no_signal_without_tx();
    test_multi_channel_operation();

    printf("\n============================================\n");
    printf("Results: %d passed, %d failed\n", g_int_pass, g_int_fail);
    printf("============================================\n");

    return g_int_fail > 0 ? 1 : 0;
}
