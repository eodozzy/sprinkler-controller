#include <Arduino.h>
#include <unity.h>
#include <ArduinoJson.h>
#include "../include/config.h"

// Test snprintf truncation for topic buffers
void test_snprintf_truncation() {
  char buffer[10];  // Small buffer to test truncation

  // Test normal string that fits
  int written = snprintf(buffer, sizeof(buffer), "test");
  TEST_ASSERT_EQUAL(4, written);
  TEST_ASSERT_EQUAL_STRING("test", buffer);

  // Test string that needs truncation
  written = snprintf(buffer, sizeof(buffer), "this is a very long string");
  TEST_ASSERT_GREATER_THAN(9, written);  // Returns what would have been written
  TEST_ASSERT_EQUAL(9, strlen(buffer));  // But buffer only holds 9 chars + null

  // Test zone topic formatting with truncation protection
  char stateTopic[MQTT_TOPIC_BUFFER_SIZE];
  int zone = 5;

  written = snprintf(stateTopic, sizeof(stateTopic), "%szone/%d/state", MQTT_TOPIC_PREFIX, zone);

  // Should fit in buffer
  TEST_ASSERT_LESS_THAN(MQTT_TOPIC_BUFFER_SIZE, written + 1);
  TEST_ASSERT_LESS_THAN(MQTT_TOPIC_BUFFER_SIZE, strlen(stateTopic) + 1);
}

// Test that longest MQTT topics fit in buffers
void test_topic_buffer_sizing() {
  char buffer[MQTT_TOPIC_BUFFER_SIZE];

  // Test state topic for max zone (zone 7)
  snprintf(buffer, sizeof(buffer), "%szone/%d/state", MQTT_TOPIC_PREFIX, 7);
  size_t stateTopicLen = strlen(buffer);
  TEST_ASSERT_LESS_THAN_MESSAGE(MQTT_TOPIC_BUFFER_SIZE, stateTopicLen + 1,
                                "State topic should fit in buffer with null terminator");

  // Test command topic for max zone
  snprintf(buffer, sizeof(buffer), "%szone/%d/command", MQTT_TOPIC_PREFIX, 7);
  size_t commandTopicLen = strlen(buffer);
  TEST_ASSERT_LESS_THAN_MESSAGE(MQTT_TOPIC_BUFFER_SIZE, commandTopicLen + 1,
                                 "Command topic should fit in buffer with null terminator");

  // Test Home Assistant config topic (longest topic)
  snprintf(buffer, sizeof(buffer), "homeassistant/switch/sprinkler_zone%d/config", 7);
  size_t configTopicLen = strlen(buffer);
  TEST_ASSERT_LESS_THAN_MESSAGE(MQTT_TOPIC_BUFFER_SIZE, configTopicLen + 1,
                                 "Config topic should fit in buffer with null terminator");

  // Test status topic
  size_t statusTopicLen = strlen(MQTT_STATUS);
  TEST_ASSERT_LESS_THAN_MESSAGE(MQTT_TOPIC_BUFFER_SIZE, statusTopicLen + 1,
                                 "Status topic should fit in buffer");
}

// Test unique ID buffer sizing
void test_unique_id_buffer_sizing() {
  char uniqueId[MQTT_UNIQUE_ID_BUFFER_SIZE];

  // Test for all zones
  for (int i = 1; i <= NUM_ZONES; i++) {
    snprintf(uniqueId, sizeof(uniqueId), "sprinkler_zone%d", i);
    size_t len = strlen(uniqueId);
    TEST_ASSERT_LESS_THAN_MESSAGE(MQTT_UNIQUE_ID_BUFFER_SIZE, len + 1,
                                   "Unique ID should fit in buffer");
  }

  // Test longest possible single-digit zone (future-proofing)
  snprintf(uniqueId, sizeof(uniqueId), "sprinkler_zone%d", 9);
  TEST_ASSERT_LESS_THAN(MQTT_UNIQUE_ID_BUFFER_SIZE, strlen(uniqueId) + 1);

  // Test two-digit zone (if system expands beyond 9 zones)
  snprintf(uniqueId, sizeof(uniqueId), "sprinkler_zone%d", 10);
  TEST_ASSERT_LESS_THAN(MQTT_UNIQUE_ID_BUFFER_SIZE, strlen(uniqueId) + 1);
}

// Test Home Assistant config JSON payload sizing
void test_json_payload_sizing() {
  // Calculate buffer size with ArduinoJson Assistant (from main.cpp)
  const size_t capacity = JSON_OBJECT_SIZE(12) + 300;
  DynamicJsonDocument json(capacity);

  char configTopic[64];
  char uniqueId[32];
  char commandTopic[64];
  char stateTopic[64];

  // Build topics for zone 7 (longest)
  snprintf(configTopic, sizeof(configTopic), "homeassistant/switch/sprinkler_zone%d/config", 7);
  snprintf(uniqueId, sizeof(uniqueId), "sprinkler_zone%d", 7);
  snprintf(commandTopic, sizeof(commandTopic), "%szone/%d/command", MQTT_TOPIC_PREFIX, 7);
  snprintf(stateTopic, sizeof(stateTopic), "%szone/%d/state", MQTT_TOPIC_PREFIX, 7);

  // Create discovery payload (matches publishHomeAssistantConfig())
  json["name"] = ZONE_NAMES[6];  // Zone 7 is index 6
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

  // Test that it doesn't overflow
  TEST_ASSERT_FALSE_MESSAGE(json.overflowed(), "JSON document should not overflow");

  // Serialize to payload buffer
  char payload[MQTT_PAYLOAD_BUFFER_SIZE];
  size_t len = serializeJson(json, payload, sizeof(payload));

  TEST_ASSERT_LESS_THAN_MESSAGE(MQTT_PAYLOAD_BUFFER_SIZE, len,
                                 "Serialized payload should fit in buffer");
  TEST_ASSERT_GREATER_THAN_MESSAGE(0, len, "Payload should have content");

  // Verify payload is valid JSON
  DynamicJsonDocument testDoc(capacity);
  DeserializationError error = deserializeJson(testDoc, payload);
  TEST_ASSERT_FALSE_MESSAGE(error, "Serialized payload should be valid JSON");
}

// Test status JSON payload sizing
void test_status_json_sizing() {
  // Calculate buffer size (from publishStatus())
  const size_t capacity = JSON_OBJECT_SIZE(2) + JSON_ARRAY_SIZE(NUM_ZONES) +
                          NUM_ZONES * JSON_OBJECT_SIZE(3) + 200;
  DynamicJsonDocument json(capacity);

  json["status"] = "online";
  JsonArray zones = json.createNestedArray("zones");

  // Add all zones with longest name
  for (int i = 0; i < NUM_ZONES; i++) {
    JsonObject zone = zones.createNestedObject();
    zone["zone"] = i + 1;
    zone["name"] = ZONE_NAMES[i];
    zone["state"] = "OFF";  // OFF is longer than ON
  }

  // Test that it doesn't overflow
  TEST_ASSERT_FALSE_MESSAGE(json.overflowed(), "Status JSON should not overflow");

  // Serialize to buffer
  char statusBuffer[512];  // From publishStatus()
  size_t len = serializeJson(json, statusBuffer, sizeof(statusBuffer));

  TEST_ASSERT_LESS_THAN_MESSAGE(512, len, "Status payload should fit in 512-byte buffer");
  TEST_ASSERT_GREATER_THAN_MESSAGE(0, len, "Status should have content");

  // Verify it's valid JSON
  DynamicJsonDocument testDoc(capacity);
  DeserializationError error = deserializeJson(testDoc, statusBuffer);
  TEST_ASSERT_FALSE_MESSAGE(error, "Status payload should be valid JSON");
}

// Test MQTT message buffer for command payloads
void test_mqtt_message_buffer() {
  // MQTT_MESSAGE_BUFFER_SIZE should be 8 (from config.h)
  TEST_ASSERT_EQUAL(8, MQTT_MESSAGE_BUFFER_SIZE);

  // Test that valid commands fit
  const char* validCommands[] = {"ON", "OFF", "on", "off", "0", "1"};
  int numCommands = sizeof(validCommands) / sizeof(validCommands[0]);

  for (int i = 0; i < numCommands; i++) {
    size_t len = strlen(validCommands[i]);
    TEST_ASSERT_LESS_THAN_MESSAGE(MQTT_MESSAGE_BUFFER_SIZE, len + 1,
                                   "Valid commands should fit in message buffer");
  }

  // Test truncation of oversized message
  char message[MQTT_MESSAGE_BUFFER_SIZE];
  const char* oversized = "VERYLONGCOMMAND";
  unsigned int length = strlen(oversized);

  if (length >= sizeof(message)) {
    length = sizeof(message) - 1;
  }
  memcpy(message, oversized, length);
  message[length] = '\0';

  TEST_ASSERT_EQUAL(7, strlen(message));  // 8 - 1 for null terminator
  TEST_ASSERT_EQUAL_STRING("VERYLON", message);
}

// Test buffer safety with zone name lengths
void test_zone_name_lengths() {
  // Verify all zone names are reasonable length
  for (int i = 0; i < NUM_ZONES; i++) {
    size_t len = strlen(ZONE_NAMES[i]);
    TEST_ASSERT_LESS_THAN_MESSAGE(50, len,
                                   "Zone names should be reasonable length for JSON payload");
  }
}

// Test combined buffer usage in publishHomeAssistantConfig
void test_combined_buffer_usage() {
  // Stack buffers used in publishHomeAssistantConfig()
  char configTopic[64];
  char uniqueId[32];
  char commandTopic[64];
  char stateTopic[64];
  char payload[512];

  // Test for zone 7 (typically longest)
  int zoneNum = 7;

  snprintf(configTopic, sizeof(configTopic), "homeassistant/switch/sprinkler_zone%d/config", zoneNum);
  snprintf(uniqueId, sizeof(uniqueId), "sprinkler_zone%d", zoneNum);
  snprintf(commandTopic, sizeof(commandTopic), "%szone/%d/command", MQTT_TOPIC_PREFIX, zoneNum);
  snprintf(stateTopic, sizeof(stateTopic), "%szone/%d/state", MQTT_TOPIC_PREFIX, zoneNum);

  // Verify all fit
  TEST_ASSERT_LESS_THAN(64, strlen(configTopic) + 1);
  TEST_ASSERT_LESS_THAN(32, strlen(uniqueId) + 1);
  TEST_ASSERT_LESS_THAN(64, strlen(commandTopic) + 1);
  TEST_ASSERT_LESS_THAN(64, strlen(stateTopic) + 1);

  // Create JSON and serialize
  const size_t capacity = JSON_OBJECT_SIZE(12) + 300;
  DynamicJsonDocument json(capacity);

  json["name"] = ZONE_NAMES[zoneNum - 1];
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

  size_t len = serializeJson(json, payload, sizeof(payload));

  TEST_ASSERT_LESS_THAN(512, len);
  TEST_ASSERT_FALSE(json.overflowed());
}

// Test memcpy safety in callback message handling
void test_memcpy_safety() {
  char message[MQTT_MESSAGE_BUFFER_SIZE];
  const char* shortPayload = "ON";
  const char* longPayload = "VERYLONGMESSAGETHATEXCEEDSBUFFER";

  // Test short payload
  unsigned int length = strlen(shortPayload);
  if (length >= sizeof(message)) {
    length = sizeof(message) - 1;
  }
  memcpy(message, shortPayload, length);
  message[length] = '\0';

  TEST_ASSERT_EQUAL_STRING("ON", message);

  // Test long payload with truncation
  length = strlen(longPayload);
  TEST_ASSERT_GREATER_OR_EQUAL(sizeof(message), length);  // Verify it's actually long

  if (length >= sizeof(message)) {
    length = sizeof(message) - 1;
  }
  memcpy(message, longPayload, length);
  message[length] = '\0';

  TEST_ASSERT_EQUAL(7, strlen(message));  // Should be truncated
  TEST_ASSERT_LESS_THAN(sizeof(message), strlen(message) + 1);
}

void setup() {
  delay(2000);  // Allow board to settle

  Serial.begin(115200);

  UNITY_BEGIN();

  // Run all buffer safety tests
  RUN_TEST(test_snprintf_truncation);
  RUN_TEST(test_topic_buffer_sizing);
  RUN_TEST(test_unique_id_buffer_sizing);
  RUN_TEST(test_json_payload_sizing);
  RUN_TEST(test_status_json_sizing);
  RUN_TEST(test_mqtt_message_buffer);
  RUN_TEST(test_zone_name_lengths);
  RUN_TEST(test_combined_buffer_usage);
  RUN_TEST(test_memcpy_safety);

  UNITY_END();
}

void loop() {
  // Nothing to do here
}
