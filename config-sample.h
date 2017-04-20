/*
 * This is a sample configuration file for the "steveo_light".
 *
 * Change the settings below and save the file as "config.h"
 * You can then upload the code using the Arduino IDE.
 */

// WiFi
#define CONFIG_WIFI_SSID "{WIFI-SSID}"
#define CONFIG_WIFI_PASS "{WIFI-PASSWORD}"

// MQTT
#define CONFIG_MQTT_HOST "{MQTT-SERVER}"
#define CONFIG_MQTT_PORT 1883

#define CONFIG_MQTT_CLIENT_ID "{UNIQUE-CLIENT-ID}" // Must be unique on the MQTT network

// MQTT Topics
#define CONFIG_MQTT_TOPIC_STATE "{TOPIC}/{TOPIC}"
#define CONFIG_MQTT_TOPIC_SET "{TOPIC}/{TOPIC}/set"

#define CONFIG_MQTT_PAYLOAD_ON "ON"
#define CONFIG_MQTT_PAYLOAD_OFF "OFF"

// OTA
#define CONFIG_OTA_PASS "{OTA-PASSWORD}"
#define CONFIG_OTA_PORT 8266
