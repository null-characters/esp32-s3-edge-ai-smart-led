# TDD Test Environment

This directory contains the unit test suite for the ESP32-S3 Edge AI Gateway project using Zephyr's ZTest framework.

## Test Structure

```
tests/
├── README.md              # This file
├── unit/                  # Unit tests
│   ├── CMakeLists.txt     # Test build configuration
│   ├── prj.conf           # Test-specific Zephyr config
│   ├── mock_pir.c         # Mock for PIR hardware
│   ├── test_main.c        # Test entry point
│   ├── test_uart_frame.c  # HW-010/011/012: Frame protocol tests
│   ├── test_time_period.c # HW-018: Time period tests
│   ├── test_rule_engine.c # HW-019: Rule engine tests
│   └── test_lighting_params.c # HW-013: Parameter validation tests
```

## Test Coverage

### HW-010/011/012: UART Frame Protocol
- `test_calc_checksum_*`: Checksum calculation validation
- `test_frame_build_*`: Frame construction with various payloads
- `test_frame_parse_*`: Frame parsing with valid/invalid inputs
- `test_frame_roundtrip`: Build-then-parse verification

### HW-018: Time Period Judgment
- `test_morning_hours`: Hours 6-11 → MORNING
- `test_afternoon_hours`: Hours 12-17 → AFTERNOON
- `test_evening_hours_*`: Hours 0-5, 18-23 → EVENING
- `test_boundary_*`: Boundary value testing (6, 12, 18, 0)
- `test_period_names`: String name verification

### HW-019: Rule Engine
- `test_period_settings_*`: Default values for each period
- `test_calculate_with_presence`: PRESENCE_YES uses period settings
- `test_calculate_no_presence_recent`: PRESENCE_NO keeps lights on
- `test_calculate_timeout_*`: PRESENCE_TIMEOUT turns lights off
- `test_null_settings_*`: Null pointer handling

### HW-013: Lighting Parameters
- `test_color_temp_*`: Range validation (2700-6500K)
- `test_brightness_*`: Range validation (0-100%)
- `test_command_codes`: Command definitions
- `test_period_defaults`: Expected default values

## Running Tests

### Prerequisites
- Zephyr SDK installed and configured
- `ZEPHYR_BASE` environment variable set

### Build and Run Tests

```bash
# Navigate to test directory
cd prj/tests/unit

# Build tests (native_sim for host execution)
west build -b native_sim --pristine

# Or build for target board (requires hardware)
# west build -b esp32s3_devkitc/esp32s3/procpu --pristine

# Run tests
west build -t run
```

### Expected Output

```
*** Booting Zephyr OS build ***
Running TESTSUITE uart_frame_suite
===================================================================
START - test_calc_checksum_basic
PASS - test_calc_checksum_basic in 0.001 seconds
===================================================================
START - test_calc_checksum_overflow
PASS - test_calc_checksum_overflow in 0.001 seconds
...
===================================================================
Test suite uart_frame_suite succeeded
===================================================================
Test suite time_period_suite succeeded
===================================================================
Test suite rule_engine_suite succeeded
===================================================================
Test suite lighting_params_suite succeeded
===================================================================
PROJECT EXECUTION SUCCESSFUL
```

## Adding New Tests

1. Create `test_<module>.c` in `tests/unit/`
2. Include `<zephyr/ztest.h>` and headers for module under test
3. Use `ZTEST(suite_name, test_name)` macro
4. Register suite with `ZTEST_SUITE(suite_name, ...)`
5. Add source file to `tests/unit/CMakeLists.txt`

## Test Status

| Module | Tests | Status |
|--------|-------|--------|
| UART Frame | 11 | Ready |
| Time Period | 7 | Ready |
| Rule Engine | 10 | Ready |
| Lighting Params | 9 | Ready |
| **Total** | **37** | Ready |

## Notes

- Hardware-dependent code (PIR GPIO, UART) is mocked for unit testing
- Tests focus on algorithmic correctness, not hardware integration
- Integration tests require actual hardware (HW-016, HW-023)
