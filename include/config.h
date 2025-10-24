#ifndef CONFIG_H
#define CONFIG_H

// Debug configuration
#define DEBUG true  // Set to false to disable debug output

// WiFiManager AP settings
#define AP_SSID "SprinklerSetup"
#define AP_PASSWORD "sprinklerconfig"

// MQTT settings
#define MQTT_CLIENT_ID "sprinkler_controller"
#define MQTT_DEFAULT_PORT "1883"

// MQTT topics
#define MQTT_TOPIC_PREFIX "home/sprinkler/"
#define MQTT_ZONE_COMMAND "home/sprinkler/zone/+/command"
#define MQTT_STATUS "home/sprinkler/status"

// Timer intervals (milliseconds)
#define RECONNECT_INTERVAL 5000
#define STATUS_INTERVAL 60000
#define CONFIG_PORTAL_TIMEOUT 180  // Seconds

// Hardware configuration
#define NUM_ZONES 7

// Pin mapping for zones (ESP8266 GPIO pins)
const int ZONE_PINS[NUM_ZONES] = {5, 4, 14, 12, 13, 15, 16};

// Zone names
const char* ZONE_NAMES[NUM_ZONES] = {
  "Front Lawn", 
  "Back Lawn", 
  "Garden", 
  "Side Yard", 
  "Flower Bed", 
  "Drip System", 
  "Extra Zone"
};

// Debug macros
#if DEBUG
  #define DEBUG_PRINT(x) Serial.print(x)
  #define DEBUG_PRINTLN(x) Serial.println(x)
  #define DEBUG_PRINTF(x, ...) Serial.printf(x, __VA_ARGS__)
#else
  #define DEBUG_PRINT(x)
  #define DEBUG_PRINTLN(x)
  #define DEBUG_PRINTF(x, ...)
#endif

#endif // CONFIG_H
