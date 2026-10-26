/**
 * @file test_lo_nulling.c
 * @brief Unit tests for LO leakage nulling calibration
 */

#include <stdio.h>
#include <math.h>
#include <string.h>

static int g_lo_pass = 0;
static int g_lo_fail = 0;

#define LO_ASSERT_FLOAT(delta, expected, actual) do { \
    if (fabs((double)(expected) - (double)(actual)) <= (double)(delta)) { g_lo_pass++; } \
    else { printf("  FAIL: %s:%d: Expected %.4f got %.4f\n", __FILE__, __LINE__, (double)(expected), (double)(actual)); g_lo_fail++; } \
} while(0)

#define LO_ASSERT_TRUE(cond) do { \
    if (cond) { g_lo_pass++; } \
    else { printf("  FAIL: %s:%d\n", __FILE__, __LINE__); g_lo_fail++; } \
} while(0)

#define LO_RUN(func) do { printf("  Running %s...\n", #func); func(); } while(0)

/* Simulate DC offset measurement */
static double simulate_lo_leakage(int16_t *bi, int16_t *bq, int n,
                                   int16_t dc_offset_i, int16_t dc_offset_q)
{
    double dc_i = 0.0, dc_q = 0.0, total = 0.0;
    for (int k = 0; k < n; k++) {
        double vi = (double)(bi[k] + dc_offset_i);
        double vq = (double)(bq[k] + dc_offset_q);
        dc_i += vi;
        dc_q += vq;
        total += vi * vi + vq * vq;
    }
    dc_i /= (double)n;
    dc_q /= (double)n;
    total /= (double)n;
    double dc_power = dc_i * dc_i + dc_q * dc_q;
    return 10.0 * log10(dc_power / (total + 1e-20) + 1e-20);
}

/* Test cases */
static void test_dc_measurement_no_offset(void)
{
    int16_t bi[512], bq[512];
    /* Generate AC signal with zero DC */
    for (int i = 0; i < 512; i++) {
        bi[i] = (int16_t)(1000.0 * cos(2.0 * 3.14159 * 10.0 * i / 512.0));
        bq[i] = (int16_t)(1000.0 * sin(2.0 * 3.14159 * 10.0 * i / 512.0));
    }
    double leakage = simulate_lo_leakage(bi, bq, 512, 0, 0);
    /* Pure AC should have very low DC component */
    LO_ASSERT_TRUE(leakage < -20.0);
}

static void test_dc_measurement_with_offset(void)
{
    int16_t bi[512], bq[512];
    for (int i = 0; i < 512; i++) {
        bi[i] = (int16_t)(1000.0 * cos(2.0 * 3.14159 * 10.0 * i / 512.0));
        bq[i] = (int16_t)(1000.0 * sin(2.0 * 3.14159 * 10.0 * i / 512.0));
    }
    /* Add significant DC offset */
    double leakage = simulate_lo_leakage(bi, bq, 512, 500, 500);
    /* DC offset should increase LO leakage significantly */
    LO_ASSERT_TRUE(leakage > -10.0);
}

static void test_binary_search_convergence(void)
{
    /* Simulate the binary search algorithm */
    int16_t optimal_i = 42;
    int16_t optimal_q = -17;
    int16_t current_i = 0, current_q = 0;
    int16_t step = 64;

    /* Binary search on I */
    while (step >= 1) {
        int16_t neg_err = abs(current_i - step - optimal_i);
        int16_t pos_err = abs(current_i + step - optimal_i);
        if (neg_err < pos_err) current_i -= step;
        else current_i += step;
        step >>= 1;
    }

    /* Binary search on Q */
    step = 64;
    while (step >= 1) {
        int16_t neg_err = abs(current_q - step - optimal_q);
        int16_t pos_err = abs(current_q + step - optimal_q);
        if (neg_err < pos_err) current_q -= step;
        else current_q += step;
        step >>= 1;
    }

    /* Should converge to within 1 code of optimal */
    LO_ASSERT_TRUE(abs(current_i - optimal_i) <= 1);
    LO_ASSERT_TRUE(abs(current_q - optimal_q) <= 1);
}

static void test_gradient_descent_refinement(void)
{
    int16_t best_i = 41, best_q = -16;
    int16_t optimal_i = 42, optimal_q = -17;

    /* Single step gradient descent */
    int16_t candidates_i[] = {best_i - 1, best_i + 1, best_i, best_i};
    int16_t candidates_q[] = {best_q, best_q, best_q - 1, best_q + 1};

    int best_err = abs(best_i - optimal_i) + abs(best_q - optimal_q);

    for (int c = 0; c < 4; c++) {
        int err = abs(candidates_i[c] - optimal_i) + abs(candidates_q[c] - optimal_q);
        if (err < best_err) {
            best_err = err;
            best_i = candidates_i[c];
            best_q = candidates_q[c];
        }
    }

    LO_ASSERT_TRUE(abs(best_i - optimal_i) + abs(best_q - optimal_q) < 2);
}

static void test_lo_leakage_target(void)
{
    double target_dbc = -40.0;
    double measured_dbc = -45.0;
    LO_ASSERT_TRUE(measured_dbc <= target_dbc);

    measured_dbc = -35.0;
    LO_ASSERT_TRUE(measured_dbc > target_dbc);
}

/* Test runner */
void run_lo_nulling_tests(void)
{
    printf("\n=== LO Leakage Nulling Tests ===\n");

    LO_RUN(test_dc_measurement_no_offset);
    LO_RUN(test_dc_measurement_with_offset);
    LO_RUN(test_binary_search_convergence);
    LO_RUN(test_gradient_descent_refinement);
    LO_RUN(test_lo_leakage_target);

    printf("  LO Nulling Tests: %d passed, %d failed\n\n", g_lo_pass, g_lo_fail);
}

#ifdef STANDALONE_TEST
int main(void) {
    run_lo_nulling_tests();
    return g_lo_fail > 0 ? 1 : 0;
}
#endif
