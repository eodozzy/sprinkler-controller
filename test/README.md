# Sprinkler Controller Tests

This directory contains unit tests for the Sprinkler Controller project.

## Running Tests

Tests can be run using PlatformIO:

```bash
# Run all tests
pio test

# Run specific test environment
pio test -e test

# Run with verbose output
pio test -v
```

## Test Structure

- `test_main.cpp`: Main test file containing core functionality tests

## Writing New Tests

1. Create a new test file in this directory with the format `test_feature.cpp`
2. Include the Unity framework: `#include <unity.h>`
3. Create test functions starting with `test_` prefix
4. Add your test function to the `setup()` function with `RUN_TEST(test_function)`

Example:

```cpp
#include <unity.h>

void test_new_feature() {
    TEST_ASSERT_EQUAL(42, calculate_meaning_of_life());
}

void setup() {
    UNITY_BEGIN();
    RUN_TEST(test_new_feature);
    UNITY_END();
}

void loop() {}
```

## Test Coverage

Current test coverage includes:
- Basic functionality validation
- MQTT topic parsing
- Zone configuration

Future tests should include:
- WiFi connection process
- MQTT message handling
- Configuration storage/retrieval
