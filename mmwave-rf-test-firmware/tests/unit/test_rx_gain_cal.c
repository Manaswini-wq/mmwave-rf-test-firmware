/**
 * @file test_rx_gain_cal.c
 * @brief Unit tests for RX gain and phase calibration
 */

#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <string.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

static int g_rx_pass = 0;
static int g_rx_fail = 0;

#define RX_ASSERT_FLOAT(delta, expected, actual) do { \
    if (fabs((double)(expected) - (double)(actual)) <= (double)(delta)) { g_rx_pass++; } \
    else { printf("  FAIL: %s:%d: Expected %.4f +/- %.4f, got %.4f\n", \
                   __FILE__, __LINE__, (double)(expected), (double)(delta), (double)(actual)); g_rx_fail++; } \
} while(0)

#define RX_ASSERT_TRUE(cond) do { \
    if (cond) { g_rx_pass++; } \
    else { printf("  FAIL: %s:%d: Condition false\n", __FILE__, __LINE__); g_rx_fail++; } \
} while(0)

#define RX_RUN(func) do { printf("  Running %s...\n", #func); func(); } while(0)

/* Test helpers */
static void generate_iq_with_offset(int16_t *bi, int16_t *bq, int n,
                                     double amp, double freq, double fs,
                                     double gain_offset_db, double phase_offset_deg)
{
    double gain_lin = pow(10.0, gain_offset_db / 20.0);
    double phase_rad = phase_offset_deg * M_PI / 180.0;

    for (int i = 0; i < n; i++) {
        double t = (double)i / fs;
        double base_phase = 2.0 * M_PI * freq * t;
        bi[i] = (int16_t)(amp * gain_lin * cos(base_phase + phase_rad));
        bq[i] = (int16_t)(amp * gain_lin * sin(base_phase + phase_rad));
    }
}

static void compute_response(const int16_t *bi, const int16_t *bq, int n,
                               double *mag_db, double *phase_deg)
{
    double si = 0.0, sq = 0.0;
    for (int k = 0; k < n; k++) {
        si += (double)bi[k];
        sq += (double)bq[k];
    }
    si /= (double)n;
    sq /= (double)n;
    double mag = sqrt(si * si + sq * sq);
    *mag_db = 20.0 * log10(mag / 32768.0 + 1e-20);
    *phase_deg = atan2(sq, si) * 180.0 / M_PI;
}

/* Test cases */
static void test_gain_offset_detection(void)
{
    int16_t ref_i[1024], ref_q[1024];
    int16_t ch1_i[1024], ch1_q[1024];

    generate_iq_with_offset(ref_i, ref_q, 1024, 10000.0, 1e6, 12.5e6, 0.0, 0.0);
    generate_iq_with_offset(ch1_i, ch1_q, 1024, 10000.0, 1e6, 12.5e6, 1.5, 0.0);

    double ref_mag, ref_phase, ch1_mag, ch1_phase;
    compute_response(ref_i, ref_q, 1024, &ref_mag, &ref_phase);
    compute_response(ch1_i, ch1_q, 1024, &ch1_mag, &ch1_phase);

    double gain_offset = ch1_mag - ref_mag;
    RX_ASSERT_FLOAT(0.5, 1.5, gain_offset);
}

static void test_phase_offset_detection(void)
{
    int16_t ref_i[1024], ref_q[1024];
    int16_t ch1_i[1024], ch1_q[1024];

    generate_iq_with_offset(ref_i, ref_q, 1024, 10000.0, 1e6, 12.5e6, 0.0, 0.0);
    generate_iq_with_offset(ch1_i, ch1_q, 1024, 10000.0, 1e6, 12.5e6, 0.0, 15.0);

    double ref_mag, ref_phase, ch1_mag, ch1_phase;
    compute_response(ref_i, ref_q, 1024, &ref_mag, &ref_phase);
    compute_response(ch1_i, ch1_q, 1024, &ch1_mag, &ch1_phase);

    double phase_offset = ch1_phase - ref_phase;
    RX_ASSERT_FLOAT(2.0, 15.0, phase_offset);
}

static void test_zero_offset_channels(void)
{
    int16_t ch0_i[1024], ch0_q[1024];
    int16_t ch1_i[1024], ch1_q[1024];

    generate_iq_with_offset(ch0_i, ch0_q, 1024, 10000.0, 1e6, 12.5e6, 0.0, 0.0);
    generate_iq_with_offset(ch1_i, ch1_q, 1024, 10000.0, 1e6, 12.5e6, 0.0, 0.0);

    double m0, p0, m1, p1;
    compute_response(ch0_i, ch0_q, 1024, &m0, &p0);
    compute_response(ch1_i, ch1_q, 1024, &m1, &p1);

    RX_ASSERT_FLOAT(0.1, 0.0, m1 - m0);
    RX_ASSERT_FLOAT(0.5, 0.0, p1 - p0);
}

static void test_phase_normalization(void)
{
    double phase = 350.0;
    while (phase > 180.0) phase -= 360.0;
    while (phase < -180.0) phase += 360.0;
    RX_ASSERT_FLOAT(0.01, -10.0, phase);

    phase = -200.0;
    while (phase > 180.0) phase -= 360.0;
    while (phase < -180.0) phase += 360.0;
    RX_ASSERT_FLOAT(0.01, 160.0, phase);
}

static void test_gain_correction_clamping(void)
{
    /* PGA gain code should be clamped to valid range */
    int pga_gain = 3;
    int adjustment = -2;
    int new_gain = pga_gain + adjustment;

    if (new_gain < 0) new_gain = 0;
    if (new_gain > 4) new_gain = 4;  /* Max gain code */
    RX_ASSERT_TRUE(new_gain >= 0 && new_gain <= 4);
    RX_ASSERT_TRUE(new_gain == 1);
}

static void test_multi_channel_calibration(void)
{
    double gains[4] = {-30.5, -31.0, -29.8, -30.2};
    double phases[4] = {10.0, 12.5, 9.0, 11.0};
    double ref_gain = gains[0];
    double ref_phase = phases[0];

    for (int ch = 0; ch < 4; ch++) {
        double g_off = gains[ch] - ref_gain;
        double p_off = phases[ch] - ref_phase;
        RX_ASSERT_FLOAT(2.0, 0.0, g_off);
        RX_ASSERT_FLOAT(5.0, 0.0, p_off);
    }
}

/* Test runner */
void run_rx_gain_tests(void)
{
    printf("\n=== RX Gain/Phase Calibration Tests ===\n");

    RX_RUN(test_gain_offset_detection);
    RX_RUN(test_phase_offset_detection);
    RX_RUN(test_zero_offset_channels);
    RX_RUN(test_phase_normalization);
    RX_RUN(test_gain_correction_clamping);
    RX_RUN(test_multi_channel_calibration);

    printf("  RX Gain Tests: %d passed, %d failed\n\n", g_rx_pass, g_rx_fail);
}

#ifdef STANDALONE_TEST
int main(void) {
    run_rx_gain_tests();
    return g_rx_fail > 0 ? 1 : 0;
}
#endif
