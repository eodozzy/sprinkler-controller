#ifndef MQTT_HANDLER_H
#define MQTT_HANDLER_H

#include <PubSubClient.h>
#include <ArduinoJson.h>
#include "config.h"

// External MQTT client and parameters
extern PubSubClient mqtt;
extern char mqtt_server[40];
extern char mqtt_port[6];
extern char mqtt_user[24];
extern char mqtt_password[24];

// Forward declarations
void callback(char* topic, byte* payload, unsigned int length);
boolean reconnectMqtt();
void publishHomeAssistantConfig();
void publishStatus();

#endif // MQTT_HANDLER_H
