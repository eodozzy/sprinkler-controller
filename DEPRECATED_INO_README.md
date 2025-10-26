# DEPRECATION NOTICE

## The `sprinkler_controller.ino` file is DEPRECATED

This file is from the original Arduino IDE implementation and is **no longer maintained**.

### Current Implementation

The project has been migrated to **PlatformIO** with the following structure:

- **Source code**: `src/main.cpp`
- **Configuration**: `include/config.h`
- **Build system**: `platformio.ini`

### Why the Migration?

The PlatformIO implementation provides:

1. **Better memory management**: Uses stack-allocated buffers and ArduinoJson instead of String objects
2. **Enhanced security**: Dynamic AP passwords based on chip ID
3. **Improved error handling**: Retry logic for SPIFFS and MQTT operations
4. **Code quality**: Modular structure with proper separation of concerns
5. **Safety features**: Zone runtime limits (2-hour max)
6. **Better monitoring**: Enhanced status reporting with system health metrics

### What to Do

- **DO NOT** upload `sprinkler_controller.ino` to your device
- **USE** the PlatformIO build system: `pio run -t upload`
- **REFER TO** `CLAUDE.md` for current development guidelines

### Version History

- **v1.x** (DEPRECATED): Arduino IDE implementation in `sprinkler_controller.ino`
- **v2.0.0** (CURRENT): PlatformIO implementation in `src/main.cpp`

---

*For questions or issues, please refer to the project documentation.*
