/**
 * @file test_protocol.h
 * @brief Test protocol layer interface
 */

#ifndef TEST_PROTOCOL_H
#define TEST_PROTOCOL_H

#include "platform_types.h"
#include "rf_test_cfg.h"

Status_t test_protocol_init(void);
void test_protocol_send_result(const TestResultEntry_t *entry);
void test_protocol_send_status(boolean ready, uint32 result_count);
void test_protocol_send_summary(uint32 pass_count, uint32 total_count);

#endif /* TEST_PROTOCOL_H */
