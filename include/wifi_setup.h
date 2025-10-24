#ifndef WIFI_SETUP_H
#define WIFI_SETUP_H

#include <ESP8266WiFi.h>
#include <WiFiManager.h>
#include <FS.h>
#include <ArduinoJson.h>
#include "config.h"

// External MQTT parameter storage
extern char mqtt_server[40];
extern char mqtt_port[6];
extern char mqtt_user[24];
extern char mqtt_password[24];
extern bool shouldSaveConfig;

// Forward declarations
void setupWifi();
void saveConfigCallback();
void loadConfig();

#endif // WIFI_SETUP_H
