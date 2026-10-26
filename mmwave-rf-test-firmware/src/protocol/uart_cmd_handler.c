/**
 * @file uart_cmd_handler.c
 * @brief UART command handler for host PC communication
 *
 * Implements a simple ASCII command protocol over UART for controlling
 * RF tests and calibration from a host PC.  Commands are line-terminated
 * and parsed into a CmdPacket_t structure for the main command loop.
 */

#include "protocol/uart_cmd_handler.h"
#include "iwr6843_regs.h"
#include "rf_test_cfg.h"

#include <string.h>

/* ========================================================================= */
/*  Internal state                                                            */
/* ========================================================================= */
static char g_cmd_buf[UART_CMD_BUF_SIZE];
static uint32 g_cmd_pos = 0U;
static boolean g_uart_initialized = FALSE;

/* ========================================================================= */
/*  Low-level UART I/O                                                        */
/* ========================================================================= */

static void uart_putchar(char c)
{
#ifndef HOST_BUILD
    /* Wait for TX FIFO space */
    while (!BIT_TST(REG_RD32(UART_LSR), UART_LSR_THRE)) {
        /* spin */
    }
    REG_WR32(UART_THR, (uint32)c);
#else
    (void)c;
#endif
}

static sint32 uart_getchar(void)
{
#ifndef HOST_BUILD
    if (BIT_TST(REG_RD32(UART_LSR), UART_LSR_DR)) {
        return (sint32)(REG_RD32(UART_RBR) & 0xFFU);
    }
    return -1;  /* No data available */
#else
    return -1;
#endif
}

/* ========================================================================= */
/*  Command parser                                                            */
/* ========================================================================= */

/**
 * @brief Simple string comparison (first n chars)
 */
static boolean str_starts_with(const char *str, const char *prefix)
{
    while (*prefix) {
        if (*str != *prefix) return FALSE;
        str++;
        prefix++;
    }
    return TRUE;
}

/**
 * @brief Parse simple integer from string
 */
static uint32 parse_uint(const char *str)
{
    uint32 val = 0U;
    while (*str >= '0' && *str <= '9') {
        val = val * 10U + (uint32)(*str - '0');
        str++;
    }
    return val;
}

/**
 * @brief Parse a simple float from string (integer.fraction)
 */
static float32 parse_float(const char *str)
{
    float32 result = 0.0f;
    float32 sign = 1.0f;

    if (*str == '-') { sign = -1.0f; str++; }

    /* Integer part */
    while (*str >= '0' && *str <= '9') {
        result = result * 10.0f + (float32)(*str - '0');
        str++;
    }

    /* Fractional part */
    if (*str == '.') {
        str++;
        float32 frac = 0.1f;
        while (*str >= '0' && *str <= '9') {
            result += (float32)(*str - '0') * frac;
            frac *= 0.1f;
            str++;
        }
    }

    return sign * result;
}

/**
 * @brief Skip whitespace and find next token
 */
static const char *skip_space(const char *str)
{
    while (*str == ' ' || *str == '\t') str++;
    return str;
}

static const char *skip_token(const char *str)
{
    while (*str && *str != ' ' && *str != '\t') str++;
    return str;
}

/**
 * @brief Parse command line into CmdPacket_t
 */
static boolean parse_command(const char *line, CmdPacket_t *cmd)
{
    memset(cmd, 0, sizeof(CmdPacket_t));

    line = skip_space(line);

    if (str_starts_with(line, "test ")) {
        line = skip_space(skip_token(line));

        if (str_starts_with(line, "tx_power")) {
            cmd->command = CMD_TEST_SINGLE;
            cmd->test_id = 0U;  /* TEST_ID_TX_POWER */
        } else if (str_starts_with(line, "rx_noise")) {
            cmd->command = CMD_TEST_SINGLE;
            cmd->test_id = 3U;  /* TEST_ID_RX_NOISE_FLOOR */
        } else if (str_starts_with(line, "phase_noise")) {
            cmd->command = CMD_TEST_SINGLE;
            cmd->test_id = 6U;  /* TEST_ID_PHASE_NOISE_1M */
        } else if (str_starts_with(line, "freq_acc")) {
            cmd->command = CMD_TEST_SINGLE;
            cmd->test_id = 8U;
        } else if (str_starts_with(line, "noise_fig")) {
            cmd->command = CMD_TEST_SINGLE;
            cmd->test_id = 9U;
        } else if (str_starts_with(line, "tx_evm")) {
            cmd->command = CMD_TEST_SINGLE;
            cmd->test_id = 10U;
        } else if (str_starts_with(line, "all")) {
            cmd->command = CMD_TEST_ALL;
        } else {
            return FALSE;
        }

        /* Parse optional --channel N and --freq F.F */
        line = skip_token(line);
        while (*line) {
            line = skip_space(line);
            if (str_starts_with(line, "--channel")) {
                line = skip_space(skip_token(line));
                cmd->channel = (uint8)parse_uint(line);
                line = skip_token(line);
            } else if (str_starts_with(line, "--freq")) {
                line = skip_space(skip_token(line));
                cmd->param_f32[0] = parse_float(line);
                line = skip_token(line);
            } else {
                line = skip_token(line);
            }
        }

        /* Default frequency if not specified */
        if (cmd->param_f32[0] < 1.0f) {
            cmd->param_f32[0] = RF_FREQ_CENTER_GHZ;
        }

        return TRUE;

    } else if (str_starts_with(line, "cal ")) {
        cmd->command = CMD_CALIBRATE;
        line = skip_space(skip_token(line));

        if (str_starts_with(line, "tx_flat"))   cmd->test_id = 0U;
        else if (str_starts_with(line, "rx_gp")) cmd->test_id = 1U;
        else if (str_starts_with(line, "lo_null")) cmd->test_id = 2U;
        else if (str_starts_with(line, "inter_ch")) cmd->test_id = 3U;
        else return FALSE;

        line = skip_token(line);
        line = skip_space(line);
        if (str_starts_with(line, "--channel")) {
            line = skip_space(skip_token(line));
            cmd->channel = (uint8)parse_uint(line);
        }
        return TRUE;

    } else if (str_starts_with(line, "results")) {
        cmd->command = CMD_GET_RESULTS;
        return TRUE;
    } else if (str_starts_with(line, "clear")) {
        cmd->command = CMD_CLEAR_RESULTS;
        return TRUE;
    } else if (str_starts_with(line, "status")) {
        cmd->command = CMD_GET_STATUS;
        return TRUE;
    } else if (str_starts_with(line, "reset")) {
        cmd->command = CMD_RESET;
        return TRUE;
    } else if (str_starts_with(line, "help")) {
        uart_cmd_send_string("Commands:\r\n");
        uart_cmd_send_string("  test <name> [--channel N] [--freq F.F]\r\n");
        uart_cmd_send_string("    Names: tx_power rx_noise phase_noise freq_acc noise_fig tx_evm all\r\n");
        uart_cmd_send_string("  cal <name> [--channel N]\r\n");
        uart_cmd_send_string("    Names: tx_flatness rx_gp lo_null inter_ch\r\n");
        uart_cmd_send_string("  results  - Show test results\r\n");
        uart_cmd_send_string("  clear    - Clear results\r\n");
        uart_cmd_send_string("  status   - System status\r\n");
        uart_cmd_send_string("  reset    - Reset device\r\n");
        return FALSE;  /* help doesn't produce a command */
    }

    return FALSE;
}

/* ========================================================================= */
/*  Public API                                                                */
/* ========================================================================= */

Status_t uart_cmd_init(uint32 baud_rate)
{
#ifndef HOST_BUILD
    /* Configure UART: 8N1 */
    REG_WR32(UART_LCR, 0x83U);  /* DLAB=1, 8-bit, no parity, 1 stop */

    /* Set baud rate divisor: clock = 48 MHz */
    uint32 divisor = 48000000U / (16U * baud_rate);
    REG_WR32(UART_DLL, divisor & 0xFFU);
    REG_WR32(UART_DLH, (divisor >> 8) & 0xFFU);

    REG_WR32(UART_LCR, 0x03U);  /* DLAB=0, 8-bit, no parity, 1 stop */

    /* Enable and reset FIFOs */
    REG_WR32(UART_FCR, 0x07U);  /* Enable FIFO, reset TX/RX FIFO */
#endif

    g_cmd_pos = 0U;
    g_uart_initialized = TRUE;

    return STATUS_OK;
}

Status_t uart_cmd_receive(CmdPacket_t *cmd)
{
    if (!g_uart_initialized || cmd == NULL_PTR) {
        return STATUS_NOT_INIT;
    }

    /* Poll for incoming characters */
    sint32 c = uart_getchar();
    while (c >= 0) {
        char ch = (char)c;

        if (ch == '\r' || ch == '\n') {
            if (g_cmd_pos > 0U) {
                g_cmd_buf[g_cmd_pos] = '\0';
                uart_cmd_send_string("\r\n");

                boolean valid = parse_command(g_cmd_buf, cmd);
                g_cmd_pos = 0U;

                if (valid) return STATUS_OK;
            }
        } else if (ch == '\b' || ch == 0x7F) {
            /* Backspace */
            if (g_cmd_pos > 0U) {
                g_cmd_pos--;
                uart_cmd_send_string("\b \b");
            }
        } else if (g_cmd_pos < UART_CMD_BUF_SIZE - 1U) {
            g_cmd_buf[g_cmd_pos++] = ch;
            uart_putchar(ch);  /* Echo */
        }

        c = uart_getchar();
    }

    return STATUS_TIMEOUT;
}

void uart_cmd_send_string(const char *str)
{
    if (str == NULL_PTR) return;
    while (*str) {
        uart_putchar(*str);
        str++;
    }
}

void uart_cmd_send_byte(uint8 byte)
{
    uart_putchar((char)byte);
}

void uart_cmd_send_u32(uint32 value)
{
    char buf[12];
    sint32 i = 10;
    buf[11] = '\0';

    if (value == 0U) {
        uart_cmd_send_string("0");
        return;
    }

    while (value > 0U && i >= 0) {
        buf[i--] = '0' + (char)(value % 10U);
        value /= 10U;
    }

    uart_cmd_send_string(&buf[i + 1]);
}
