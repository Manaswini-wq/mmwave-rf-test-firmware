/**
 * @file main.c
 * @brief Main entry point for mmWave RF Test Firmware
 *
 * Bare-metal firmware for IWR6843 ARM Cortex-R5F that initializes all
 * analog circuit FW drivers, loads ROM boot hooks and factory trim data,
 * then enters a UART command-driven test loop for RF production tests
 * and calibration sequences.
 */

#include "platform_types.h"
#include "iwr6843_regs.h"
#include "rf_test_cfg.h"
#include "test_limits.h"

#include "drivers/synth_driver.h"
#include "drivers/pga_driver.h"
#include "drivers/if_filter_driver.h"
#include "drivers/adc_driver.h"
#include "drivers/dac_driver.h"
#include "drivers/tx_frontend.h"
#include "drivers/rx_frontend.h"
#include "rom/rom_boot_hooks.h"
#include "rom/factory_trim.h"
#include "data/capture_engine.h"
#include "data/dma_manager.h"
#include "protocol/uart_cmd_handler.h"
#include "protocol/test_protocol.h"

#include "rf_tests/tx_power_test.h"
#include "rf_tests/rx_sensitivity_test.h"
#include "rf_tests/phase_noise_test.h"
#include "rf_tests/freq_accuracy_test.h"
#include "rf_tests/noise_figure_test.h"
#include "rf_tests/tx_evm_test.h"

#include "calibration/tx_power_flatness_cal.h"
#include "calibration/rx_gain_phase_cal.h"
#include "calibration/lo_leakage_nulling.h"
#include "calibration/inter_channel_cal.h"

/* ========================================================================= */
/*  Static variables                                                          */
/* ========================================================================= */
static TestResultEntry_t g_test_results[MAX_TEST_RESULTS];
static uint32 g_result_count = 0U;
static boolean g_system_ready = FALSE;

/* Firmware version */
#ifndef VERSION
#define VERSION "1.0.0"
#endif

/* ========================================================================= */
/*  Forward declarations                                                      */
/* ========================================================================= */
static Status_t system_init(void);
static Status_t hw_drivers_init(void);
static void main_command_loop(void);
static void execute_test(uint32 test_id, uint8 channel, float32 freq_ghz);
static void execute_calibration(uint32 cal_id, uint8 channel);
static void run_production_test_sequence(void);
static void report_result(const TestResultEntry_t *entry);
static void store_result(const TestResultEntry_t *entry);

/* ========================================================================= */
/*  Platform delay implementation                                             */
/* ========================================================================= */
#ifndef HOST_BUILD
void platform_delay_us(uint32 us)
{
    /* Simple busy-wait delay using R5F cycle counter */
    volatile uint32 count = us * 200U;  /* ~200 MHz R5F clock */
    while (count > 0U) {
        count--;
    }
}

uint32 platform_get_tick_ms(void)
{
    /* Read free-running timer (simplified) */
    static volatile uint32 *timer_reg = (volatile uint32 *)0x02130000U;
    return (*timer_reg) / 200000U;  /* Convert cycles to ms */
}
#else
void platform_delay_us(uint32 us) { (void)us; }
uint32 platform_get_tick_ms(void) { return 0U; }
#endif

/* ========================================================================= */
/*  Main entry point                                                          */
/* ========================================================================= */
int main(void)
{
    Status_t status;

    /* Phase 1: System initialization */
    status = system_init();
    if (status != STATUS_OK) {
        /* Fatal: blink error LED and halt */
        REG_WR32(GPIO_SET, GPIO_PIN_LED_FAIL);
        while (1) { /* halt */ }
    }

    /* Phase 2: Signal system ready */
    g_system_ready = TRUE;
    REG_WR32(GPIO_SET, GPIO_PIN_LED_PASS);

    /* Send boot banner over UART */
    uart_cmd_send_string("\r\n========================================\r\n");
    uart_cmd_send_string(" mmWave RF Test Firmware v");
    uart_cmd_send_string(VERSION);
    uart_cmd_send_string("\r\n");
    uart_cmd_send_string(" Device: IWR6843 | Core: R5F\r\n");
    uart_cmd_send_string(" Build: " __DATE__ " " __TIME__ "\r\n");
    uart_cmd_send_string("========================================\r\n");
    uart_cmd_send_string("Ready. Type 'help' for commands.\r\n> ");

    /* Phase 3: Enter command processing loop */
    main_command_loop();

    /* Should never reach here */
    return 0;
}

/* ========================================================================= */
/*  System initialization                                                     */
/* ========================================================================= */
static Status_t system_init(void)
{
    Status_t status;

    /* Step 1: ROM boot hooks - early hardware setup from ROM tables */
    status = rom_boot_hooks_init();
    if (status != STATUS_OK) return status;

    /* Step 2: Load factory trim values from OTP memory */
    status = factory_trim_load();
    if (status != STATUS_OK) return status;

    /* Step 3: Initialize all hardware drivers */
    status = hw_drivers_init();
    if (status != STATUS_OK) return status;

    /* Step 4: Initialize DMA engine for ADC data transfers */
    status = dma_manager_init();
    if (status != STATUS_OK) return status;

    /* Step 5: Initialize capture engine */
    status = capture_engine_init();
    if (status != STATUS_OK) return status;

    /* Step 6: Initialize UART command handler */
    status = uart_cmd_init(UART_BAUD_RATE);
    if (status != STATUS_OK) return status;

    /* Step 7: Initialize test protocol layer */
    status = test_protocol_init();
    if (status != STATUS_OK) return status;

    return STATUS_OK;
}

static Status_t hw_drivers_init(void)
{
    Status_t status;

    /* Initialize frequency synthesizer */
    status = synth_init();
    if (status != STATUS_OK) return status;

    /* Initialize PGA on all RX channels */
    status = pga_init();
    if (status != STATUS_OK) return status;

    /* Initialize IF bandpass filters */
    status = if_filter_init();
    if (status != STATUS_OK) return status;

    /* Initialize ADC subsystem */
    status = adc_init(ADC_DEFAULT_RESOLUTION, ADC_DEFAULT_FORMAT,
                      ADC_DEFAULT_SAMPLE_RATE);
    if (status != STATUS_OK) return status;

    /* Initialize DAC for calibration outputs */
    status = dac_init();
    if (status != STATUS_OK) return status;

    /* Initialize TX frontend (PA, phase shifters) */
    status = tx_frontend_init();
    if (status != STATUS_OK) return status;

    /* Initialize RX frontend (LNA, mixer) */
    status = rx_frontend_init();
    if (status != STATUS_OK) return status;

    return STATUS_OK;
}

/* ========================================================================= */
/*  Command processing loop                                                   */
/* ========================================================================= */
static void main_command_loop(void)
{
    CmdPacket_t cmd;

    while (1) {
        /* Wait for next command from host */
        if (uart_cmd_receive(&cmd) == STATUS_OK) {
            switch (cmd.command) {
            case CMD_TEST_SINGLE:
                execute_test(cmd.test_id, cmd.channel, cmd.param_f32[0]);
                break;

            case CMD_TEST_ALL:
                run_production_test_sequence();
                break;

            case CMD_CALIBRATE:
                execute_calibration(cmd.test_id, cmd.channel);
                break;

            case CMD_GET_RESULTS:
                for (uint32 i = 0U; i < g_result_count; i++) {
                    report_result(&g_test_results[i]);
                }
                break;

            case CMD_CLEAR_RESULTS:
                g_result_count = 0U;
                uart_cmd_send_string("Results cleared.\r\n");
                break;

            case CMD_GET_STATUS:
                test_protocol_send_status(g_system_ready, g_result_count);
                break;

            case CMD_RESET:
                uart_cmd_send_string("Resetting...\r\n");
                platform_reset();
                break;

            default:
                uart_cmd_send_string("Unknown command.\r\n");
                break;
            }
            uart_cmd_send_string("> ");
        }
    }
}

/* ========================================================================= */
/*  Test execution                                                            */
/* ========================================================================= */
static void execute_test(uint32 test_id, uint8 channel, float32 freq_ghz)
{
    TestResultEntry_t entry = {0};
    entry.test_id = test_id;
    entry.channel = channel;
    entry.frequency_ghz = freq_ghz;
    entry.timestamp_ms = platform_get_tick_ms();

    const TestLimit_t *limits = test_limits_get((TestId_t)test_id);
    if (limits != NULL_PTR) {
        entry.lower_limit = limits->lower;
        entry.upper_limit = limits->upper;
    }

    switch ((TestId_t)test_id) {
    case TEST_ID_TX_POWER:
        entry.measured_value = tx_power_test_measure(channel, freq_ghz);
        break;
    case TEST_ID_RX_NOISE_FLOOR:
        entry.measured_value = rx_sensitivity_test_measure(channel, freq_ghz);
        break;
    case TEST_ID_PHASE_NOISE_1M:
        entry.measured_value = phase_noise_test_measure(channel, freq_ghz,
                                                         PN_OFFSET_1MHZ);
        break;
    case TEST_ID_FREQ_ACCURACY:
        entry.measured_value = freq_accuracy_test_measure(channel, freq_ghz);
        break;
    case TEST_ID_NOISE_FIGURE:
        entry.measured_value = noise_figure_test_measure(channel, freq_ghz);
        break;
    case TEST_ID_TX_EVM:
        entry.measured_value = tx_evm_test_measure(channel, freq_ghz);
        break;
    default:
        uart_cmd_send_string("Invalid test ID.\r\n");
        return;
    }

    entry.result = (limits != NULL_PTR) ? LIMIT_CHECK(entry.measured_value, *limits)
                                         : TEST_RESULT_ERROR;

    store_result(&entry);
    report_result(&entry);
}

static void execute_calibration(uint32 cal_id, uint8 channel)
{
    Status_t status;

    switch (cal_id) {
    case 0U:
        status = tx_power_flatness_cal_run(channel);
        break;
    case 1U:
        status = rx_gain_phase_cal_run(channel);
        break;
    case 2U:
        status = lo_leakage_nulling_run(channel);
        break;
    case 3U:
        status = inter_channel_cal_run();
        break;
    default:
        uart_cmd_send_string("Invalid calibration ID.\r\n");
        return;
    }

    if (status == STATUS_OK) {
        uart_cmd_send_string("Calibration PASSED.\r\n");
    } else {
        uart_cmd_send_string("Calibration FAILED.\r\n");
    }
}

static void run_production_test_sequence(void)
{
    uint8 ch;

    uart_cmd_send_string("=== Production Test Sequence ===\r\n");
    g_result_count = 0U;

    /* TX power test on all channels */
    for (ch = 0U; ch < RF_NUM_TX_CHANNELS; ch++) {
        execute_test(TEST_ID_TX_POWER, ch, RF_FREQ_CENTER_GHZ);
    }

    /* RX noise floor on all channels */
    for (ch = 0U; ch < RF_NUM_RX_CHANNELS; ch++) {
        execute_test(TEST_ID_RX_NOISE_FLOOR, ch, RF_FREQ_CENTER_GHZ);
    }

    /* Phase noise (single channel) */
    execute_test(TEST_ID_PHASE_NOISE_1M, 0U, RF_FREQ_CENTER_GHZ);

    /* Frequency accuracy */
    execute_test(TEST_ID_FREQ_ACCURACY, 0U, RF_FREQ_CENTER_GHZ);

    /* Noise figure on all RX channels */
    for (ch = 0U; ch < RF_NUM_RX_CHANNELS; ch++) {
        execute_test(TEST_ID_NOISE_FIGURE, ch, RF_FREQ_CENTER_GHZ);
    }

    /* TX EVM on all TX channels */
    for (ch = 0U; ch < RF_NUM_TX_CHANNELS; ch++) {
        execute_test(TEST_ID_TX_EVM, ch, RF_FREQ_CENTER_GHZ);
    }

    /* Summary */
    uint32 pass_count = 0U;
    for (uint32 i = 0U; i < g_result_count; i++) {
        if (g_test_results[i].result == TEST_RESULT_PASS) pass_count++;
    }
    uart_cmd_send_string("\r\n=== Summary ===\r\n");
    test_protocol_send_summary(pass_count, g_result_count);
}

/* ========================================================================= */
/*  Result handling                                                           */
/* ========================================================================= */
static void store_result(const TestResultEntry_t *entry)
{
    if (g_result_count < MAX_TEST_RESULTS) {
        g_test_results[g_result_count] = *entry;
        g_result_count++;
    }
}

static void report_result(const TestResultEntry_t *entry)
{
    test_protocol_send_result(entry);
}
