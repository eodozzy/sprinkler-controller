# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

ESP8266-based MQTT sprinkler controller that integrates with Home Assistant. Controls 7 sprinkler zones via relays, uses WiFiManager for configuration portal (no hardcoded credentials), and supports OTA firmware updates.

## Build System

This project uses PlatformIO. Both CLI and VSCode extension workflows are supported.

### Common Commands

```bash
# Build the project
pio run

# Upload to device via USB
pio run -t upload

# Monitor serial output (115200 baud)
pio device monitor

# Build, upload, and monitor in one step
pio run -t upload && pio device monitor

# Clean build artifacts
pio run -t clean

# Install/update dependencies
pio pkg install

# List connected devices
pio device list
```

### Running Tests

```bash
# Run all tests
pio test

# Run specific test environment
pio test -e test

# Run tests with verbose output
pio test -v
```

### OTA Updates

After initial USB upload:
1. Note the device IP from serial monitor
2. Edit `platformio.ini` and uncomment:
   ```ini
   upload_protocol = espota
   upload_port = 192.168.1.x  ; Replace with actual IP
   ```
3. Upload normally: `pio run -t upload`

## Architecture

### Code Organization

The project follows a modular structure with clear separation of concerns:

- **`src/main.cpp`**: Main application logic, orchestrates all subsystems. Contains setup/loop functions and integrates WiFi, MQTT, and OTA functionality.
- **`include/config.h`**: Central configuration - pin mappings, zone names, MQTT topics, intervals, debug flags. Single source of truth for all configurable parameters.
- **`include/wifi_setup.h`**: WiFi and configuration management using WiFiManager library.
- **`include/mqtt_handler.h`**: MQTT communication and Home Assistant auto-discovery protocol.
- **`include/ota_setup.h`**: Over-the-air update functionality declarations.
- **`test/test_main.cpp`**: Unit tests using Unity framework.

### Key Architectural Patterns

**Configuration Management**:
- WiFi/MQTT credentials stored in SPIFFS filesystem (`/config.json`)
- Uses ArduinoJson for serialization/deserialization
- WiFiManager provides captive portal for user configuration (AP: "SprinklerSetup", password generated from chip ID: "sprinkler-XXXXXXXX")
- Config persists across reboots

**MQTT Integration**:
- Command topic pattern: `home/sprinkler/zone/{1-7}/command` (accepts ON/OFF)
- State topic pattern: `home/sprinkler/zone/{1-7}/state` (publishes ON/OFF)
- Status topic: `home/sprinkler/status` (online/offline with last will)
- Home Assistant auto-discovery: publishes to `homeassistant/switch/sprinkler_zone{N}/config`
- Retained messages ensure state persistence

**Timing and Reconnection**:
- MQTT reconnection attempts every 5 seconds (`RECONNECT_INTERVAL`)
- Status published every 60 seconds (`STATUS_INTERVAL`)
- Non-blocking reconnection logic to prevent loop() freezing

**Hardware Control**:
- 7 GPIO pins mapped to relay outputs (D1, D2, D5-D8, D0)
- Zone state controlled via `digitalWrite(ZONE_PINS[index], HIGH/LOW)`
- All zones initialized to LOW (off) at startup

### Important Implementation Details

**All functionality is in `src/main.cpp`**: Despite having header files for modularity, the actual implementations of WiFi setup, MQTT handling, and OTA setup are currently all in `main.cpp`. The headers only contain forward declarations. When modifying functionality:
- Edit functions in `src/main.cpp`
- Update corresponding header if changing function signatures

**MQTT Callback Flow**:
1. Incoming message triggers `callback()` function
2. Topic parsed to extract zone number using string manipulation
3. Payload compared against ON/OFF variants (case-insensitive)
4. GPIO pin toggled and state published back to MQTT

**WiFiManager Integration**:
- Custom parameters added for MQTT server, port, username, password
- `shouldSaveConfig` flag triggers filesystem write on successful connection
- Configuration portal timeout: 180 seconds

**Debug Output**:
- Controlled by `DEBUG` flag in `config.h`
- Uses conditional macros (`DEBUG_PRINT`, `DEBUG_PRINTLN`, `DEBUG_PRINTF`)
- All debug output goes to Serial at 115200 baud

## Security Considerations

When working with this codebase, be aware of the following security characteristics:

**Authentication:**
- OTA updates use device-specific password (ESP.getChipId())
- WiFi configuration portal uses unique password per device
- MQTT authentication via username/password

**Credential Storage:**
- Credentials stored in plaintext in `/config.json` on SPIFFS
- No encryption available due to ESP8266 memory constraints
- Physical access = credential extraction risk

**Input Validation:**
- All MQTT payloads bounded and validated
- Zone numbers checked against NUM_ZONES constant
- Buffer overflow protection via strlcpy/snprintf

**Safety Features:**
- Zone runtime limited to MAX_ZONE_RUNTIME (2 hours)
- Configuration validation prevents invalid states
- SPIFFS mount retry logic for resilience

When modifying security-sensitive code, maintain these principles:
1. Never store additional credentials in plaintext
2. Always bounds-check buffer operations
3. Validate all user/network input
4. Use device-specific values for passwords (ESP.getChipId())
5. Log security events at DEBUG level

## Hardware Configuration

Pin assignments are defined in `include/config.h`:
```cpp
const int ZONE_PINS[NUM_ZONES] = {5, 4, 14, 12, 13, 15, 16};
// Maps to ESP8266 labels: D1, D2, D5, D6, D7, D8, D0
```

Zone names are customizable in the same file:
```cpp
const char* ZONE_NAMES[NUM_ZONES] = {
  "Front Lawn", "Back Lawn", "Garden", "Side Yard",
  "Flower Bed", "Drip System", "Extra Zone"
};
```

## MQTT Topics Reference

- Commands: `home/sprinkler/zone/{1-7}/command` → "ON" or "OFF"
- State feedback: `home/sprinkler/zone/{1-7}/state` → "ON" or "OFF"
- Controller status: `home/sprinkler/status` → "online" or JSON status object
- Zone subscription wildcard: `home/sprinkler/zone/+/command`

## Development Notes

**ArduinoJson Usage**: Buffer sizes are calculated using the ArduinoJson Assistant recommendations. When modifying JSON structures, recalculate capacity with `JSON_OBJECT_SIZE()` macros.

**SPIFFS Filesystem**: Used for persistent storage. Must be properly mounted before reading/writing config. SPIFFS is initialized in `loadConfig()`.

**Testing Strategy**: Tests use Unity framework. Current tests cover configuration validation and MQTT topic parsing. Tests run on the actual hardware (ESP8266), not in a simulator.

**Reset Configuration**: To force WiFiManager to restart the configuration portal, uncomment `wifiManager.resetSettings()` in `setupWifi()`, upload once, then re-comment and upload again.
