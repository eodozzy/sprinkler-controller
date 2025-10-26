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

// Callback for MQTT messages
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

// Load saved configuration from filesystem
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

// Callback notifying us of the need to save config
void saveConfigCallback() {
  DEBUG_PRINTLN("Should save config");
  shouldSaveConfig = true;
}

// Setup WiFi connection using WiFiManager
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
  
  // Set custom AP name and optional password
  bool connected = wifiManager.autoConnect(AP_SSID, AP_PASSWORD);
  
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

// Setup OTA updates
void setupOTA() {
  // Port defaults to 8266
  ArduinoOTA.setPort(8266);

  // Hostname defaults to esp8266-[ChipID]
  ArduinoOTA.setHostname("sprinkler-controller");

  // No authentication by default
  // ArduinoOTA.setPassword("admin");

  ArduinoOTA.onStart([]() {
    String type;
    if (ArduinoOTA.getCommand() == U_FLASH) {
      type = "sketch";
    } else { // U_FS
      type = "filesystem";
    }
    DEBUG_PRINTLN("Start updating " + type);
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

// Attempt to reconnect to MQTT
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

// Publish Home Assistant MQTT discovery configurations
void publishHomeAssistantConfig() {
  // Calculate buffer size with ArduinoJson Assistant
  const size_t capacity = JSON_OBJECT_SIZE(12) + 300;

  // Stack buffers for topic construction
  char configTopic[64];
  char uniqueId[32];
  char commandTopic[64];
  char stateTopic[64];
  char payload[512];  // Buffer for serialized JSON

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

    // Serialize json to buffer and publish
    size_t len = serializeJson(json, payload, sizeof(payload));
    if (len < sizeof(payload)) {
      mqtt.publish(configTopic, payload, true);
    } else {
      DEBUG_PRINTLN("Warning: Home Assistant config payload truncated");
    }
  }
}

// Publish status information
void publishStatus() {
  // Calculate buffer size with ArduinoJson Assistant
  const size_t capacity = JSON_OBJECT_SIZE(2) + JSON_ARRAY_SIZE(NUM_ZONES) + NUM_ZONES * JSON_OBJECT_SIZE(3) + 200;
  DynamicJsonDocument json(capacity);

  json["status"] = "online";

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
    
    // Publish status periodically
    unsigned long now = millis();
    if (now - lastStatusReport > STATUS_INTERVAL) {
      lastStatusReport = now;
      publishStatus();
    }
  }
}
