/*
 * Unit Tests for UART Retry Mechanism (HW-015)
 * TDD Test Suite for uart_send_with_retry
 */

#include <zephyr/ztest.h>
#include "uart_driver.h"
#include "lighting.h"  /* for RESP_ACK, RESP_NAK */

/* ================================================================
 * Test: Frame building in retry context
 * ================================================================ */
ZTEST(uart_retry_suite, test_retry_frame_build_valid)
{
	uint8_t data[] = {0x0A, 0x74};
	uint8_t buf[FRAME_MAX_TOTAL];
	
	int len = frame_build(CMD_SET_COLOR_TEMP, data, sizeof(data), buf);
	
	zassert_true(len > 0, "Frame build should succeed");
	zassert_equal(len, 9, "Frame length should be 9");
}

ZTEST(uart_retry_suite, test_retry_null_data_with_zero_len)
{
	uint8_t buf[FRAME_MAX_TOTAL];
	
	int len = frame_build(CMD_GET_STATUS, NULL, 0, buf);
	
	zassert_true(len > 0, "Frame build with NULL data and len=0 should succeed");
}

/* ================================================================
 * Test: Command codes
 * ================================================================ */
ZTEST(uart_retry_suite, test_command_codes_defined)
{
	zassert_equal(CMD_SET_COLOR_TEMP, 0x01);
	zassert_equal(CMD_SET_BRIGHTNESS, 0x02);
	zassert_equal(CMD_SET_BOTH, 0x03);
	zassert_equal(CMD_GET_STATUS, 0x04);
}

/* ================================================================
 * Test: Retry constants
 * ================================================================ */
ZTEST(uart_retry_suite, test_retry_constants)
{
	zassert_true(UART_MAX_RETRIES >= 2, "Should have at least 2 retries");
	zassert_true(UART_MAX_RETRIES <= 5, "Should not have too many retries");
	zassert_true(UART_RETRY_DELAY_MS >= 50, "Retry delay should be reasonable");
}

/* ================================================================
 * Test: Frame constants
 * ================================================================ */
ZTEST(uart_retry_suite, test_frame_constants)
{
	zassert_equal(FRAME_HEADER_HI, 0xAA);
	zassert_equal(FRAME_HEADER_LO, 0x55);
	zassert_equal(FRAME_FOOTER_HI, 0x0D);
	zassert_equal(FRAME_FOOTER_LO, 0x0A);
	zassert_equal(FRAME_HEADER_LEN, 2);
	zassert_equal(FRAME_FOOTER_LEN, 2);
	zassert_equal(FRAME_OVERHEAD, 7);
}

/* ================================================================
 * Test: Timeout constant
 * ================================================================ */
ZTEST(uart_retry_suite, test_recv_timeout)
{
	zassert_true(UART_RECV_TIMEOUT_MS >= 50, "Timeout should be at least 50ms");
	zassert_true(UART_RECV_TIMEOUT_MS <= 1000, "Timeout should not exceed 1s");
}

/* ================================================================
 * Test: Max frame size
 * ================================================================ */
ZTEST(uart_retry_suite, test_max_frame_size)
{
	zassert_true(FRAME_MAX_TOTAL >= 32, "Max frame should be reasonable");
	zassert_true(FRAME_MAX_TOTAL <= 512, "Max frame should not be too large");
}

/* ================================================================
 * Test: Parse error codes
 * ================================================================ */
ZTEST(uart_retry_suite, test_parse_error_codes)
{
	zassert_equal(FRAME_PARSE_OK, 0);
	zassert_true(FRAME_PARSE_ERR_HEADER < 0);
	zassert_true(FRAME_PARSE_ERR_LENGTH < 0);
	zassert_true(FRAME_PARSE_ERR_FOOTER < 0);
	zassert_true(FRAME_PARSE_ERR_CSUM < 0);
}

/* ================================================================
 * Test: Response codes (ACK/NAK)
 * ================================================================ */
ZTEST(uart_retry_suite, test_response_codes)
{
	zassert_equal(RESP_ACK, 0x00, "ACK should be 0x00");
	zassert_equal(RESP_NAK, 0xFF, "NAK should be 0xFF");
}

/* ================================================================
 * Test: Frame build with large data
 * ================================================================ */
ZTEST(uart_retry_suite, test_frame_build_large_data)
{
	uint8_t buf[FRAME_MAX_TOTAL];
	uint8_t large_data[16];
	
	for (int i = 0; i < 16; i++) {
		large_data[i] = i;
	}
	
	int len = frame_build(CMD_SET_BOTH, large_data, sizeof(large_data), buf);
	
	zassert_true(len > 0, "Frame build with large data should succeed");
	zassert_equal(len, 7 + 16, "Length should be overhead + data");
}

/* ================================================================
 * Test Suite Setup
 * ================================================================ */
ZTEST_SUITE(uart_retry_suite, NULL, NULL, NULL, NULL, NULL);
