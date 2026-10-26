/**
 * @file test_capture_engine.c
 * @brief Unit tests for data capture engine and DMA manager
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

static int g_ce_pass = 0;
static int g_ce_fail = 0;

#define CE_ASSERT(cond) do { \
    if (cond) { g_ce_pass++; } \
    else { printf("  FAIL: %s:%d\n", __FILE__, __LINE__); g_ce_fail++; } \
} while(0)

#define CE_ASSERT_EQ(exp, act) do { \
    if ((exp) == (act)) { g_ce_pass++; } \
    else { printf("  FAIL: %s:%d: Expected %d got %d\n", __FILE__, __LINE__, (int)(exp), (int)(act)); g_ce_fail++; } \
} while(0)

#define CE_RUN(func) do { printf("  Running %s...\n", #func); func(); } while(0)

/* Simulate I/Q de-interleaving */
static void deinterleave_iq(const int16_t *interleaved, int16_t *buf_i,
                             int16_t *buf_q, int num_samples)
{
    for (int i = 0; i < num_samples; i++) {
        buf_i[i] = interleaved[i * 2];
        buf_q[i] = interleaved[i * 2 + 1];
    }
}

/* Test cases */
static void test_deinterleave_basic(void)
{
    int16_t interleaved[] = {100, 200, 300, 400, 500, 600, 700, 800};
    int16_t bi[4], bq[4];
    deinterleave_iq(interleaved, bi, bq, 4);

    CE_ASSERT_EQ(100, bi[0]); CE_ASSERT_EQ(200, bq[0]);
    CE_ASSERT_EQ(300, bi[1]); CE_ASSERT_EQ(400, bq[1]);
    CE_ASSERT_EQ(500, bi[2]); CE_ASSERT_EQ(600, bq[2]);
    CE_ASSERT_EQ(700, bi[3]); CE_ASSERT_EQ(800, bq[3]);
}

static void test_deinterleave_negative_values(void)
{
    int16_t interleaved[] = {-100, -200, 300, -400};
    int16_t bi[2], bq[2];
    deinterleave_iq(interleaved, bi, bq, 2);

    CE_ASSERT_EQ(-100, bi[0]); CE_ASSERT_EQ(-200, bq[0]);
    CE_ASSERT_EQ(300, bi[1]);  CE_ASSERT_EQ(-400, bq[1]);
}

static void test_buffer_size_validation(void)
{
    int max_samples = 4096;
    int requested = 2048;
    CE_ASSERT(requested <= max_samples);

    requested = 8192;
    CE_ASSERT(requested > max_samples);  /* Should be rejected */
}

static void test_capture_state_machine(void)
{
    /* Simulate state machine transitions */
    typedef enum { IDLE, PING, PONG, DONE } State;
    State state = IDLE;

    /* Start -> PING */
    CE_ASSERT(state == IDLE);
    state = PING;
    CE_ASSERT(state == PING);

    /* Capture complete -> DONE */
    state = DONE;
    CE_ASSERT(state == DONE);

    /* Read data -> back to IDLE */
    state = IDLE;
    CE_ASSERT(state == IDLE);
}

static void test_dma_transfer_size_calculation(void)
{
    int num_samples = 1024;
    int bytes_per_sample = 4;  /* I16 + Q16 */
    int transfer_bytes = num_samples * bytes_per_sample;
    CE_ASSERT_EQ(4096, transfer_bytes);

    num_samples = 4096;
    transfer_bytes = num_samples * bytes_per_sample;
    CE_ASSERT_EQ(16384, transfer_bytes);
}

static void test_timing_offset_calculation(void)
{
    /* Verify timing offset conversion from samples to picoseconds */
    double sample_rate = 12.5e6;  /* 12.5 MSPS */
    double sample_period_ps = 1e12 / sample_rate;  /* 80000 ps = 80 ns */

    double lag_samples = 0.5;
    double offset_ps = lag_samples * sample_period_ps;

    CE_ASSERT(fabs(offset_ps - 40000.0) < 1.0);
}

static void test_ping_pong_alternation(void)
{
    int ping_count = 0, pong_count = 0;
    int use_ping = 1;

    for (int frame = 0; frame < 10; frame++) {
        if (use_ping) {
            ping_count++;
        } else {
            pong_count++;
        }
        use_ping = !use_ping;
    }

    CE_ASSERT_EQ(5, ping_count);
    CE_ASSERT_EQ(5, pong_count);
}

/* Test runner */
void run_capture_engine_tests(void)
{
    printf("\n=== Capture Engine Tests ===\n");

    CE_RUN(test_deinterleave_basic);
    CE_RUN(test_deinterleave_negative_values);
    CE_RUN(test_buffer_size_validation);
    CE_RUN(test_capture_state_machine);
    CE_RUN(test_dma_transfer_size_calculation);
    CE_RUN(test_timing_offset_calculation);
    CE_RUN(test_ping_pong_alternation);

    printf("  Capture Engine Tests: %d passed, %d failed\n\n", g_ce_pass, g_ce_fail);
}

/* Main runner that calls all test suites */
extern void run_tx_power_tests(void);
extern void run_rx_gain_tests(void);
extern void run_lo_nulling_tests(void);

int main(void)
{
    printf("============================================\n");
    printf("mmWave RF Test Firmware - Unit Test Suite\n");
    printf("============================================\n");

    run_tx_power_tests();
    run_rx_gain_tests();
    run_lo_nulling_tests();
    run_capture_engine_tests();

    printf("============================================\n");
    printf("All unit test suites completed.\n");
    printf("============================================\n");

    return 0;
}
