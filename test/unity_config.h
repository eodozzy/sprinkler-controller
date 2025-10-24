#ifndef UNITY_CONFIG_H
#define UNITY_CONFIG_H

// Unity configuration for ESP8266 tests
// See Unity documentation for more options: https://github.com/ThrowTheSwitch/Unity

// Define the width of pointers for the target
#ifndef UNITY_POINTER_WIDTH
    #define UNITY_POINTER_WIDTH 32
#endif

// Enable float assertions
#ifndef UNITY_INCLUDE_FLOAT
    #define UNITY_INCLUDE_FLOAT
#endif

// Enable double assertions
#ifndef UNITY_INCLUDE_DOUBLE
    #define UNITY_INCLUDE_DOUBLE
#endif

#endif // UNITY_CONFIG_H
