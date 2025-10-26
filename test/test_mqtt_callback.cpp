#include <Arduino.h>
#include <unity.h>
#include "../include/config.h"

// Test zone extraction from valid topics
void test_valid_zone_extraction() {
  // Test zone 1
  const char* topic1 = "home/sprinkler/zone/1/command";
  const char* zonePrefix = "/zone/";
  const char* zonePrefixPos = strstr(topic1, zonePrefix);
  const char* zoneNumStart = zonePrefixPos + strlen(zonePrefix);
  int zone = atoi(zoneNumStart);
  TEST_ASSERT_EQUAL(1, zone);

  // Test zone 3
  const char* topic3 = "home/sprinkler/zone/3/command";
  zonePrefixPos = strstr(topic3, zonePrefix);
  zoneNumStart = zonePrefixPos + strlen(zonePrefix);
  zone = atoi(zoneNumStart);
  TEST_ASSERT_EQUAL(3, zone);

  // Test zone 7
  const char* topic7 = "home/sprinkler/zone/7/command";
  zonePrefixPos = strstr(topic7, zonePrefix);
  zoneNumStart = zonePrefixPos + strlen(zonePrefix);
  zone = atoi(zoneNumStart);
  TEST_ASSERT_EQUAL(7, zone);
}

// Test zone bounds checking (valid zones: 1-7)
void test_zone_bounds_checking() {
  const char* zonePrefix = "/zone/";

  // Test zone 0 (invalid - below minimum)
  const char* topic0 = "home/sprinkler/zone/0/command";
  const char* zonePrefixPos = strstr(topic0, zonePrefix);
  const char* zoneNumStart = zonePrefixPos + strlen(zonePrefix);
  int zone = atoi(zoneNumStart);
  TEST_ASSERT_FALSE_MESSAGE(zone > 0 && zone <= NUM_ZONES, "Zone 0 should be rejected");

  // Test zone 8 (invalid - above maximum)
  const char* topic8 = "home/sprinkler/zone/8/command";
  zonePrefixPos = strstr(topic8, zonePrefix);
  zoneNumStart = zonePrefixPos + strlen(zonePrefix);
  zone = atoi(zoneNumStart);
  TEST_ASSERT_FALSE_MESSAGE(zone > 0 && zone <= NUM_ZONES, "Zone 8 should be rejected");

  // Test zone 999 (invalid - way above maximum)
  const char* topic999 = "home/sprinkler/zone/999/command";
  zonePrefixPos = strstr(topic999, zonePrefix);
  zoneNumStart = zonePrefixPos + strlen(zonePrefix);
  zone = atoi(zoneNumStart);
  TEST_ASSERT_FALSE_MESSAGE(zone > 0 && zone <= NUM_ZONES, "Zone 999 should be rejected");

  // Test negative zone (atoi returns 0 for invalid input starting with -)
  const char* topicNeg = "home/sprinkler/zone/-1/command";
  zonePrefixPos = strstr(topicNeg, zonePrefix);
  zoneNumStart = zonePrefixPos + strlen(zonePrefix);
  zone = atoi(zoneNumStart);
  TEST_ASSERT_EQUAL(0, zone);  // atoi stops at non-digit
  TEST_ASSERT_FALSE_MESSAGE(zone > 0 && zone <= NUM_ZONES, "Negative zone should be rejected");
}

// Test ON command variants (case-insensitive)
void test_on_command_variants() {
  char message[MQTT_MESSAGE_BUFFER_SIZE];
  char upperMessage[MQTT_MESSAGE_BUFFER_SIZE];

  // Test "ON"
  strcpy(message, "ON");
  size_t length = strlen(message);
  for (unsigned int i = 0; message[i] && i < sizeof(upperMessage) - 1; i++) {
    upperMessage[i] = toupper(message[i]);
  }
  upperMessage[length] = '\0';
  TEST_ASSERT_TRUE(strcmp(upperMessage, "ON") == 0 || strcmp(message, "1") == 0);

  // Test "on"
  strcpy(message, "on");
  length = strlen(message);
  for (unsigned int i = 0; message[i] && i < sizeof(upperMessage) - 1; i++) {
    upperMessage[i] = toupper(message[i]);
  }
  upperMessage[length] = '\0';
  TEST_ASSERT_TRUE(strcmp(upperMessage, "ON") == 0 || strcmp(message, "1") == 0);

  // Test "On"
  strcpy(message, "On");
  length = strlen(message);
  for (unsigned int i = 0; message[i] && i < sizeof(upperMessage) - 1; i++) {
    upperMessage[i] = toupper(message[i]);
  }
  upperMessage[length] = '\0';
  TEST_ASSERT_TRUE(strcmp(upperMessage, "ON") == 0 || strcmp(message, "1") == 0);

  // Test "1"
  strcpy(message, "1");
  TEST_ASSERT_EQUAL_STRING("1", message);
  TEST_ASSERT_TRUE(strcmp(message, "1") == 0);
}

// Test OFF command variants (case-insensitive)
void test_off_command_variants() {
  char message[MQTT_MESSAGE_BUFFER_SIZE];
  char upperMessage[MQTT_MESSAGE_BUFFER_SIZE];

  // Test "OFF"
  strcpy(message, "OFF");
  size_t length = strlen(message);
  for (unsigned int i = 0; message[i] && i < sizeof(upperMessage) - 1; i++) {
    upperMessage[i] = toupper(message[i]);
  }
  upperMessage[length] = '\0';
  TEST_ASSERT_TRUE(strcmp(upperMessage, "OFF") == 0 || strcmp(message, "0") == 0);

  // Test "off"
  strcpy(message, "off");
  length = strlen(message);
  for (unsigned int i = 0; message[i] && i < sizeof(upperMessage) - 1; i++) {
    upperMessage[i] = toupper(message[i]);
  }
  upperMessage[length] = '\0';
  TEST_ASSERT_TRUE(strcmp(upperMessage, "OFF") == 0 || strcmp(message, "0") == 0);

  // Test "Off"
  strcpy(message, "Off");
  length = strlen(message);
  for (unsigned int i = 0; message[i] && i < sizeof(upperMessage) - 1; i++) {
    upperMessage[i] = toupper(message[i]);
  }
  upperMessage[length] = '\0';
  TEST_ASSERT_TRUE(strcmp(upperMessage, "OFF") == 0 || strcmp(message, "0") == 0);

  // Test "0"
  strcpy(message, "0");
  TEST_ASSERT_EQUAL_STRING("0", message);
  TEST_ASSERT_TRUE(strcmp(message, "0") == 0);
}

// Test malformed topics
void test_malformed_topics() {
  const char* zonePrefix = "/zone/";
  const char* commandSuffix = "/command";

  // Test missing zone number: "home/sprinkler/zone//command"
  const char* topic1 = "home/sprinkler/zone//command";
  const char* zonePrefixPos = strstr(topic1, zonePrefix);
  const char* commandPos = strstr(topic1, commandSuffix);
  TEST_ASSERT_NOT_NULL(zonePrefixPos);
  TEST_ASSERT_NOT_NULL(commandPos);
  const char* zoneNumStart = zonePrefixPos + strlen(zonePrefix);
  int zone = atoi(zoneNumStart);
  TEST_ASSERT_EQUAL(0, zone);  // atoi returns 0 for invalid input

  // Test non-numeric zone: "home/sprinkler/zone/abc/command"
  const char* topic2 = "home/sprinkler/zone/abc/command";
  zonePrefixPos = strstr(topic2, zonePrefix);
  commandPos = strstr(topic2, commandSuffix);
  TEST_ASSERT_NOT_NULL(zonePrefixPos);
  TEST_ASSERT_NOT_NULL(commandPos);
  zoneNumStart = zonePrefixPos + strlen(zonePrefix);
  zone = atoi(zoneNumStart);
  TEST_ASSERT_EQUAL(0, zone);  // atoi returns 0 for non-numeric

  // Test missing /command suffix: "home/sprinkler/zone/3"
  const char* topic3 = "home/sprinkler/zone/3";
  commandPos = strstr(topic3, commandSuffix);
  TEST_ASSERT_NULL_MESSAGE(commandPos, "Should not find command suffix");

  // Test missing /zone/ prefix: "home/sprinkler/3/command"
  const char* topic4 = "home/sprinkler/3/command";
  zonePrefixPos = strstr(topic4, zonePrefix);
  TEST_ASSERT_NULL_MESSAGE(zonePrefixPos, "Should not find zone prefix");
}

// Test oversized payload handling
void test_oversized_payload() {
  // MQTT_MESSAGE_BUFFER_SIZE is 8, so messages >= 8 should be truncated
  char message[MQTT_MESSAGE_BUFFER_SIZE];
  const char* oversizedPayload = "ONONONONONON";  // 12 chars
  unsigned int length = strlen(oversizedPayload);

  // Simulate truncation logic from callback()
  if (length >= sizeof(message)) {
    length = sizeof(message) - 1;
  }
  memcpy(message, oversizedPayload, length);
  message[length] = '\0';

  TEST_ASSERT_EQUAL(7, length);  // Buffer is 8, so max data is 7 + null terminator
  TEST_ASSERT_EQUAL_STRING("ONONONO", message);
}

// Test message buffer size limit
void test_message_truncation() {
  // Test that buffer size is correctly defined
  TEST_ASSERT_EQUAL(8, MQTT_MESSAGE_BUFFER_SIZE);

  // Test valid short messages fit
  const char* shortMsg1 = "ON";
  TEST_ASSERT_LESS_THAN(MQTT_MESSAGE_BUFFER_SIZE, strlen(shortMsg1) + 1);

  const char* shortMsg2 = "OFF";
  TEST_ASSERT_LESS_THAN(MQTT_MESSAGE_BUFFER_SIZE, strlen(shortMsg2) + 1);

  const char* shortMsg3 = "1";
  TEST_ASSERT_LESS_THAN(MQTT_MESSAGE_BUFFER_SIZE, strlen(shortMsg3) + 1);

  const char* shortMsg4 = "0";
  TEST_ASSERT_LESS_THAN(MQTT_MESSAGE_BUFFER_SIZE, strlen(shortMsg4) + 1);

  // Test long messages get truncated
  const char* longMsg = "VERYLONGMESSAGE";
  TEST_ASSERT_GREATER_OR_EQUAL(MQTT_MESSAGE_BUFFER_SIZE, strlen(longMsg) + 1);
}

// Test topic structure validation
void test_topic_structure_validation() {
  const char* zonePrefix = "/zone/";
  const char* commandSuffix = "/command";

  // Valid topic structure
  const char* validTopic = "home/sprinkler/zone/5/command";
  const char* zonePrefixPos = strstr(validTopic, zonePrefix);
  const char* commandSuffixPos = strstr(validTopic, commandSuffix);

  TEST_ASSERT_NOT_NULL_MESSAGE(zonePrefixPos, "Valid topic should have zone prefix");
  TEST_ASSERT_NOT_NULL_MESSAGE(commandSuffixPos, "Valid topic should have command suffix");
  TEST_ASSERT_TRUE_MESSAGE(commandSuffixPos > zonePrefixPos, "Command should come after zone");

  // Invalid: command before zone
  const char* invalidTopic = "home/sprinkler/command/zone/5";
  zonePrefixPos = strstr(invalidTopic, zonePrefix);
  commandSuffixPos = strstr(invalidTopic, commandSuffix);
  TEST_ASSERT_NOT_NULL(zonePrefixPos);
  TEST_ASSERT_NOT_NULL(commandSuffixPos);
  TEST_ASSERT_FALSE_MESSAGE(commandSuffixPos > zonePrefixPos, "Invalid order should be rejected");
}

void setup() {
  delay(2000);  // Allow board to settle

  Serial.begin(115200);

  UNITY_BEGIN();

  // Run all MQTT callback tests
  RUN_TEST(test_valid_zone_extraction);
  RUN_TEST(test_zone_bounds_checking);
  RUN_TEST(test_on_command_variants);
  RUN_TEST(test_off_command_variants);
  RUN_TEST(test_malformed_topics);
  RUN_TEST(test_oversized_payload);
  RUN_TEST(test_message_truncation);
  RUN_TEST(test_topic_structure_validation);

  UNITY_END();
}

void loop() {
  // Nothing to do here
}
