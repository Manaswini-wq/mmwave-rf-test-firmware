/**
 * @file uart_cmd_handler.h
 * @brief UART command handler interface for host communication
 */

#ifndef UART_CMD_HANDLER_H
#define UART_CMD_HANDLER_H

#include "platform_types.h"

/* Command IDs */
#define CMD_TEST_SINGLE     0x01U
#define CMD_TEST_ALL        0x02U
#define CMD_CALIBRATE       0x03U
#define CMD_GET_RESULTS     0x04U
#define CMD_CLEAR_RESULTS   0x05U
#define CMD_GET_STATUS      0x06U
#define CMD_RESET           0x07U

typedef struct {
    uint8   command;
    uint32  test_id;
    uint8   channel;
    float32 param_f32[4];
    uint32  param_u32[4];
} CmdPacket_t;

Status_t uart_cmd_init(uint32 baud_rate);
Status_t uart_cmd_receive(CmdPacket_t *cmd);
void     uart_cmd_send_string(const char *str);
void     uart_cmd_send_byte(uint8 byte);
void     uart_cmd_send_u32(uint32 value);

#endif /* UART_CMD_HANDLER_H */
