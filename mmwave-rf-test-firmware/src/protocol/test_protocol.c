/**
 * @file test_protocol.c
 * @brief Test protocol layer for structured result reporting
 *
 * Provides formatted output of test results, status, and summary data
 * over the UART command interface.  Results are formatted for both
 * human-readable console output and machine-parseable format for
 * automated test orchestration.
 */

#include "protocol/test_protocol.h"
#include "protocol/uart_cmd_handler.h"
#include "test_limits.h"

#include <string.h>

/* ========================================================================= */
/*  Internal helpers                                                          */
/* ========================================================================= */

static const char *test_id_to_name(uint32 test_id)
{
    static const char *names[] = {
        "TX_POWER", "TX_VARIATION", "TX_FLATNESS",
        "RX_NOISE_FLOOR", "RX_GAIN",
        "PHASE_NOISE_100K", "PHASE_NOISE_1M", "PHASE_NOISE_10M",
        "FREQ_ACCURACY", "NOISE_FIGURE", "TX_EVM"
    };
    if (test_id < ARRAY_SIZE(names)) return names[test_id];
    return "UNKNOWN";
}

static const char *result_to_string(TestResult_t result)
{
    switch (result) {
        case TEST_RESULT_PASS:     return "PASS";
        case TEST_RESULT_FAIL:     return "FAIL";
        case TEST_RESULT_MARGINAL: return "MARGINAL";
        case TEST_RESULT_SKIPPED:  return "SKIPPED";
        case TEST_RESULT_ERROR:    return "ERROR";
        default:                   return "UNKNOWN";
    }
}

/**
 * @brief Format a float as integer.fraction string (1 decimal place)
 */
static void send_float_1dp(float32 value)
{
    if (value < 0.0f) {
        uart_cmd_send_string("-");
        value = -value;
    }

    uint32 integer_part = (uint32)value;
    uint32 frac_part = (uint32)((value - (float32)integer_part) * 10.0f + 0.5f);

    if (frac_part >= 10U) {
        integer_part++;
        frac_part = 0U;
    }

    uart_cmd_send_u32(integer_part);
    uart_cmd_send_string(".");
    uart_cmd_send_u32(frac_part);
}

/* ========================================================================= */
/*  Public API                                                                */
/* ========================================================================= */

Status_t test_protocol_init(void)
{
    return STATUS_OK;
}

void test_protocol_send_result(const TestResultEntry_t *entry)
{
    if (entry == NULL_PTR) return;

    /* Format: TEST_NAME CH# @ FREQ GHz: VALUE UNIT [LOW, HIGH] RESULT */
    uart_cmd_send_string("  ");
    uart_cmd_send_string(test_id_to_name(entry->test_id));
    uart_cmd_send_string(" CH");
    uart_cmd_send_u32((uint32)entry->channel);
    uart_cmd_send_string(" @ ");
    send_float_1dp(entry->frequency_ghz);
    uart_cmd_send_string(" GHz: ");
    send_float_1dp(entry->measured_value);

    /* Get unit string from limits */
    const TestLimit_t *lim = test_limits_get((TestId_t)entry->test_id);
    if (lim != NULL_PTR) {
        uart_cmd_send_string(" ");
        uart_cmd_send_string(lim->unit);
        uart_cmd_send_string(" [");
        send_float_1dp(entry->lower_limit);
        uart_cmd_send_string(", ");
        send_float_1dp(entry->upper_limit);
        uart_cmd_send_string("]");
    }

    uart_cmd_send_string(" -> ");
    uart_cmd_send_string(result_to_string(entry->result));
    uart_cmd_send_string("\r\n");
}

void test_protocol_send_status(boolean ready, uint32 result_count)
{
    uart_cmd_send_string("System Status:\r\n");
    uart_cmd_send_string("  Ready: ");
    uart_cmd_send_string(ready ? "YES" : "NO");
    uart_cmd_send_string("\r\n  Results stored: ");
    uart_cmd_send_u32(result_count);
    uart_cmd_send_string("\r\n");
}

void test_protocol_send_summary(uint32 pass_count, uint32 total_count)
{
    uart_cmd_send_string("Total: ");
    uart_cmd_send_u32(total_count);
    uart_cmd_send_string("  Passed: ");
    uart_cmd_send_u32(pass_count);
    uart_cmd_send_string("  Failed: ");
    uart_cmd_send_u32(total_count - pass_count);
    uart_cmd_send_string("\r\n");

    if (pass_count == total_count) {
        uart_cmd_send_string("OVERALL: PASS\r\n");
    } else {
        uart_cmd_send_string("OVERALL: FAIL\r\n");
    }
}
