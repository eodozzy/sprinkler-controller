# PlatformIO CLI Guide for Sprinkler Controller

This guide covers basic PlatformIO CLI commands for developing the Sprinkler Controller project.

## Prerequisites

1. Install PlatformIO Core (CLI):
   ```
   pip install -U platformio
   ```

2. Verify installation:
   ```
   pio --version
   ```

## Common Commands

### Project Management

- Initialize project (already done in this repo):
  ```
  pio init --board nodemcuv2
  ```

- Install dependencies:
  ```
  pio pkg install
  ```

### Building and Uploading

- Build the project:
  ```
  pio run
  ```

- Upload to device via USB:
  ```
  pio run -t upload
  ```

- Monitor serial output:
  ```
  pio device monitor --baud 115200
  ```

- Build and upload in one command:
  ```
  pio run -t upload && pio device monitor
  ```

### Over-the-Air (OTA) Updates

After the initial upload via USB, you can enable OTA updates:

1. Note the IP address displayed in the serial monitor
2. Edit the `platformio.ini` file to uncomment and update:
   ```
   upload_protocol = espota
   upload_port = 192.168.1.x  ; Replace with your ESP8266's IP
   ```
3. Use the standard upload command:
   ```
   pio run -t upload
   ```

### Library Management

- Search for libraries:
  ```
  pio pkg search "library name"
  ```

- Install a library:
  ```
  pio pkg install "library name"
  ```

- List installed libraries:
  ```
  pio pkg list
  ```

### Project Information

- Show project information:
  ```
  pio project config
  ```

- List connected devices:
  ```
  pio device list
  ```

### Cleaning

- Clean build files:
  ```
  pio run -t clean
  ```

## Development Workflow Example

1. Edit code in `src/main.cpp`
2. Build project: `pio run`
3. Upload to device: `pio run -t upload`
4. Monitor output: `pio device monitor`
5. Repeat

## Advanced Usage

- Specify a specific environment:
  ```
  pio run -e nodemcuv2
  ```

- Build specific environments:
  ```
  pio run -e nodemcuv2 -t upload
  ```

- Debug information:
  ```
  pio run --verbose
  ```

For more information, see the [PlatformIO CLI documentation](https://docs.platformio.org/en/latest/core/index.html).
