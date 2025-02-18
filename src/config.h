/**
 * Word Clock Configuration
 * 
 * Personal configuration file - not tracked in git
 */

#ifndef CONFIG_H
#define CONFIG_H

// LED Matrix Configuration
#define NUM_LEDS 64
#define DATA_PIN 10

// Light Sensor Configuration
#define LIGHT_SENSOR_PIN 2  // ADC pin for light sensor
#define LIGHT_SAMPLES 10    // Number of samples to average

// WiFi Configuration
#define WIFI_AP_NAME "WordClock-AP"      // Name when in AP mode
#define WIFI_AP_PASSWORD "password123"    // Password when in AP mode

// Over-the-Air (OTA) Update Configuration
#define OTA_HOSTNAME "wordclock"          // Hostname for OTA updates
#define OTA_PASSWORD "wordclock123"       // Password for OTA updates

// Optional: Default WiFi credentials
#define DEFAULT_WIFI_SSID "U908"          // Your WiFi network
#define DEFAULT_WIFI_PASSWORD "k1ndleb00ks"  // Your WiFi password

// Debug Configuration
#define DEBUG_LEVEL 0  // 0=minimal, 1=normal, 2=verbose

// Timezone Configuration
#define DEFAULT_TIMEZONE "Australia/Sydney"  // See: https://en.wikipedia.org/wiki/List_of_tz_database_time_zones

#endif // CONFIG_H
