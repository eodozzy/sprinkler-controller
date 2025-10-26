# Sprinkler Controller Tests

This directory contains comprehensive unit tests for the Sprinkler Controller project.

## Running Tests

Tests can be run using PlatformIO:

```bash
# Run all tests
pio test

# Run specific test file
pio test --filter test_mqtt_callback
pio test --filter test_config
pio test --filter test_buffers
pio test --filter test_main

# Run with verbose output
pio test -v

# Run tests with detailed Unity output
pio test -v --test-port=/dev/cu.usbserial-*
```

## Test Structure

### Test Files

- **`test_main.cpp`**: Core functionality tests (3 tests)
  - Zone pin validation (ESP8266 GPIO pins)
  - Zone count verification
  - MQTT topic parsing using C-string functions

- **`test_mqtt_callback.cpp`**: MQTT callback functionality tests (8 tests)
  - Valid zone extraction from topics
  - Zone bounds checking (rejects invalid zones: 0, 8, 999, -1)
  - ON command variants (ON, on, On, 1)
  - OFF command variants (OFF, off, Off, 0)
  - Malformed topic handling
  - Oversized payload handling
  - Message truncation at buffer limits
  - Topic structure validation

- **`test_config.cpp`**: Configuration management tests (9 tests)
  - SPIFFS filesystem mounting
  - SPIFFS mount retry mechanism
  - Config save and load cycle
  - Missing field handling with defaults
  - Invalid JSON error handling
  - Port number validation (1-65535)
  - Config file size limits
  - Buffer overflow protection (strlcpy)
  - ArduinoJson buffer capacity validation

- **`test_buffers.cpp`**: Buffer safety and memory tests (9 tests)
  - snprintf truncation behavior
  - MQTT topic buffer sizing (MQTT_TOPIC_BUFFER_SIZE=64)
  - Unique ID buffer sizing (MQTT_UNIQUE_ID_BUFFER_SIZE=32)
  - Home Assistant JSON payload sizing (MQTT_PAYLOAD_BUFFER_SIZE=512)
  - Status JSON payload sizing
  - MQTT message buffer for commands (MQTT_MESSAGE_BUFFER_SIZE=8)
  - Zone name length validation
  - Combined buffer usage in publishHomeAssistantConfig
  - memcpy safety in callback message handling

## Test Coverage Summary

**Total Tests: 29 tests** across 4 test files

### Coverage by Category:

1. **Hardware Configuration** (2 tests)
   - GPIO pin validation
   - Zone count verification

2. **MQTT Protocol** (10 tests)
   - Topic parsing and validation
   - Command recognition (case-insensitive)
   - Payload handling
   - Bounds checking

3. **Configuration Management** (9 tests)
   - Filesystem operations
   - JSON serialization/deserialization
   - Error handling
   - Default values

4. **Buffer Safety** (9 tests)
   - Memory overflow protection
   - snprintf/strlcpy safety
   - JSON buffer sizing
   - Truncation handling

### Hardware Requirements

**Hardware-in-Loop Tests** (require actual ESP8266):
- All tests in this suite require actual ESP8266 hardware
- SPIFFS filesystem tests need flash memory
- GPIO pin tests validate against ESP8266 pin mappings

**Unit Tests** (logic validation without hardware interaction):
- MQTT topic parsing tests
- Buffer sizing calculations
- Command variant recognition
- JSON capacity tests

## Writing New Tests

1. Create a new test file in this directory with the format `test_feature.cpp`
2. Include the Unity framework: `#include <unity.h>`
3. Create test functions starting with `test_` prefix
4. Add your test function to the `setup()` function with `RUN_TEST(test_function)`

Example:

```cpp
#include <unity.h>
#include "../include/config.h"

void test_new_feature() {
    TEST_ASSERT_EQUAL(42, calculate_meaning_of_life());
}

void setup() {
    delay(2000);  // Allow board to settle
    Serial.begin(115200);

    UNITY_BEGIN();
    RUN_TEST(test_new_feature);
    UNITY_END();
}

void loop() {
    // Nothing to do here
}
```

### Available Unity Assertions

- `TEST_ASSERT_TRUE(condition)` / `TEST_ASSERT_FALSE(condition)`
- `TEST_ASSERT_EQUAL(expected, actual)`
- `TEST_ASSERT_EQUAL_STRING(expected, actual)`
- `TEST_ASSERT_NOT_NULL(pointer)`
- `TEST_ASSERT_NULL(pointer)`
- `TEST_ASSERT_GREATER_THAN(threshold, actual)`
- `TEST_ASSERT_LESS_THAN(threshold, actual)`
- `TEST_ASSERT_*_MESSAGE(condition, message)` - Add custom failure message

## Test Development Guidelines

1. **Focus on Critical Paths**:
   - MQTT callback (controls physical relays - safety critical)
   - Configuration validation (prevents device bricking)
   - Buffer overflows (security critical)

2. **Test Edge Cases**:
   - Minimum/maximum values
   - Empty/null inputs
   - Oversized inputs
   - Malformed data

3. **Use Informative Messages**:
   ```cpp
   TEST_ASSERT_TRUE_MESSAGE(zone > 0 && zone <= 7, "Zone must be 1-7");
   ```

4. **Test Actual Implementation**:
   - Use same functions/logic as production code
   - Don't test String objects if code uses C-strings
   - Match buffer sizes and truncation behavior

5. **Document Hardware Dependencies**:
   - Mark tests that require specific hardware
   - Note any SPIFFS/flash requirements
   - Indicate tests that need GPIO access

## Future Test Enhancements

Potential areas for additional testing:

- **WiFiManager Integration**:
  - Configuration portal timeout behavior
  - Parameter validation before save
  - Failed connection recovery

- **MQTT Connection**:
  - Reconnection logic timing
  - Last will message delivery
  - Retained message handling

- **OTA Updates**:
  - Update progress callbacks
  - Error handling during updates
  - Filesystem updates vs. sketch updates

- **Home Assistant Integration**:
  - Auto-discovery message validation
  - State synchronization
  - Availability reporting

- **Timing and Intervals**:
  - Reconnection interval enforcement
  - Status reporting timing
  - Non-blocking loop behavior

## Test Coverage Goals

Current progress toward coverage goals:
- Unit tests: ~60% code coverage (29 tests implemented)
- Integration tests: Future enhancement
- Critical path coverage: ~85% (MQTT callback, config, buffers)

Target goals:
- Unit tests: 80% code coverage
- Integration tests: 60% code coverage
- Critical path coverage: 100%
