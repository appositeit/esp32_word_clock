#ifndef CONFIG_H
#define CONFIG_H

// LED Matrix Configuration
#define NUM_LEDS 64
#define DATA_PIN 10

// Light Sensor Configuration
#define LIGHT_SENSOR_PIN 2  // ADC pin for light sensor
#define LIGHT_SAMPLES 10    // Number of samples to average

// WiFi Configuration
#define WIFI_AP_NAME "WordClock-AP"     // Name when in AP mode
#define WIFI_AP_PASSWORD "password123"   // Password when in AP mode

// Over-the-Air (OTA) Update Configuration
#define OTA_HOSTNAME "wordclock"         // Hostname for OTA updates
#define OTA_PASSWORD "wordclock123"      // Password for OTA updates

// Optional: Default WiFi credentials (will be saved by WiFiManager if connection successful)
#define DEFAULT_WIFI_SSID ""             // Leave empty to force portal
#define DEFAULT_WIFI_PASSWORD ""         // Leave empty to force portal

#endif 