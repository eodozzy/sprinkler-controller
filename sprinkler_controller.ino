#include <FS.h>          // File system for ESP8266
#include <ESP8266WiFi.h>
#include <PubSubClient.h>
#include <ESP8266mDNS.h>
#include <ArduinoOTA.h>
#include <WiFiManager.h>  // https://github.com/tzapu/WiFiManager
#include <ArduinoJson.h>  // https://github.com/bblanchon/ArduinoJson

// WiFiManager AP settings (when no WiFi is configured)
const char* ap_ssid = "SprinklerSetup";     // AP name for configuration portal
const char* ap_password = "sprinklerconfig"; // AP password, empty for open network

// MQTT broker details
char mqtt_server[40] = "";
char mqtt_port[6] = "1883";
char mqtt_user[24] = "";
char mqtt_password[24] = "";
const char* client_id = "sprinkler_controller";

// MQTT topics
const char* topic_prefix = "home/sprinkler/";
const char* topic_zone_command = "home/sprinkler/zone/+/command";
const char* topic_status = "home/sprinkler/status";

// Zone configuration - adjust according to your setup
const int NUM_ZONES = 7;
// ESP8266 usable GPIO pins: D1(5), D2(4), D5(14), D6(12), D7(13), D8(15), D0(16)
const int zonePins[NUM_ZONES] = {5, 4, 14, 12, 13, 15, 16}; 
const char* zoneNames[NUM_ZONES] = {
  "Front Lawn", 
  "Back Lawn", 
  "Garden", 
  "Side Yard", 
  "Flower Bed", 
  "Drip System", 
  "Extra Zone"
};

WiFiClient espClient;
PubSubClient mqtt(espClient);

unsigned long lastReconnectAttempt = 0;
unsigned long lastStatusReport = 0;
const unsigned long STATUS_INTERVAL = 60000; // Status update every 60 seconds

// Flag for WiFiManager reset
bool shouldSaveConfig = false;

// Callback notifying us of the need to save config
void saveConfigCallback() {
  Serial.println("Should save config");
  shouldSaveConfig = true;
}

// Function to load saved configuration
void loadConfig() {
  Serial.println("Mounting file system...");
  
  if (SPIFFS.begin()) {
    Serial.println("Mounted file system");
    if (SPIFFS.exists("/config.json")) {
      // File exists, reading and loading
      Serial.println("Reading config file");
      File configFile = SPIFFS.open("/config.json", "r");
      if (configFile) {
        Serial.println("Opened config file");
        size_t size = configFile.size();
        // Allocate a buffer to store contents of the file.
        std::unique_ptr<char[]> buf(new char[size]);

        configFile.readBytes(buf.get(), size);

        DynamicJsonDocument json(1024);
        DeserializationError error = deserializeJson(json, buf.get());
        
        if (!error) {
          Serial.println("Parsed json");
          strcpy(mqtt_server, json["mqtt_server"]);
          strcpy(mqtt_port, json["mqtt_port"]);
          strcpy(mqtt_user, json["mqtt_user"]);
          strcpy(mqtt_password, json["mqtt_password"]);
        } else {
          Serial.println("Failed to load json config");
        }
        configFile.close();
      }
    }
  } else {
    Serial.println("Failed to mount file system");
  }
}

void setup_wifi() {
  delay(10);
  Serial.println();
  // Load saved configuration first
  loadConfig();
  Serial.println("Setting up WiFi and MQTT params...");

  // The extra parameters to be configured (can be either global or just in the setup)
  WiFiManagerParameter custom_mqtt_server("server", "MQTT Server", mqtt_server, 40);
  WiFiManagerParameter custom_mqtt_port("port", "MQTT Port", mqtt_port, 6);
  WiFiManagerParameter custom_mqtt_user("user", "MQTT User", mqtt_user, 24);
  WiFiManagerParameter custom_mqtt_password("password", "MQTT Password", mqtt_password, 24, "password");

  // WiFiManager
  // Local initialization. Once its business is done, there is no need to keep it around
  WiFiManager wifiManager;

  // Set callback for saving configuration
  wifiManager.setSaveConfigCallback(saveConfigCallback);
  
  // Add all your parameters here
  wifiManager.addParameter(&custom_mqtt_server);
  wifiManager.addParameter(&custom_mqtt_port);
  wifiManager.addParameter(&custom_mqtt_user);
  wifiManager.addParameter(&custom_mqtt_password);
  
  // Set timeout for the configuration portal
  wifiManager.setConfigPortalTimeout(180); // 3 minutes timeout
  
  // Reset saved settings - uncomment to test
  //wifiManager.resetSettings();
  
  // Set custom AP name and optional password
  bool connected = wifiManager.autoConnect(ap_ssid, ap_password);
  
  if (!connected) {
    Serial.println("Failed to connect and hit timeout");
    // Reset and try again
    ESP.restart();
    delay(5000);
  }
  
  // Read updated parameters
  strcpy(mqtt_server, custom_mqtt_server.getValue());
  strcpy(mqtt_port, custom_mqtt_port.getValue());
  strcpy(mqtt_user, custom_mqtt_user.getValue());
  strcpy(mqtt_password, custom_mqtt_password.getValue());
  
  Serial.println("WiFi connected");
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());
  
  // Save the custom parameters to file system
  if (shouldSaveConfig) {
    Serial.println("Saving config to /config.json");
    DynamicJsonDocument json(1024);
    json["mqtt_server"] = mqtt_server;
    json["mqtt_port"] = mqtt_port;
    json["mqtt_user"] = mqtt_user;
    json["mqtt_password"] = mqtt_password;

    File configFile = SPIFFS.open("/config.json", "w");
    if (!configFile) {
      Serial.println("Failed to open config file for writing");
    } else {
      serializeJson(json, configFile);
      configFile.close();
      Serial.println("Config saved successfully");
    }
  }
}

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
    Serial.println("Start updating " + type);
  });
  
  ArduinoOTA.onEnd([]() {
    Serial.println("\nEnd");
  });
  
  ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
    Serial.printf("Progress: %u%%\r", (progress / (total / 100)));
  });
  
  ArduinoOTA.onError([](ota_error_t error) {
    Serial.printf("Error[%u]: ", error);
    if (error == OTA_AUTH_ERROR) {
      Serial.println("Auth Failed");
    } else if (error == OTA_BEGIN_ERROR) {
      Serial.println("Begin Failed");
    } else if (error == OTA_CONNECT_ERROR) {
      Serial.println("Connect Failed");
    } else if (error == OTA_RECEIVE_ERROR) {
      Serial.println("Receive Failed");
    } else if (error == OTA_END_ERROR) {
      Serial.println("End Failed");
    }
  });
  
  ArduinoOTA.begin();
}

void callback(char* topic, byte* payload, unsigned int length) {
  String message;
  for (unsigned int i = 0; i < length; i++) {
    message += (char)payload[i];
  }
  
  Serial.print("Message arrived [");
  Serial.print(topic);
  Serial.print("] ");
  Serial.println(message);

  // Extract zone number from topic
  String topicStr = String(topic);
  int zoneStartPos = topicStr.indexOf("/zone/") + 6;
  int zoneEndPos = topicStr.indexOf("/command");
  
  if (zoneStartPos > 6 && zoneEndPos > zoneStartPos) {
    String zoneStr = topicStr.substring(zoneStartPos, zoneEndPos);
    int zone = zoneStr.toInt();
    
    if (zone > 0 && zone <= NUM_ZONES) {
      int zoneIndex = zone - 1;
      
      if (message == "ON" || message == "on" || message == "1") {
        digitalWrite(zonePins[zoneIndex], HIGH);
        mqtt.publish((String(topic_prefix) + "zone/" + zone + "/state").c_str(), "ON", true);
        Serial.print("Turning ON zone ");
        Serial.println(zone);
      } else if (message == "OFF" || message == "off" || message == "0") {
        digitalWrite(zonePins[zoneIndex], LOW);
        mqtt.publish((String(topic_prefix) + "zone/" + zone + "/state").c_str(), "OFF", true);
        Serial.print("Turning OFF zone ");
        Serial.println(zone);
      }
    }
  }
}

boolean reconnect() {
  int mqtt_port_int = atoi(mqtt_port);
  mqtt.setServer(mqtt_server, mqtt_port_int);
  
  if (mqtt.connect(client_id, mqtt_user, mqtt_password, topic_status, 0, true, "offline")) {
    Serial.println("MQTT connected");
    
    // Subscribe to zone commands
    mqtt.subscribe(topic_zone_command);
    
    // Publish that we're online
    mqtt.publish(topic_status, "online", true);
    
    // Publish current state of all zones
    for (int i = 0; i < NUM_ZONES; i++) {
      String stateTopic = String(topic_prefix) + "zone/" + String(i+1) + "/state";
      mqtt.publish(stateTopic.c_str(), digitalRead(zonePins[i]) == HIGH ? "ON" : "OFF", true);
    }
    
    // Publish zone configurations for Home Assistant auto-discovery
    publishHomeAssistantConfig();
  }
  return mqtt.connected();
}

void publishHomeAssistantConfig() {
  // Publish configuration for each zone to support Home Assistant MQTT discovery
  for (int i = 0; i < NUM_ZONES; i++) {
    String zoneNum = String(i + 1);
    String configTopic = "homeassistant/switch/sprinkler_zone" + zoneNum + "/config";
    
    // Create discovery payload
    String payload = "{";
    payload += "\"name\":\"" + String(zoneNames[i]) + "\",";
    payload += "\"unique_id\":\"sprinkler_zone" + zoneNum + "\",";
    payload += "\"command_topic\":\"" + String(topic_prefix) + "zone/" + zoneNum + "/command\",";
    payload += "\"state_topic\":\"" + String(topic_prefix) + "zone/" + zoneNum + "/state\",";
    payload += "\"availability_topic\":\"" + String(topic_status) + "\",";
    payload += "\"payload_on\":\"ON\",";
    payload += "\"payload_off\":\"OFF\",";
    payload += "\"state_on\":\"ON\",";
    payload += "\"state_off\":\"OFF\",";
    payload += "\"optimistic\":false,";
    payload += "\"qos\":0,";
    payload += "\"retain\":true";
    payload += "}";
    
    mqtt.publish(configTopic.c_str(), payload.c_str(), true);
  }
}

void publishStatus() {
  String status = "{\"status\":\"online\",\"zones\":[";
  
  for (int i = 0; i < NUM_ZONES; i++) {
    if (i > 0) status += ",";
    status += "{\"zone\":" + String(i+1) + ",";
    status += "\"name\":\"" + String(zoneNames[i]) + "\",";
    status += "\"state\":\"" + String(digitalRead(zonePins[i]) == HIGH ? "ON" : "OFF") + "\"}";
  }
  
  status += "]}";
  mqtt.publish(topic_status, status.c_str(), true);
}

void setup() {
  Serial.begin(115200);
  Serial.println("\nStarting Sprinkler Controller");
  
  // Initialize all zone pins as outputs and set to LOW (off)
  for (int i = 0; i < NUM_ZONES; i++) {
    pinMode(zonePins[i], OUTPUT);
    digitalWrite(zonePins[i], LOW);
    Serial.print("Initialized zone ");
    Serial.print(i+1);
    Serial.print(" (");
    Serial.print(zoneNames[i]);
    Serial.println(") as OFF");
  }
  
  setup_wifi();
  setupOTA();
  
  // MQTT server is set in reconnect() function now
  // Convert mqtt_port string to int
  int mqtt_port_int = atoi(mqtt_port);
  mqtt.setServer(mqtt_server, mqtt_port_int);
  mqtt.setCallback(callback);
  
  lastReconnectAttempt = 0;
}

void loop() {
  // Handle OTA updates
  ArduinoOTA.handle();
  
  // Handle MQTT connection
  if (!mqtt.connected()) {
    unsigned long now = millis();
    if (now - lastReconnectAttempt > 5000) {
      lastReconnectAttempt = now;
      // Attempt to reconnect
      if (reconnect()) {
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