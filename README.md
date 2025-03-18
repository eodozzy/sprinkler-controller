# ESP8266 MQTT Sprinkler Controller

A simple yet expandable sprinkler control system built on ESP8266 that communicates with Home Assistant/Node-Red via MQTT.

## Features

- Controls 7 sprinkler zones via relays
- WiFiManager for easy network configuration (no hardcoded credentials)
- Web configuration portal for WiFi and MQTT settings
- Integrates with Home Assistant via MQTT auto-discovery
- OTA (Over-The-Air) firmware updates
- Automatic reconnection to WiFi and MQTT
- Status reporting to track zone states
- Designed for easy expansion with future features

## Hardware Requirements

- ESP8266 board (NodeMCU, Wemos D1 Mini, etc.)
- 7-channel relay board (or individual relays)
- 12V AC power source (for sprinkler valves)
- 5V DC buck converter (to power the ESP8266)
- Connecting wires

## Wiring Diagram

```
Power Supply:
12V AC Input → Sprinkler Valves
12V AC Input → Buck Converter → 5V DC → ESP8266

Control Connections:
ESP8266 GPIO 5 (D1)  → Relay 1 → Zone 1 Valve
ESP8266 GPIO 4 (D2)  → Relay 2 → Zone 2 Valve
ESP8266 GPIO 14 (D5) → Relay 3 → Zone 3 Valve
ESP8266 GPIO 12 (D6) → Relay 4 → Zone 4 Valve
ESP8266 GPIO 13 (D7) → Relay 5 → Zone 5 Valve
ESP8266 GPIO 15 (D8) → Relay 6 → Zone 6 Valve
ESP8266 GPIO 16 (D0) → Relay 7 → Zone 7 Valve
```

## Software Setup

This project supports both Arduino IDE and PlatformIO development environments.

### Arduino IDE

#### Prerequisites

1. Arduino IDE with ESP8266 support installed
2. Required libraries:
   - ESP8266WiFi
   - PubSubClient
   - ESP8266mDNS
   - ArduinoOTA

#### Installation

1. Clone this repository:
   ```
   git clone https://github.com/yourusername/sprinkler-controller.git
   ```

2. Open the `sprinkler_controller.ino` file in Arduino IDE

3. Edit the configuration settings:
   - WiFi credentials
   - MQTT broker details
   - Zone names (if needed)

4. Upload the sketch to your ESP8266 board

### PlatformIO

This project can be built using either the PlatformIO CLI or VSCode with PlatformIO extension.

#### Prerequisites

- **CLI method**: PlatformIO Core installed (`pip install -U platformio`)
- **VSCode method**: VSCode with PlatformIO extension installed
- Git (optional, for version control)

#### Installation

1. Clone this repository:
   ```
   git clone https://github.com/yourusername/sprinkler-controller.git
   ```

2. Edit the configuration settings in `src/main.cpp`:
   - WiFi credentials
   - MQTT broker details
   - Zone names (if needed)

3. Build and upload the project:
   
   **CLI method**:
   ```
   cd "Sprinkler Controller"
   pio run -t upload
   pio device monitor
   ```
   
   **VSCode method**:
   - Open the project folder in VSCode with PlatformIO
   - Use the PlatformIO toolbar for building and uploading
   
4. For detailed CLI usage, see the `PLATFORMIO_CLI.md` file

#### OTA Updates with PlatformIO

After the initial upload via USB, you can enable OTA updates:

1. Note the IP address displayed in the serial monitor
2. Uncomment and update the following lines in `platformio.ini`:
   ```
   upload_protocol = espota
   upload_port = 192.168.1.x  ; Replace with your ESP8266's IP
   ```
3. Subsequent uploads will happen over WiFi

### MQTT Topics

- **Commands**: `home/sprinkler/zone/{1-7}/command` (payload: "ON" or "OFF")
- **Status**: `home/sprinkler/zone/{1-7}/state` (payload: "ON" or "OFF")
- **Controller Status**: `home/sprinkler/status` (payload: "online" or "offline")

## First-Time Setup

This project uses WiFiManager for easy network configuration without hardcoding credentials.

1. When first powered on, the controller will create a WiFi access point named "SprinklerSetup"

2. Connect to this WiFi network using password "sprinklerconfig"

3. A captive portal should automatically open (or navigate to 192.168.4.1)

4. Configure the following settings:
   - Your home WiFi network credentials
   - MQTT broker details (server, port, username, password)

5. After saving, the device will connect to your WiFi network and MQTT broker

6. The configuration is saved to flash memory and will be used on subsequent boots

### Reset Configuration

If you need to reset the WiFi or MQTT settings:

1. Uncomment the line `wifiManager.resetSettings();` in the code and upload once
2. Re-comment the line and upload again
3. The device will reset to AP mode for reconfiguration

## Home Assistant Integration

The controller automatically configures entities in Home Assistant using MQTT discovery. After powering on, you'll find the zones as switches in Home Assistant that you can control directly or via automation.

Example automation in Home Assistant:

```yaml
automation:
  - alias: "Water Front Lawn Every Morning"
    trigger:
      platform: time
      at: "06:00:00"
    action:
      - service: switch.turn_on
        entity_id: switch.front_lawn
      - delay: "00:15:00"
      - service: switch.turn_off
        entity_id: switch.front_lawn
```

## Development Roadmap

### Version 1.0 (Current)
- ✅ Basic MQTT control of 7 zones
- ✅ Home Assistant integration
- ✅ OTA updates

### Version 1.1 (Planned)
- [ ] Basic scheduling functionality
- [ ] Web configuration interface
- [ ] Manual control buttons
- [ ] Status LEDs

### Version 2.0 (Future)
- [ ] Rain sensor integration
- [ ] Soil moisture sensor support
- [ ] Weather forecast integration
- [ ] Water usage tracking
- [ ] Autonomous operation if MQTT disconnects
- [ ] Mobile app

## Git Version Control

This project uses the following branching strategy:

```
main             - Stable releases
├── develop      - Development branch
├── feature/*    - Feature branches
├── bugfix/*     - Bug fix branches
└── release/*    - Release preparation
```

## Contributing

1. Fork the repository
2. Create your feature branch: `git checkout -b feature/amazing-feature`
3. Commit your changes: `git commit -m 'Add some amazing feature'`
4. Push to the branch: `git push origin feature/amazing-feature`
5. Open a Pull Request

## License

This project is licensed under the MIT License - see the LICENSE file for details.

## Acknowledgments

- Home Assistant community
- PubSubClient library developers
- ESP8266 community