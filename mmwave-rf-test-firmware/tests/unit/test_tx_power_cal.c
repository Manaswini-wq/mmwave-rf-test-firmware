/**
 * @file test_tx_power_cal.c
 * @brief Unit tests for TX power measurement and calibration
 *
 * Uses a simple Unity-style test framework for host-side unit testing.
 * Tests validate the power computation algorithm, DAC code adjustment
 * logic, and limit checking without requiring actual hardware.
 */

#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <string.h>

/* Simple test framework macros */
static int g_test_pass = 0;
static int g_test_fail = 0;

#define TEST_ASSERT_FLOAT_WITHIN(delta, expected, actual) do { \
    if (fabs((double)(expected) - (double)(actual)) <= (double)(delta)) { \
        g_test_pass++; \
    } else { \
        printf("  FAIL: %s:%d: Expected %.4f +/- %.4f, got %.4f\n", \
               __FILE__, __LINE__, (double)(expected), (double)(delta), (double)(actual)); \
        g_test_fail++; \
    } \
} while(0)

#define TEST_ASSERT_TRUE(cond) do { \
    if (cond) { g_test_pass++; } \
    else { printf("  FAIL: %s:%d: Condition false\n", __FILE__, __LINE__); g_test_fail++; } \
} while(0)

#define TEST_ASSERT_EQUAL_INT(expected, actual) do { \
    if ((expected) == (actual)) { g_test_pass++; } \
    else { printf("  FAIL: %s:%d: Expected %d, got %d\n", __FILE__, __LINE__, (int)(expected), (int)(actual)); g_test_fail++; } \
} while(0)

#define RUN_TEST(func) do { \
    printf("  Running %s...\n", #func); \
    func(); \
} while(0)

/* ========================================================================= */
/*  Test helpers - simulate hardware for host testing                         */
/* ========================================================================= */

/* Simulated ADC capture data */
static void generate_test_tone(int16_t *buf_i, int16_t *buf_q, int n,
                                double amplitude, double freq_hz, double fs_hz)
{
    for (int i = 0; i < n; i++) {
        double t = (double)i / fs_hz;
        double phase = 2.0 * M_PI * freq_hz * t;
        buf_i[i] = (int16_t)(amplitude * cos(phase));
        buf_q[i] = (int16_t)(amplitude * sin(phase));
    }
}

/* Simplified power computation (mirrors firmware algorithm) */
static float compute_power_dbm_host(const int16_t *si, const int16_t *sq,
                                     int n, float lb_atten_db)
{
    double sum_sq = 0.0;
    for (int i = 0; i < n; i++) {
        sum_sq += (double)si[i] * si[i] + (double)sq[i] * sq[i];
    }
    double rms_sq = sum_sq / (double)n;
    double fs = 32768.0 * 32768.0;
    double dbfs = 10.0 * log10(rms_sq / fs + 1e-20);
    return (float)(dbfs + 4.0 + (double)lb_atten_db);
}

/* ========================================================================= */
/*  Test cases                                                                */
/* ========================================================================= */

static void test_power_computation_full_scale(void)
{
    /* Full-scale sine wave should give ~4 dBm + loopback atten */
    int16_t buf_i[1024], buf_q[1024];
    generate_test_tone(buf_i, buf_q, 1024, 32000.0, 1e6, 12.5e6);

    float power = compute_power_dbm_host(buf_i, buf_q, 1024, 30.0f);

    /* Expected: ~4 dBm (ADC FS) + 30 dB (loopback) - 3 dB (half power of sine) = ~31 dBm */
    TEST_ASSERT_FLOAT_WITHIN(2.0f, 31.0f, power);
}

static void test_power_computation_reduced_amplitude(void)
{
    int16_t buf_i[1024], buf_q[1024];
    /* -20 dB below full scale */
    generate_test_tone(buf_i, buf_q, 1024, 3200.0, 1e6, 12.5e6);

    float power = compute_power_dbm_host(buf_i, buf_q, 1024, 30.0f);

    /* Should be ~20 dB below full-scale case */
    TEST_ASSERT_FLOAT_WITHIN(2.0f, 11.0f, power);
}

static void test_power_computation_zero_signal(void)
{
    int16_t buf_i[1024], buf_q[1024];
    memset(buf_i, 0, sizeof(buf_i));
    memset(buf_q, 0, sizeof(buf_q));

    float power = compute_power_dbm_host(buf_i, buf_q, 1024, 30.0f);

    /* Should be very low (limited by epsilon in log) */
    TEST_ASSERT_TRUE(power < -100.0f);
}

static void test_power_limit_check_pass(void)
{
    /* Simulated measurement within limits */
    float measured = 11.5f;
    float lower = 10.0f;
    float upper = 13.0f;
    int pass = (measured >= lower && measured <= upper) ? 1 : 0;
    TEST_ASSERT_EQUAL_INT(1, pass);
}

static void test_power_limit_check_fail_low(void)
{
    float measured = 8.0f;
    float lower = 10.0f;
    float upper = 13.0f;
    int pass = (measured >= lower && measured <= upper) ? 1 : 0;
    TEST_ASSERT_EQUAL_INT(0, pass);
}

static void test_power_limit_check_fail_high(void)
{
    float measured = 15.0f;
    float lower = 10.0f;
    float upper = 13.0f;
    int pass = (measured >= lower && measured <= upper) ? 1 : 0;
    TEST_ASSERT_EQUAL_INT(0, pass);
}

static void test_dac_code_clamping(void)
{
    /* Verify DAC code stays within valid range */
    int16_t code = 50;
    int16_t max_code = 63;

    if (code > max_code) code = max_code;
    if (code < 0) code = 0;
    TEST_ASSERT_EQUAL_INT(50, code);

    code = 70;
    if (code > max_code) code = max_code;
    TEST_ASSERT_EQUAL_INT(63, code);

    code = -5;
    if (code < 0) code = 0;
    TEST_ASSERT_EQUAL_INT(0, code);
}

static void test_channel_variation(void)
{
    /* Simulate 3-channel power measurements */
    float powers[] = {11.2f, 11.8f, 11.0f};
    float min_p = 999.0f, max_p = -999.0f;

    for (int i = 0; i < 3; i++) {
        if (powers[i] < min_p) min_p = powers[i];
        if (powers[i] > max_p) max_p = powers[i];
    }

    float variation = max_p - min_p;
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 0.8f, variation);
    TEST_ASSERT_TRUE(variation < 1.5f);  /* Within spec */
}

/* ========================================================================= */
/*  Test runner                                                               */
/* ========================================================================= */

void run_tx_power_tests(void)
{
    printf("\n=== TX Power Calibration Tests ===\n");

    RUN_TEST(test_power_computation_full_scale);
    RUN_TEST(test_power_computation_reduced_amplitude);
    RUN_TEST(test_power_computation_zero_signal);
    RUN_TEST(test_power_limit_check_pass);
    RUN_TEST(test_power_limit_check_fail_low);
    RUN_TEST(test_power_limit_check_fail_high);
    RUN_TEST(test_dac_code_clamping);
    RUN_TEST(test_channel_variation);

    printf("  TX Power Tests: %d passed, %d failed\n\n", g_test_pass, g_test_fail);
}

#ifdef STANDALONE_TEST
int main(void) {
    run_tx_power_tests();
    return g_test_fail > 0 ? 1 : 0;
}
#endif
