/*
 * SECURITY NOTICE:
 * ================
 * This firmware stores MQTT credentials in plaintext on the SPIFFS filesystem.
 * Anyone with physical access to the device can read these credentials via USB.
 * This is a known limitation of the current architecture.
 *
 * Mitigations in place:
 * - OTA updates require chip-specific password (see serial output)
 * - Configuration portal uses unique password per device
 * - Network credentials managed by ESP8266 WiFi core (encrypted)
 *
 * For production deployments, consider:
 * - Physical security of devices
 * - Network isolation (separate VLAN for IoT devices)
 * - MQTT broker authentication and TLS
 * - Regular security audits
 */

#include <Arduino.h>
#include <FS.h>
#include "config.h"
#include "wifi_setup.h"
#include "mqtt_handler.h"
#include "ota_setup.h"

// MQTT connection parameters
char mqtt_server[40] = "";
char mqtt_port[6] = "1883";
char mqtt_user[24] = "";
char mqtt_password[24] = "";

// Client objects
WiFiClient espClient;
PubSubClient mqtt(espClient);

// Timing variables
unsigned long lastReconnectAttempt = 0;
unsigned long lastStatusReport = 0;

// Flag for WiFiManager reset
bool shouldSaveConfig = false;

// Zone runtime tracking for safety limits
unsigned long zone_on_time[NUM_ZONES] = {0};

/**
 * MQTT message callback - handles incoming zone control commands
 *
 * @param topic The MQTT topic the message was received on
 * @param payload Raw byte array containing the message payload
 * @param length Number of bytes in the payload
 *
 * Side effects:
 * - Parses zone number from topic (expects format: home/sprinkler/zone/N/command)
 * - Controls GPIO pins to turn zones ON/OFF based on payload ("ON", "OFF", "1", "0")
 * - Publishes state confirmation back to MQTT state topic
 */
void callback(char* topic, byte* payload, unsigned int length) {
  // Use stack buffer for message (longest valid message is "OFF" = 3 chars)
  char message[MQTT_MESSAGE_BUFFER_SIZE];
  if (length >= sizeof(message)) {
    DEBUG_PRINTLN("Warning: Message too long, truncating");
    length = sizeof(message) - 1;
  }
  memcpy(message, payload, length);
  message[length] = '\0';

  DEBUG_PRINT("Message arrived [");
  DEBUG_PRINT(topic);
  DEBUG_PRINT("] ");
  DEBUG_PRINTLN(message);

  // Extract zone number using C string functions (no String objects)
  const char* zonePrefix = "/zone/";
  const char* zonePrefixPos = strstr(topic, zonePrefix);
  const char* commandSuffix = strstr(topic, "/command");

  if (zonePrefixPos && commandSuffix && commandSuffix > zonePrefixPos) {
    const char* zoneNumStart = zonePrefixPos + strlen(zonePrefix);
    int zone = atoi(zoneNumStart);  // Stops at first non-digit (the '/')

    if (zone > 0 && zone <= NUM_ZONES) {
      int zoneIndex = zone - 1;

      // Build state topic using stack buffer
      char stateTopic[MQTT_TOPIC_BUFFER_SIZE];
      snprintf(stateTopic, sizeof(stateTopic), "%szone/%d/state", MQTT_TOPIC_PREFIX, zone);

      // Case-insensitive comparison without String
      char upperMessage[MQTT_MESSAGE_BUFFER_SIZE];
      for (unsigned int i = 0; message[i] && i < sizeof(upperMessage) - 1; i++) {
        upperMessage[i] = toupper(message[i]);
      }
      upperMessage[length] = '\0';

      if (strcmp(upperMessage, "ON") == 0 || strcmp(message, "1") == 0) {
        digitalWrite(ZONE_PINS[zoneIndex], HIGH);
        mqtt.publish(stateTopic, "ON", true);
        DEBUG_PRINT("Turning ON zone ");
        DEBUG_PRINTLN(zone);
      } else if (strcmp(upperMessage, "OFF") == 0 || strcmp(message, "0") == 0) {
        digitalWrite(ZONE_PINS[zoneIndex], LOW);
        mqtt.publish(stateTopic, "OFF", true);
        DEBUG_PRINT("Turning OFF zone ");
        DEBUG_PRINTLN(zone);
      }
    }
  }
}

/**
 * Load MQTT configuration from SPIFFS filesystem
 *
 * Reads /config.json and populates mqtt_server, mqtt_port, mqtt_user, mqtt_password
 * global variables. Implements retry logic for transient SPIFFS mount failures.
 *
 * Side effects:
 * - Mounts SPIFFS filesystem (retries up to 3 times)
 * - Reads and parses /config.json if it exists
 * - Updates global MQTT configuration variables
 * - Outputs debug messages via Serial
 */
void loadConfig() {
  DEBUG_PRINTLN("Mounting file system...");

  // Try mounting SPIFFS with retry for transient errors
  bool mounted = false;
  int retries = 3;
  while (!mounted && retries > 0) {
    mounted = SPIFFS.begin();
    if (!mounted) {
      DEBUG_PRINTF("SPIFFS mount failed, retrying... (%d attempts left)\n", retries);
      delay(500);
      retries--;
    }
  }

  if (mounted) {
    DEBUG_PRINTLN("Mounted file system");
    if (SPIFFS.exists("/config.json")) {
      // File exists, reading and loading
      DEBUG_PRINTLN("Reading config file");
      File configFile = SPIFFS.open("/config.json", "r");
      if (configFile) {
        DEBUG_PRINTLN("Opened config file");
        size_t size = configFile.size();
        // Allocate a buffer to store contents of the file.
        std::unique_ptr<char[]> buf(new char[size]);

        configFile.readBytes(buf.get(), size);

        // Calculate buffer size with ArduinoJson Assistant
        const size_t capacity = JSON_OBJECT_SIZE(4) + 100;
        DynamicJsonDocument json(capacity);
        DeserializationError error = deserializeJson(json, buf.get());
        
        if (!error) {
          DEBUG_PRINTLN("Parsed json");
          strlcpy(mqtt_server, json["mqtt_server"] | "", sizeof(mqtt_server));
          strlcpy(mqtt_port, json["mqtt_port"] | "1883", sizeof(mqtt_port));
          strlcpy(mqtt_user, json["mqtt_user"] | "", sizeof(mqtt_user));
          strlcpy(mqtt_password, json["mqtt_password"] | "", sizeof(mqtt_password));

          // Validate loaded configuration
          bool config_valid = (strlen(mqtt_server) > 0 &&
                              strlen(mqtt_port) > 0 &&
                              atoi(mqtt_port) > 0 &&
                              atoi(mqtt_port) <= 65535);

          if (!config_valid) {
            DEBUG_PRINTLN("Config validation failed - will force reconfiguration on next WiFi setup");
            // Clear invalid config
            mqtt_server[0] = '\0';
          }
        } else {
          DEBUG_PRINTLN("Failed to load json config");
        }
        configFile.close();
      }
    } else {
      DEBUG_PRINTLN("Config file not found - first boot or reset");
    }
  } else {
    DEBUG_PRINTLN("Failed to mount file system after retries - filesystem may be corrupted");
  }
}

/**
 * Callback to flag that WiFiManager configuration needs to be saved
 *
 * Called by WiFiManager when user submits new configuration via web portal.
 *
 * Side effects:
 * - Sets shouldSaveConfig global flag to true
 */
void saveConfigCallback() {
  DEBUG_PRINTLN("Should save config");
  shouldSaveConfig = true;
}

/**
 * Setup WiFi connection and configure MQTT parameters via WiFiManager
 *
 * Implements a captive portal for WiFi and MQTT configuration. Loads existing
 * config from SPIFFS, presents web UI for changes, saves updated config back to
 * filesystem. Forces configuration portal if no valid MQTT server is configured.
 *
 * Side effects:
 * - Calls loadConfig() to read saved configuration
 * - Starts WiFiManager captive portal (SSID: "SprinklerSetup")
 * - Connects to WiFi network
 * - Updates global MQTT configuration variables
 * - Saves configuration to /config.json if changed
 * - Restarts ESP8266 if connection fails or times out
 */
void setupWifi() {
  delay(10);
  DEBUG_PRINTLN();
  // Load saved configuration first
  loadConfig();

  DEBUG_PRINTLN("Setting up WiFi and MQTT params...");

  // The extra parameters to be configured
  WiFiManagerParameter custom_mqtt_server("server", "MQTT Server", mqtt_server, 40);
  WiFiManagerParameter custom_mqtt_port("port", "MQTT Port", mqtt_port, 6);
  WiFiManagerParameter custom_mqtt_user("user", "MQTT User", mqtt_user, 24);
  WiFiManagerParameter custom_mqtt_password("password", "MQTT Password", mqtt_password, 24, "password");

  // WiFiManager
  WiFiManager wifiManager;

  // Generate unique AP password from chip ID for security
  char ap_password[32];
  snprintf(ap_password, sizeof(ap_password), "sprinkler-%08X", ESP.getChipId());

  DEBUG_PRINTLN("=================================");
  DEBUG_PRINT("Configuration Portal Password: ");
  DEBUG_PRINTLN(ap_password);
  DEBUG_PRINTLN("=================================");

  // Check if we have valid configuration - force portal if empty
  if (mqtt_server[0] == '\0') {
    DEBUG_PRINTLN("No valid config found, forcing configuration portal");
    wifiManager.resetSettings();
  }

  // Set callback for saving configuration
  wifiManager.setSaveConfigCallback(saveConfigCallback);

  // Add all your parameters here
  wifiManager.addParameter(&custom_mqtt_server);
  wifiManager.addParameter(&custom_mqtt_port);
  wifiManager.addParameter(&custom_mqtt_user);
  wifiManager.addParameter(&custom_mqtt_password);

  // Set timeout for the configuration portal
  wifiManager.setConfigPortalTimeout(CONFIG_PORTAL_TIMEOUT);

  // Reset saved settings - uncomment to test
  //wifiManager.resetSettings();

  // Set custom AP name with unique password (not hardcoded)
  bool connected = wifiManager.autoConnect(AP_SSID, ap_password);
  
  if (!connected) {
    DEBUG_PRINTLN("Failed to connect and hit timeout");
    // Reset and try again
    ESP.restart();
  }
  
  // Read updated parameters
  strlcpy(mqtt_server, custom_mqtt_server.getValue(), sizeof(mqtt_server));
  strlcpy(mqtt_port, custom_mqtt_port.getValue(), sizeof(mqtt_port));
  strlcpy(mqtt_user, custom_mqtt_user.getValue(), sizeof(mqtt_user));
  strlcpy(mqtt_password, custom_mqtt_password.getValue(), sizeof(mqtt_password));
  
  DEBUG_PRINTLN("WiFi connected");
  DEBUG_PRINT("IP address: ");
  DEBUG_PRINTLN(WiFi.localIP());
  
  // Save the custom parameters to file system
  if (shouldSaveConfig) {
    DEBUG_PRINTLN("Saving config to /config.json");
    
    // Calculate buffer size with ArduinoJson Assistant
    const size_t capacity = JSON_OBJECT_SIZE(4) + 100;
    DynamicJsonDocument json(capacity);
    
    json["mqtt_server"] = mqtt_server;
    json["mqtt_port"] = mqtt_port;
    json["mqtt_user"] = mqtt_user;
    json["mqtt_password"] = mqtt_password;

    File configFile = SPIFFS.open("/config.json", "w");
    if (!configFile) {
      DEBUG_PRINTLN("Failed to open config file for writing");
    } else {
      serializeJson(json, configFile);
      configFile.close();
      DEBUG_PRINTLN("Config saved successfully");
    }
  }
}

/**
 * Configure Over-The-Air (OTA) firmware update functionality
 *
 * Sets up ArduinoOTA with hostname "sprinkler-controller" on port 8266.
 * Registers callbacks for update progress and error reporting.
 *
 * Side effects:
 * - Configures OTA hostname and port
 * - Registers event handlers for OTA updates
 * - Calls ArduinoOTA.begin()
 */
void setupOTA() {
  // Port defaults to 8266
  ArduinoOTA.setPort(8266);

  // Hostname defaults to esp8266-[ChipID]
  ArduinoOTA.setHostname("sprinkler-controller");

  // Generate unique OTA password from chip ID for security
  char ota_password[16];
  snprintf(ota_password, sizeof(ota_password), "%08X", ESP.getChipId());
  ArduinoOTA.setPassword(ota_password);
  DEBUG_PRINTLN("=================================");
  DEBUG_PRINT("OTA Password: ");
  DEBUG_PRINTLN(ota_password);
  DEBUG_PRINTLN("=================================");

  ArduinoOTA.onStart([]() {
    // Use stack buffer instead of String to avoid heap allocation
    const char* type;
    if (ArduinoOTA.getCommand() == U_FLASH) {
      type = "sketch";
    } else { // U_FS
      type = "filesystem";
    }
    DEBUG_PRINT("Start updating ");
    DEBUG_PRINTLN(type);
  });
  
  ArduinoOTA.onEnd([]() {
    DEBUG_PRINTLN("\nEnd");
  });
  
  ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
    DEBUG_PRINTF("Progress: %u%%\r", (progress / (total / 100)));
  });
  
  ArduinoOTA.onError([](ota_error_t error) {
    DEBUG_PRINTF("Error[%u]: ", error);
    if (error == OTA_AUTH_ERROR) {
      DEBUG_PRINTLN("Auth Failed");
    } else if (error == OTA_BEGIN_ERROR) {
      DEBUG_PRINTLN("Begin Failed");
    } else if (error == OTA_CONNECT_ERROR) {
      DEBUG_PRINTLN("Connect Failed");
    } else if (error == OTA_RECEIVE_ERROR) {
      DEBUG_PRINTLN("Receive Failed");
    } else if (error == OTA_END_ERROR) {
      DEBUG_PRINTLN("End Failed");
    }
  });
  
  ArduinoOTA.begin();
}

/**
 * Attempt to connect/reconnect to MQTT broker
 *
 * Validates and parses MQTT port, establishes connection with last will testament,
 * subscribes to zone command topics, and publishes current state of all zones.
 *
 * @return true if connected to MQTT broker, false otherwise
 *
 * Side effects:
 * - Configures MQTT server and port
 * - Connects to MQTT broker with "offline" last will on status topic
 * - Subscribes to "home/sprinkler/zone/+/command"
 * - Publishes "online" to status topic
 * - Publishes current state of all zones
 * - Calls publishHomeAssistantConfig() for auto-discovery
 */
bool reconnectMqtt() {
  // Validate and convert port number, use default if invalid
  int mqtt_port_int = (mqtt_port[0] != '\0') ? atoi(mqtt_port) : 1883;
  if (mqtt_port_int <= 0 || mqtt_port_int > 65535) {
    mqtt_port_int = 1883;  // Fallback to default MQTT port
    DEBUG_PRINTLN("Invalid MQTT port, using default 1883");
  }

  // Configure MQTT buffer size for large payloads (Home Assistant discovery)
  mqtt.setBufferSize(512);
  mqtt.setServer(mqtt_server, mqtt_port_int);
  
  if (mqtt.connect(MQTT_CLIENT_ID, mqtt_user, mqtt_password, MQTT_STATUS, 0, true, "offline")) {
    DEBUG_PRINTLN("MQTT connected");
    
    // Subscribe to zone commands
    mqtt.subscribe(MQTT_ZONE_COMMAND);
    
    // Publish that we're online
    mqtt.publish(MQTT_STATUS, "online", true);
    
    // Publish current state of all zones
    char stateTopic[64];
    for (int i = 0; i < NUM_ZONES; i++) {
      snprintf(stateTopic, sizeof(stateTopic), "%szone/%d/state", MQTT_TOPIC_PREFIX, i+1);
      mqtt.publish(stateTopic, digitalRead(ZONE_PINS[i]) == HIGH ? "ON" : "OFF", true);
    }
    
    // Publish zone configurations for Home Assistant auto-discovery
    publishHomeAssistantConfig();
  }
  return mqtt.connected();
}

/**
 * Publish Home Assistant MQTT auto-discovery configurations for all zones
 *
 * Sends discovery messages for each zone switch to enable automatic integration
 * with Home Assistant. Includes device information for grouping all zones under
 * a single device in the HA UI.
 *
 * Side effects:
 * - Publishes 7 discovery messages to homeassistant/switch/sprinkler_zoneN/config
 * - Each message contains switch configuration, MQTT topics, and device metadata
 * - Device information includes chip ID, model, manufacturer, and software version
 */
void publishHomeAssistantConfig() {
  // Calculate buffer size with ArduinoJson Assistant (includes device object)
  const size_t capacity = JSON_OBJECT_SIZE(12) + JSON_OBJECT_SIZE(5) + 400;

  // Stack buffers for topic construction
  char configTopic[64];
  char uniqueId[32];
  char commandTopic[64];
  char stateTopic[64];
  char payload[512];  // Buffer for serialized JSON
  char deviceId[16];

  // Generate device ID once for all zones
  snprintf(deviceId, sizeof(deviceId), "%08X", ESP.getChipId());

  for (int i = 0; i < NUM_ZONES; i++) {
    int zoneNum = i + 1;

    // Build all topic strings using stack buffers
    snprintf(configTopic, sizeof(configTopic), "homeassistant/switch/sprinkler_zone%d/config", zoneNum);
    snprintf(uniqueId, sizeof(uniqueId), "sprinkler_zone%d", zoneNum);
    snprintf(commandTopic, sizeof(commandTopic), "%szone/%d/command", MQTT_TOPIC_PREFIX, zoneNum);
    snprintf(stateTopic, sizeof(stateTopic), "%szone/%d/state", MQTT_TOPIC_PREFIX, zoneNum);

    // Create discovery payload using ArduinoJson
    DynamicJsonDocument json(capacity);

    json["name"] = ZONE_NAMES[i];
    json["unique_id"] = uniqueId;
    json["command_topic"] = commandTopic;
    json["state_topic"] = stateTopic;
    json["availability_topic"] = MQTT_STATUS;
    json["payload_on"] = "ON";
    json["payload_off"] = "OFF";
    json["state_on"] = "ON";
    json["state_off"] = "OFF";
    json["optimistic"] = false;
    json["qos"] = 0;
    json["retain"] = true;

    // Add device information for Home Assistant
    JsonObject device = json.createNestedObject("device");
    device["name"] = "Sprinkler Controller";
    device["identifiers"] = deviceId;
    device["model"] = "ESP8266 NodeMCU";
    device["manufacturer"] = "DIY";
    device["sw_version"] = SW_VERSION;

    // Serialize json to buffer and publish
    size_t len = serializeJson(json, payload, sizeof(payload));
    if (len < sizeof(payload)) {
      mqtt.publish(configTopic, payload, true);
    } else {
      DEBUG_PRINTLN("Warning: Home Assistant config payload truncated");
    }
  }
}

/**
 * Publish comprehensive status information to MQTT
 *
 * Sends a JSON payload containing system health metrics (uptime, free memory,
 * WiFi signal strength, chip ID) and current state of all zones.
 *
 * Side effects:
 * - Publishes JSON status message to home/sprinkler/status topic
 * - Message includes: status, uptime, free_heap, wifi_rssi, chip_id, zones array
 * - Each zone in array includes: zone number, name, and current state (ON/OFF)
 */
void publishStatus() {
  // Calculate buffer size with ArduinoJson Assistant
  const size_t capacity = JSON_OBJECT_SIZE(6) + JSON_ARRAY_SIZE(NUM_ZONES) + NUM_ZONES * JSON_OBJECT_SIZE(3) + 300;
  DynamicJsonDocument json(capacity);

  json["status"] = "online";
  json["uptime"] = millis() / 1000;  // seconds
  json["free_heap"] = ESP.getFreeHeap();
  json["wifi_rssi"] = WiFi.RSSI();
  char chipId[16];
  snprintf(chipId, sizeof(chipId), "%08X", ESP.getChipId());
  json["chip_id"] = chipId;

  JsonArray zones = json.createNestedArray("zones");

  for (int i = 0; i < NUM_ZONES; i++) {
    JsonObject zone = zones.createNestedObject();
    zone["zone"] = i + 1;
    zone["name"] = ZONE_NAMES[i];
    zone["state"] = digitalRead(ZONE_PINS[i]) == HIGH ? "ON" : "OFF";
  }

  // Serialize json to buffer and publish
  char statusBuffer[512];
  size_t len = serializeJson(json, statusBuffer, sizeof(statusBuffer));
  if (len < sizeof(statusBuffer)) {
    mqtt.publish(MQTT_STATUS, statusBuffer, true);
  } else {
    DEBUG_PRINTLN("Warning: Status payload truncated");
  }
}

// Main setup function
void setup() {
  Serial.begin(115200);
  DEBUG_PRINTLN("\nStarting Sprinkler Controller");
  
  // Initialize all zone pins as outputs and set to LOW (off)
  for (int i = 0; i < NUM_ZONES; i++) {
    pinMode(ZONE_PINS[i], OUTPUT);
    digitalWrite(ZONE_PINS[i], LOW);
    DEBUG_PRINT("Initialized zone ");
    DEBUG_PRINT(i+1);
    DEBUG_PRINT(" (");
    DEBUG_PRINT(ZONE_NAMES[i]);
    DEBUG_PRINTLN(") as OFF");
  }
  
  setupWifi();

  // Enable light sleep for power savings (~20mA reduction)
  WiFi.setSleepMode(WIFI_LIGHT_SLEEP);
  DEBUG_PRINTLN("WiFi light sleep enabled");

  setupOTA();

  // Set up MQTT callback
  mqtt.setCallback(callback);
  
  lastReconnectAttempt = 0;
}

// Main loop function
void loop() {
  // Handle OTA updates
  ArduinoOTA.handle();
  
  // Handle MQTT connection
  if (!mqtt.connected()) {
    unsigned long now = millis();
    if (now - lastReconnectAttempt > RECONNECT_INTERVAL) {
      lastReconnectAttempt = now;
      // Attempt to reconnect
      if (reconnectMqtt()) {
        lastReconnectAttempt = 0;
      }
    }
  } else {
    // Client connected
    mqtt.loop();

    // Safety check: enforce maximum zone runtime
    for (int i = 0; i < NUM_ZONES; i++) {
      if (digitalRead(ZONE_PINS[i]) == HIGH) {
        if (zone_on_time[i] == 0) {
          zone_on_time[i] = millis();
        } else if (millis() - zone_on_time[i] > MAX_ZONE_RUNTIME) {
          digitalWrite(ZONE_PINS[i], LOW);
          DEBUG_PRINTF("Zone %d safety timeout - forced OFF after %d seconds\n",
                       i+1, MAX_ZONE_RUNTIME/1000);
          // Publish state update
          char stateTopic[MQTT_TOPIC_BUFFER_SIZE];
          snprintf(stateTopic, sizeof(stateTopic), "%szone/%d/state", MQTT_TOPIC_PREFIX, i+1);
          mqtt.publish(stateTopic, "OFF", true);
          zone_on_time[i] = 0;
        }
      } else {
        zone_on_time[i] = 0;  // Reset timer when zone is off
      }
    }

    // Publish status periodically
    unsigned long now = millis();
    if (now - lastStatusReport > STATUS_INTERVAL) {
      lastStatusReport = now;
      publishStatus();
    }
  }
}
