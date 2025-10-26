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

## Security Notice

This controller stores WiFi and MQTT credentials in **plaintext** on the device's flash memory. Anyone with physical access to the device can extract these credentials.

**Security Recommendations:**
- Use dedicated IoT credentials with limited privileges
- Deploy on an isolated IoT VLAN when possible
- Each device generates a unique configuration portal password based on its chip ID
- OTA updates are password-protected (password displayed on serial console during boot)
- Do not expose the MQTT broker to the internet without TLS

**Security Features:**
- Unique OTA password per device (based on ESP8266 chip ID)
- Unique WiFi configuration portal password per device
- Configuration validation prevents corrupt settings
- Zone runtime safety limits (2-hour maximum)
- No MQTT TLS/SSL (due to memory constraints)
- No credential encryption on device (ESP8266 limitation)

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

3. Upload the sketch to your ESP8266 board

4. Follow the First-Time Setup section below for WiFi and MQTT configuration

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

2. Build and upload the project:
   
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

1. **Flash the firmware** using PlatformIO:
   ```bash
   pio run -t upload
   ```

2. **Connect to serial console** to get unique passwords:
   ```bash
   pio device monitor
   ```
   Note the displayed OTA password and Configuration Portal password.

3. **Connect to WiFi setup portal:**
   - The device will create a WiFi access point named "SprinklerSetup"
   - Password is displayed on serial console (format: sprinkler-XXXXXXXX)
   - Connect to this network with your phone/computer
   - Configuration portal will open automatically
   - Enter your WiFi credentials and MQTT broker details

4. **Device will restart** and connect to your WiFi network

5. **For OTA updates** after initial setup:
   ```bash
   # Update platformio.ini with device IP
   upload_protocol = espota
   upload_port = <device-ip>

   # Use the OTA password from step 2
   ```

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

## Troubleshooting

### Device stuck in AP mode
- Check that WiFi credentials are correct
- Verify MQTT broker is reachable from your network
- Check serial console for error messages
- If config is corrupted, device will automatically force reconfiguration

### Zones not responding to commands
- Verify MQTT broker is running
- Check that device is subscribed to `home/sprinkler/zone/+/command`
- Use MQTT client to verify messages are published
- Check serial console DEBUG output for received commands

### OTA upload fails
- Verify device IP address in platformio.ini
- Check OTA password matches (shown on serial during boot)
- Ensure device is powered on and connected to WiFi
- Try serial upload if OTA fails

### Zone runs longer than expected
- Safety limit is 2 hours - zone will auto-shutoff
- Check MQTT state is being published correctly
- Verify Home Assistant or controller is sending OFF command

### Configuration portal password doesn't work
- Password is based on chip ID and shown on serial console
- Power cycle device and check serial output
- Password format: sprinkler-XXXXXXXX (where X is chip ID in hex)

## Development Roadmap

### Version 2.0 (Current - 2025-10-26)
- ✅ Basic MQTT control of 7 zones
- ✅ Home Assistant integration
- ✅ OTA updates with device-specific passwords
- ✅ WiFi configuration portal with unique passwords
- ✅ Configuration validation and safety limits
- ✅ Buffer overflow protection
- ✅ Zone runtime safety limits (2-hour maximum)

### Version 2.1 (Planned)
- [ ] Basic scheduling functionality
- [ ] Web configuration interface
- [ ] Manual control buttons
- [ ] Status LEDs

### Version 3.0 (Future)
- [ ] Rain sensor integration
- [ ] Soil moisture sensor support
- [ ] Weather forecast integration
- [ ] Water usage tracking
- [ ] Autonomous operation if MQTT disconnects
- [ ] Mobile app
- [ ] MQTT TLS/SSL support

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