#include <Arduino.h>
#include <unity.h>
#include <FS.h>
#include <ArduinoJson.h>
#include "../include/config.h"

// Test SPIFFS mount functionality
void test_spiffs_mount() {
  // Try mounting SPIFFS
  bool mounted = SPIFFS.begin();
  TEST_ASSERT_TRUE_MESSAGE(mounted, "SPIFFS should mount successfully");

  if (mounted) {
    SPIFFS.end();
  }
}

// Test SPIFFS mount retry mechanism
void test_spiffs_mount_retry() {
  // This test validates the retry logic structure
  // Actual retry behavior requires hardware testing

  bool mounted = false;
  int retries = 3;
  int attemptCount = 0;

  while (!mounted && retries > 0) {
    attemptCount++;
    mounted = SPIFFS.begin();
    if (!mounted) {
      retries--;
      delay(100);  // Short delay for testing
    }
  }

  // Should have attempted at least once
  TEST_ASSERT_GREATER_OR_EQUAL(1, attemptCount);
  TEST_ASSERT_LESS_OR_EQUAL(3, attemptCount);

  if (mounted) {
    SPIFFS.end();
  }
}

// Test config file save and load
void test_config_save_and_load() {
  // Mount SPIFFS
  bool mounted = SPIFFS.begin();
  TEST_ASSERT_TRUE_MESSAGE(mounted, "SPIFFS must mount for config tests");

  // Test data
  const char* testServer = "mqtt.test.com";
  const char* testPort = "8883";
  const char* testUser = "testuser";
  const char* testPassword = "testpass";

  // Create JSON config
  const size_t capacity = JSON_OBJECT_SIZE(4) + 100;
  DynamicJsonDocument json(capacity);

  json["mqtt_server"] = testServer;
  json["mqtt_port"] = testPort;
  json["mqtt_user"] = testUser;
  json["mqtt_password"] = testPassword;

  // Save to file
  File configFile = SPIFFS.open("/test_config.json", "w");
  TEST_ASSERT_TRUE_MESSAGE(configFile, "Should be able to create config file");

  serializeJson(json, configFile);
  configFile.close();

  // Verify file exists
  TEST_ASSERT_TRUE_MESSAGE(SPIFFS.exists("/test_config.json"), "Config file should exist");

  // Load from file
  configFile = SPIFFS.open("/test_config.json", "r");
  TEST_ASSERT_TRUE_MESSAGE(configFile, "Should be able to open config file");

  size_t size = configFile.size();
  TEST_ASSERT_GREATER_THAN(0, size);

  std::unique_ptr<char[]> buf(new char[size]);
  configFile.readBytes(buf.get(), size);
  configFile.close();

  // Parse JSON
  DynamicJsonDocument loadedJson(capacity);
  DeserializationError error = deserializeJson(loadedJson, buf.get());

  TEST_ASSERT_FALSE_MESSAGE(error, "JSON should parse without error");
  TEST_ASSERT_EQUAL_STRING(testServer, loadedJson["mqtt_server"]);
  TEST_ASSERT_EQUAL_STRING(testPort, loadedJson["mqtt_port"]);
  TEST_ASSERT_EQUAL_STRING(testUser, loadedJson["mqtt_user"]);
  TEST_ASSERT_EQUAL_STRING(testPassword, loadedJson["mqtt_password"]);

  // Cleanup
  SPIFFS.remove("/test_config.json");
  SPIFFS.end();
}

// Test handling of missing fields in config
void test_config_missing_fields() {
  bool mounted = SPIFFS.begin();
  TEST_ASSERT_TRUE_MESSAGE(mounted, "SPIFFS must mount for config tests");

  // Create config with missing fields
  const size_t capacity = JSON_OBJECT_SIZE(2) + 50;
  DynamicJsonDocument json(capacity);

  json["mqtt_server"] = "mqtt.test.com";
  // Intentionally omit mqtt_port, mqtt_user, mqtt_password

  File configFile = SPIFFS.open("/test_missing.json", "w");
  TEST_ASSERT_TRUE(configFile);
  serializeJson(json, configFile);
  configFile.close();

  // Load and parse
  configFile = SPIFFS.open("/test_missing.json", "r");
  size_t size = configFile.size();
  std::unique_ptr<char[]> buf(new char[size]);
  configFile.readBytes(buf.get(), size);
  configFile.close();

  DynamicJsonDocument loadedJson(capacity);
  DeserializationError error = deserializeJson(loadedJson, buf.get());

  TEST_ASSERT_FALSE(error);

  // Test default values using the | operator pattern from loadConfig()
  char mqtt_server[40];
  char mqtt_port[6];
  char mqtt_user[24];
  char mqtt_password[24];

  strlcpy(mqtt_server, loadedJson["mqtt_server"] | "", sizeof(mqtt_server));
  strlcpy(mqtt_port, loadedJson["mqtt_port"] | "1883", sizeof(mqtt_port));
  strlcpy(mqtt_user, loadedJson["mqtt_user"] | "", sizeof(mqtt_user));
  strlcpy(mqtt_password, loadedJson["mqtt_password"] | "", sizeof(mqtt_password));

  TEST_ASSERT_EQUAL_STRING("mqtt.test.com", mqtt_server);
  TEST_ASSERT_EQUAL_STRING("1883", mqtt_port);  // Should use default
  TEST_ASSERT_EQUAL_STRING("", mqtt_user);      // Should be empty
  TEST_ASSERT_EQUAL_STRING("", mqtt_password);  // Should be empty

  // Cleanup
  SPIFFS.remove("/test_missing.json");
  SPIFFS.end();
}

// Test handling of invalid JSON
void test_config_invalid_json() {
  bool mounted = SPIFFS.begin();
  TEST_ASSERT_TRUE_MESSAGE(mounted, "SPIFFS must mount for config tests");

  // Create file with invalid JSON
  File configFile = SPIFFS.open("/test_invalid.json", "w");
  TEST_ASSERT_TRUE(configFile);
  configFile.print("{\"mqtt_server\": \"test.com\", invalid json here}");
  configFile.close();

  // Try to load and parse
  configFile = SPIFFS.open("/test_invalid.json", "r");
  size_t size = configFile.size();
  std::unique_ptr<char[]> buf(new char[size]);
  configFile.readBytes(buf.get(), size);
  configFile.close();

  const size_t capacity = JSON_OBJECT_SIZE(4) + 100;
  DynamicJsonDocument json(capacity);
  DeserializationError error = deserializeJson(json, buf.get());

  // Should have a parse error
  TEST_ASSERT_TRUE_MESSAGE(error, "Invalid JSON should produce error");

  // Cleanup
  SPIFFS.remove("/test_invalid.json");
  SPIFFS.end();
}

// Test port validation logic
void test_config_port_validation() {
  // Test valid ports
  const char* validPort1 = "1883";
  int port1 = atoi(validPort1);
  TEST_ASSERT_TRUE(port1 > 0 && port1 <= 65535);

  const char* validPort2 = "8883";
  int port2 = atoi(validPort2);
  TEST_ASSERT_TRUE(port2 > 0 && port2 <= 65535);

  // Test invalid ports
  const char* invalidPort1 = "0";
  int port3 = atoi(invalidPort1);
  TEST_ASSERT_FALSE(port3 > 0 && port3 <= 65535);

  const char* invalidPort2 = "99999";
  int port4 = atoi(invalidPort2);
  TEST_ASSERT_FALSE(port4 > 0 && port4 <= 65535);

  const char* invalidPort3 = "-100";
  int port5 = atoi(invalidPort3);
  TEST_ASSERT_FALSE(port5 > 0 && port5 <= 65535);

  const char* invalidPort4 = "abc";
  int port6 = atoi(invalidPort4);
  TEST_ASSERT_FALSE(port6 > 0 && port6 <= 65535);

  // Test empty string (should use default 1883)
  const char* emptyPort = "";
  int port7 = (emptyPort[0] != '\0') ? atoi(emptyPort) : 1883;
  TEST_ASSERT_EQUAL(1883, port7);
}

// Test config file size limits
void test_config_file_size() {
  bool mounted = SPIFFS.begin();
  TEST_ASSERT_TRUE_MESSAGE(mounted, "SPIFFS must mount for config tests");

  // Create reasonably sized config
  const size_t capacity = JSON_OBJECT_SIZE(4) + 100;
  DynamicJsonDocument json(capacity);

  json["mqtt_server"] = "mqtt.example.com";
  json["mqtt_port"] = "1883";
  json["mqtt_user"] = "testuser";
  json["mqtt_password"] = "testpassword";

  // Serialize to buffer to check size
  char buffer[200];
  size_t len = serializeJson(json, buffer, sizeof(buffer));

  // Config should fit in reasonable buffer (ArduinoJson Assistant recommends ~100 bytes)
  TEST_ASSERT_LESS_THAN(200, len);
  TEST_ASSERT_GREATER_THAN(0, len);

  SPIFFS.end();
}

// Test buffer overflow protection in strlcpy
void test_config_buffer_protection() {
  char mqtt_server[40];
  char mqtt_port[6];

  // Test normal strings fit
  strlcpy(mqtt_server, "mqtt.example.com", sizeof(mqtt_server));
  TEST_ASSERT_EQUAL_STRING("mqtt.example.com", mqtt_server);

  strlcpy(mqtt_port, "1883", sizeof(mqtt_port));
  TEST_ASSERT_EQUAL_STRING("1883", mqtt_port);

  // Test oversized string gets truncated
  const char* longServer = "this.is.a.very.long.server.name.that.exceeds.buffer.size.example.com";
  strlcpy(mqtt_server, longServer, sizeof(mqtt_server));
  TEST_ASSERT_EQUAL(39, strlen(mqtt_server));  // Buffer is 40, so max 39 + null

  const char* longPort = "123456789";
  strlcpy(mqtt_port, longPort, sizeof(mqtt_port));
  TEST_ASSERT_EQUAL(5, strlen(mqtt_port));  // Buffer is 6, so max 5 + null
}

// Test ArduinoJson buffer capacity
void test_json_buffer_capacity() {
  // Test that our calculated capacity is sufficient
  const size_t capacity = JSON_OBJECT_SIZE(4) + 100;
  DynamicJsonDocument json(capacity);

  json["mqtt_server"] = "mqtt.example.com";
  json["mqtt_port"] = "1883";
  json["mqtt_user"] = "username";
  json["mqtt_password"] = "password";

  // Should not overflow
  TEST_ASSERT_FALSE(json.overflowed());

  // Serialize and check
  char buffer[200];
  size_t len = serializeJson(json, buffer, sizeof(buffer));
  TEST_ASSERT_GREATER_THAN(0, len);
  TEST_ASSERT_LESS_THAN(200, len);
}

void setup() {
  delay(2000);  // Allow board to settle

  Serial.begin(115200);

  UNITY_BEGIN();

  // Run all config tests
  RUN_TEST(test_spiffs_mount);
  RUN_TEST(test_spiffs_mount_retry);
  RUN_TEST(test_config_save_and_load);
  RUN_TEST(test_config_missing_fields);
  RUN_TEST(test_config_invalid_json);
  RUN_TEST(test_config_port_validation);
  RUN_TEST(test_config_file_size);
  RUN_TEST(test_config_buffer_protection);
  RUN_TEST(test_json_buffer_capacity);

  UNITY_END();
}

void loop() {
  // Nothing to do here
}
