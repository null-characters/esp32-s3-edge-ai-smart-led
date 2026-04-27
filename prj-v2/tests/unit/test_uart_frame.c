/*
 * Unit Tests for UART Frame Protocol (HW-010, HW-011, HW-012)
 * TDD Test Suite for frame_build, frame_parse, calc_checksum
 */

#include <zephyr/ztest.h>
#include <string.h>

/* Include the source under test - we need internal functions */
#include "uart_driver.h"

/* ================================================================
 * Test: calc_checksum calculation
 * HW-012: Checksum = sum of CMD+LEN+DATA, low 8 bits
 * ================================================================ */
ZTEST(uart_frame_suite, test_calc_checksum_basic)
{
	/* Test case: CMD=0x01, LEN=0x02, DATA={0x0A, 0x74}
	 * Expected: 0x01 + 0x02 + 0x0A + 0x74 = 0x81
	 */
	uint8_t data[] = {0x01, 0x02, 0x0A, 0x74};
	uint8_t sum = calc_checksum(data, sizeof(data));
	zassert_equal(sum, 0x81, "Checksum should be 0x81, got 0x%02X", sum);
}

ZTEST(uart_frame_suite, test_calc_checksum_zero)
{
	/* Test case: empty data (just CMD+LEN=0) */
	uint8_t data[] = {0x00, 0x00};
	uint8_t sum = calc_checksum(data, sizeof(data));
	zassert_equal(sum, 0x00, "Checksum of zeros should be 0");
}

ZTEST(uart_frame_suite, test_calc_checksum_overflow)
{
	/* Test case: checksum overflow (should wrap to low 8 bits) */
	uint8_t data[] = {0xFF, 0xFF, 0xFF, 0xFF};
	uint8_t sum = calc_checksum(data, sizeof(data));
	uint8_t expected = (0xFF + 0xFF + 0xFF + 0xFF) & 0xFF; /* = 0xFC */
	zassert_equal(sum, expected, "Checksum should wrap to low 8 bits");
}

/* ================================================================
 * Test: frame_build (HW-010)
 * Frame format: [AA 55] [CMD] [LEN] [DATA...] [CS] [0D 0A]
 * ================================================================ */
ZTEST(uart_frame_suite, test_frame_build_color_temp)
{
	/* Build CMD_SET_COLOR_TEMP (0x01) with temp=2676K = 0x0A74 */
	uint8_t buf[FRAME_MAX_TOTAL];
	uint8_t data[] = {0x0A, 0x74}; /* 2676K little-endian */
	
	int len = frame_build(0x01, data, sizeof(data), buf);
	
	/* Should return total frame length */
	zassert_equal(len, 9, "Frame length should be 9 bytes (2+1+1+2+1+2)");
	
	/* Verify frame header */
	zassert_equal(buf[0], 0xAA, "Frame header high byte should be 0xAA");
	zassert_equal(buf[1], 0x55, "Frame header low byte should be 0x55");
	
	/* Verify CMD and LEN */
	zassert_equal(buf[2], 0x01, "CMD should be 0x01");
	zassert_equal(buf[3], 0x02, "LEN should be 2");
	
	/* Verify DATA */
	zassert_equal(buf[4], 0x0A, "DATA[0] should be 0x0A");
	zassert_equal(buf[5], 0x74, "DATA[1] should be 0x74");
	
	/* Verify checksum: 0x01+0x02+0x0A+0x74 = 0x81 */
	zassert_equal(buf[6], 0x81, "Checksum should be 0x81");
	
	/* Verify footer */
	zassert_equal(buf[7], 0x0D, "Footer high byte should be 0x0D");
	zassert_equal(buf[8], 0x0A, "Footer low byte should be 0x0A");
}

ZTEST(uart_frame_suite, test_frame_build_no_data)
{
	/* Build frame with no data payload */
	uint8_t buf[FRAME_MAX_TOTAL];
	
	int len = frame_build(0x04, NULL, 0, buf); /* CMD_GET_STATUS */
	
	/* Should return minimum frame length */
	zassert_equal(len, FRAME_OVERHEAD, "No-data frame length should be %d", FRAME_OVERHEAD);
	
	/* Verify header, CMD, LEN=0, checksum=CMD+LEN=0x04, footer */
	zassert_equal(buf[0], 0xAA);
	zassert_equal(buf[1], 0x55);
	zassert_equal(buf[2], 0x04);
	zassert_equal(buf[3], 0x00);
	zassert_equal(buf[4], 0x04); /* checksum = 0x04 + 0x00 */
	zassert_equal(buf[5], 0x0D);
	zassert_equal(buf[6], 0x0A);
}

ZTEST(uart_frame_suite, test_frame_build_null_buffer)
{
	/* Should return error for null buffer */
	uint8_t data[] = {0x01};
	int len = frame_build(0x01, data, sizeof(data), NULL);
	zassert_true(len < 0, "Should return error for null buffer");
}

/* ================================================================
 * Test: frame_parse (HW-011)
 * ================================================================ */
ZTEST(uart_frame_suite, test_frame_parse_valid)
{
	/* Build a valid frame then parse it */
	uint8_t tx_buf[FRAME_MAX_TOTAL];
	uint8_t test_data[] = {0x0A, 0x74};
	int tx_len = frame_build(0x01, test_data, sizeof(test_data), tx_buf);
	zassert_true(tx_len > 0, "Frame build should succeed");
	
	/* Parse the frame */
	uint8_t parsed_cmd, parsed_data[256], parsed_len;
	int ret = frame_parse(tx_buf, tx_len, &parsed_cmd, parsed_data, &parsed_len);
	
	zassert_equal(ret, FRAME_PARSE_OK, "Parse should succeed");
	zassert_equal(parsed_cmd, 0x01, "Parsed CMD should match");
	zassert_equal(parsed_len, 2, "Parsed LEN should match");
	zassert_equal(parsed_data[0], 0x0A, "Parsed DATA[0] should match");
	zassert_equal(parsed_data[1], 0x74, "Parsed DATA[1] should match");
}

ZTEST(uart_frame_suite, test_frame_parse_invalid_header)
{
	/* Frame with wrong header */
	uint8_t bad_frame[] = {0x12, 0x34, 0x01, 0x00, 0x01, 0x0D, 0x0A};
	uint8_t cmd, data[256], len;
	
	int ret = frame_parse(bad_frame, sizeof(bad_frame), &cmd, data, &len);
	zassert_equal(ret, FRAME_PARSE_ERR_HEADER, "Should detect invalid header");
}

ZTEST(uart_frame_suite, test_frame_parse_invalid_footer)
{
	/* Frame with wrong footer */
	uint8_t bad_frame[] = {0xAA, 0x55, 0x01, 0x00, 0x01, 0xDE, 0xAD};
	uint8_t cmd, data[256], len;
	
	int ret = frame_parse(bad_frame, sizeof(bad_frame), &cmd, data, &len);
	zassert_equal(ret, FRAME_PARSE_ERR_FOOTER, "Should detect invalid footer");
}

ZTEST(uart_frame_suite, test_frame_parse_wrong_checksum)
{
	/* Frame with corrupted checksum */
	uint8_t bad_frame[] = {0xAA, 0x55, 0x01, 0x00, 0xFF, 0x0D, 0x0A}; /* checksum should be 0x01 */
	uint8_t cmd, data[256], len;
	
	int ret = frame_parse(bad_frame, sizeof(bad_frame), &cmd, data, &len);
	zassert_equal(ret, FRAME_PARSE_ERR_CSUM, "Should detect checksum error");
}

ZTEST(uart_frame_suite, test_frame_parse_too_short)
{
	/* Frame shorter than minimum */
	uint8_t bad_frame[] = {0xAA, 0x55, 0x01};
	uint8_t cmd, data[256], len;
	
	int ret = frame_parse(bad_frame, sizeof(bad_frame), &cmd, data, &len);
	zassert_equal(ret, FRAME_PARSE_ERR_LENGTH, "Should detect too short frame");
}

/* ================================================================
 * Round-trip test: build then parse
 * ================================================================ */
ZTEST(uart_frame_suite, test_frame_roundtrip)
{
	uint8_t tx_buf[FRAME_MAX_TOTAL];
	uint8_t test_data[] = {0x12, 0x34, 0x56, 0x78, 0x9A, 0xBC, 0xDE, 0xF0};
	
	/* Build */
	int tx_len = frame_build(0x03, test_data, sizeof(test_data), tx_buf);
	zassert_true(tx_len > 0, "Build should succeed");
	
	/* Parse */
	uint8_t parsed_cmd, parsed_data[256], parsed_len;
	int ret = frame_parse(tx_buf, tx_len, &parsed_cmd, parsed_data, &parsed_len);
	zassert_equal(ret, FRAME_PARSE_OK, "Parse should succeed");
	
	/* Verify */
	zassert_equal(parsed_cmd, 0x03, "CMD mismatch");
	zassert_equal(parsed_len, sizeof(test_data), "LEN mismatch");
	zassert_mem_equal(parsed_data, test_data, sizeof(test_data), "DATA mismatch");
}

/* ================================================================
 * Test Suite Setup
 * ================================================================ */
ZTEST_SUITE(uart_frame_suite, NULL, NULL, NULL, NULL, NULL);
