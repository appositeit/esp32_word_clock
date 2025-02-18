# ESP32 Word Clock

A word clock implementation using an ESP32-C3 and an 8x8 WS2812B LED matrix. The clock displays time in a natural language format, such as "IT IS HALF PAST TWO" or "IT IS TWENTY FIVE TO THREE".

## Features

- Natural language time display
- NTP time synchronization
- Automatic timezone/DST handling for Sydney, Australia
- Automatic brightness adjustment based on ambient light
- Web-based configuration interface
- OTA (Over-The-Air) update support
- WiFi configuration via captive portal

## Hardware

### Components

- ESP32-C3 Super Mini
- 8x8 WS2812B LED Matrix (64 LEDs)
- Light Dependent Resistor (LDR)
- 10kΩ resistor (pull-down for LDR)

### Wiring Details

1. LED Matrix:
   - Data pin → GPIO10 (configurable in config.h)
   - VCC → 5V
   - GND → GND
   - Note: The data pin must be capable of digital output. GPIO10 is recommended as it's connected to the dedicated LED control peripheral.

2. Light Sensor:
   - LDR → 3.3V and GPIO2 (configurable in config.h)
   - 10kΩ resistor → GPIO2 and GND
   - Note: The LDR pin must be connected to an ADC (Analog-to-Digital Converter) capable pin. GPIO2 is recommended as it's connected to ADC1_CH2.

### Pin Configuration

Pins can be changed in `config.h`:
```cpp
// LED Matrix Configuration
#define DATA_PIN 10        // GPIO pin for LED data
#define NUM_LEDS 64       // Total number of LEDs (8x8 matrix)

// Light Sensor Configuration
#define LIGHT_SENSOR_PIN 2 // ADC pin for light sensor
#define LIGHT_SAMPLES 10   // Number of samples to average
```

## Configuration

### Settings in config.h

```cpp
// WiFi Access Point Configuration
#define WIFI_AP_NAME "WordClock-AP"      // Name when in AP mode
#define WIFI_AP_PASSWORD "password123"    // Password when in AP mode

// Over-the-Air (OTA) Update Configuration
#define OTA_HOSTNAME "wordclock"          // mDNS hostname for OTA
#define OTA_PASSWORD "wordclock123"       // Password for OTA updates

// Optional: Default WiFi credentials
#define DEFAULT_WIFI_SSID ""              // Your WiFi network
#define DEFAULT_WIFI_PASSWORD ""          // Your WiFi password

// Debug Configuration
#define DEBUG_LEVEL 0                     // 0=minimal, 1=normal, 2=verbose
```

### Brightness Settings

The clock automatically adjusts brightness based on ambient light levels:

- Dark Mode Brightness (1-10 recommended)
  - LED brightness when room is dark
  - Lower values prevent glare in dark rooms
  
- Light Mode Brightness (20-50 recommended)
  - LED brightness when room is well lit
  - Higher values improve visibility in bright conditions
  
- Light/Dark Threshold (0-4095)
  - ADC reading that triggers mode switch
  - Higher values require more light to trigger Light Mode
  - Default: 2600
  - Calibrate based on your LDR and room conditions

### Light Sensor Operation

The light sensor uses voltage divider principles:
- LDR resistance decreases in bright light
- ADC reads voltage at midpoint of divider
- Reading range: 0 (dark) to 4095 (bright)
- Readings are averaged over LIGHT_SAMPLES measurements
- Sample delay of 10ms between readings

## Software Setup

1. Clone the repository
2. Copy `src/config_template.h` to `src/config.h` and update settings
3. Build and flash using PlatformIO

### First Run

1. Power on the device
2. Connect to the "WordClock-AP" WiFi network
3. Configure your WiFi settings via the captive portal
4. The clock will sync time via NTP and begin operation

## Configuration

### Web Interface

Access the configuration interface at `http://<device-ip>/`:
- Configure WiFi settings
- Adjust brightness levels
- Set light sensor threshold
- View current status

### Settings

- Dark Mode Brightness: 0-255 (recommended: 1-10)
- Light Mode Brightness: 0-255 (recommended: 20-50)
- Light/Dark Threshold: 0-4095

## Development

Built using:
- PlatformIO
- Arduino framework
- FastLED library
- WiFiManager
- ezTime

## LED Matrix Layout

### Physical Layout

The 8x8 WS2812B LED matrix is arranged in a zig-zag pattern:
```
63 62 61 60 59 58 57 56  ←  Row 0
48 49 50 51 52 53 54 55  →  Row 1
47 46 45 44 43 42 41 40  ←  Row 2
32 33 34 35 36 37 38 39  →  Row 3
31 30 29 28 27 26 25 24  ←  Row 4
16 17 18 19 20 21 22 23  →  Row 5
15 14 13 12 11 10  9  8  ←  Row 6
 0  1  2  3  4  5  6  7  →  Row 7
```

### Word Positions

The matrix overlays these words:
```
IT IS | HALF | TEN      (Row 0)
QUARTER | TWENTY        (Row 1)
FIVE | MINUTES | TO     (Row 2)
PAST | ONE | THREE      (Row 3)
TWO | FOUR | FIVE       (Row 4)
SIX | SEVEN | EIGHT     (Row 5)
NINE | TEN | ELEVEN     (Row 6)
TWELVE | O'CLOCK        (Row 7)
```

### Word LED Mappings

Each word is mapped to specific LED indices:
```cpp
// Examples from the mapping array:
IT IS    -> LEDs 63, 62
HALF     -> LEDs 60, 59
QUARTER  -> LEDs 48, 49, 50, 51
TWENTY   -> LEDs 52, 53, 54, 55
FIVE     -> LEDs 47, 46
MINUTES  -> LEDs 45, 44, 43, 42
```

### Time Display Examples

1. 1:00 - Lights up: "IT IS ONE O'CLOCK"
   - IT IS (63,62)
   - ONE (35,36)
   - O'CLOCK (4,5,6,7)

2. 2:30 - Lights up: "IT IS HALF PAST TWO"
   - IT IS (63,62)
   - HALF (60,59)
   - PAST (32,33)
   - TWO (31,30)

3. 5:45 - Lights up: "IT IS QUARTER TO SIX"
   - IT IS (63,62)
   - QUARTER (48,49,50,51)
   - TO (40)
   - SIX (16,17)

### Time Rounding

The display rounds to the nearest 5 minutes:
- XX:00-XX:02 → O'CLOCK
- XX:03-XX:07 → FIVE PAST
- XX:08-XX:12 → TEN PAST
- etc.

## License

MIT License - See LICENSE file for details 