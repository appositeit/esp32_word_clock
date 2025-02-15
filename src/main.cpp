/**
 * Word Clock - ESP32-C3 Implementation
 * 
 * This program drives an 8x8 WS2812B LED matrix to create a word clock display.
 * The clock shows the time in a natural language format, such as:
 * "IT IS HALF PAST TWO" or "IT IS TWENTY FIVE TO THREE"
 * 
 * Hardware:
 * - ESP32-C3 Super Mini
 * - 8x8 WS2812B LED Matrix (64 LEDs total)
 * - LED data line connected to GPIO10
 * 
 * Features:
 * - NTP time synchronization
 * - Automatic timezone/DST handling for Sydney, Australia
 * - Fallback to simulated time if network unavailable
 * - LED test sequence on startup
 * - Rounds time to nearest 5 minutes
 * 
 * Time Display Format:
 * Minutes:
 * - XX:00 -> "O'CLOCK"
 * - XX:05 -> "FIVE PAST"
 * - XX:10 -> "TEN PAST"
 * - XX:15 -> "QUARTER PAST"
 * - XX:20 -> "TWENTY PAST"
 * - XX:25 -> "TWENTY FIVE PAST"
 * - XX:30 -> "HALF PAST"
 * - XX:35 -> "TWENTY FIVE TO" (next hour)
 * - XX:40 -> "TWENTY TO" (next hour)
 * - XX:45 -> "QUARTER TO" (next hour)
 * - XX:50 -> "TEN TO" (next hour)
 * - XX:55 -> "FIVE TO" (next hour)
 */

#include <Arduino.h>
#include <FastLED.h>
#include <WiFi.h>
#include <ezTime.h>

// WiFi credentials
const char* ssid = "U908";
const char* password = "k1ndleb00ks";

// LED configuration
#define NUM_LEDS 64
#define DATA_PIN 10
CRGB leds[NUM_LEDS];

// Timezone
Timezone Australia;

/**
 * LED Matrix Layout (8x8):
 * The matrix is arranged in a zig-zag pattern, with words overlaid on a mask.
 * Numbers represent LED indices (0-63).
 * 
 * 63 62 61 60 59 58 57 56   <- Row 0: IT IS | HALF | TEN
 * 48 49 50 51 52 53 54 55   <- Row 1: QUARTER | TWENTY
 * 47 46 45 44 43 42 41 40   <- Row 2: FIVE | MINUTES | TO
 * 32 33 34 35 36 37 38 39   <- Row 3: PAST | ONE | THREE
 * 31 30 29 28 27 26 25 24   <- Row 4: TWO | FOUR | FIVE
 * 16 17 18 19 20 21 22 23   <- Row 5: SIX | SEVEN | EIGHT
 * 15 14 13 12 11 10  9  8   <- Row 6: NINE | TEN | ELEVEN
 *  0  1  2  3  4  5  6  7   <- Row 7: TWELVE | O'CLOCK
 */

// Word positions in LED array - each sub-array contains LED indices for a word
const int WORDS[][8] = {
    {63, 62},          // IT IS
    {60, 59},          // HALF
    {57, 56},          // TEN
    {48, 49, 50, 51},  // QUARTER
    {52, 53, 54, 55},  // TWENTY
    {47, 46},          // FIVE
    {45, 44, 43, 42},  // MINUTES
    {40},              // TO
    {32, 33},          // PAST
    {35, 36},          // ONE
    {37, 38, 39},      // THREE
    {31, 30},          // TWO
    {28, 27},          // FOUR
    {25, 24},          // FIVE
    {16, 17},          // SIX
    {18, 19, 20},      // SEVEN
    {21, 22, 23},      // EIGHT
    {15, 14},          // NINE
    {13},              // TEN
    {10, 9, 8},        // ELEVEN
    {0, 1, 2},         // TWELVE
    {4, 5, 6, 7},      // O'CLOCK
};

// Number of LEDs used for each word
const int WORD_LENGTHS[] = {2, 2, 2, 4, 4, 2, 4, 1, 2, 2, 3, 2, 2, 2, 2, 3, 3, 2, 1, 3, 3, 4};

/**
 * Tests all LEDs in sequence to verify wiring and positioning
 * Lights each LED for 100ms, then turns it off
 */
void testLEDs() {
    Serial.println("Testing LEDs sequentially...");
    for (int i = 0; i < NUM_LEDS; i++) {
        fill_solid(leds, NUM_LEDS, CRGB::Black);  // Clear all
        leds[i] = CRGB::White;  // Light current LED
        FastLED.show();
        Serial.printf("Testing LED %d\n", i);
        delay(100);  // 100ms per LED = 6.4 seconds total
    }
    fill_solid(leds, NUM_LEDS, CRGB::Black);
    FastLED.show();
}

/**
 * Attempts to connect to WiFi with multiple retries
 * Will attempt connection maxAttempts times before giving up
 */
void connectToWiFi() {
    int attempts = 0;
    const int maxAttempts = 3;
    
    while (attempts < maxAttempts) {
        Serial.printf("WiFi attempt %d of %d...\n", attempts + 1, maxAttempts);
        WiFi.begin(ssid, password);
        
        int waitCount = 0;
        while (WiFi.status() != WL_CONNECTED && waitCount < 20) {  // 10 second timeout
            delay(500);
            Serial.print(".");
            waitCount++;
        }
        
        if (WiFi.status() == WL_CONNECTED) {
            Serial.println("\nWiFi connected");
            Serial.println("IP address: ");
            Serial.println(WiFi.localIP());
            return;
        }
        
        Serial.println("\nWiFi connection failed, retrying...");
        WiFi.disconnect();
        delay(1000);
        attempts++;
    }
    
    Serial.println("Could not connect to WiFi. Continuing without network...");
}

// Add a simulated time for testing
unsigned long lastUpdate = 0;
time_t simulatedTime = 0;

/**
 * Returns current time, either from NTP or simulation
 * If network is available, returns actual time from NTP
 * If network is unavailable, returns simulated time advancing 1 minute per second
 */
time_t getTime() {
    if (WiFi.status() != WL_CONNECTED) {
        // If no network, use simulated time
        unsigned long currentMillis = millis();
        if (currentMillis - lastUpdate >= 1000) {  // Every second
            simulatedTime += 60;  // Add one minute
            lastUpdate = currentMillis;
        }
        return simulatedTime;
    }
    return Australia.now();
}

/**
 * Displays the time on the LED matrix
 * @param localTime Current time (either real or simulated)
 * 
 * Process:
 * 1. Clears all LEDs
 * 2. Always displays "IT IS"
 * 3. Rounds minutes to nearest 5
 * 4. Determines minute phrase (PAST/TO)
 * 5. Determines hour (handling the "TO" case for next hour)
 * 6. Lights appropriate LEDs for all words
 */
void displayTime(time_t localTime) {
    static int lastHour = -1;
    static int lastMinute = -1;
    
    int hours = hour(localTime);
    int minutes = minute(localTime);
    
    // Round minutes to nearest 5
    int roundedMinutes = ((minutes + 2) / 5) * 5;
    if (roundedMinutes == 60) {
        roundedMinutes = 0;
        hours = (hours + 1) % 24;
    }
    
    // Only update display if time has changed
    if (hours != lastHour || roundedMinutes != lastMinute) {
        Serial.printf("Time updating: %02d:%02d (rounded from %02d:%02d)\n", 
                     hours, roundedMinutes, hour(localTime), minute(localTime));
        
        lastHour = hours;
        lastMinute = roundedMinutes;
        
        // Clear all LEDs
        fill_solid(leds, NUM_LEDS, CRGB::Black);
        
        // Always display "IT IS"
        for (int i = 0; i < WORD_LENGTHS[0]; i++) {
            leds[WORDS[0][i]] = CRGB::White;
        }
        
        // Convert 24-hour format to 12-hour
        hours = hours % 12;
        if (hours == 0) hours = 12;
        
        // Handle minute phrases
        if (roundedMinutes > 30) {
            hours = (hours % 12) + 1;
            if (hours == 13) hours = 1;
        }
        
        // Display minutes
        if (roundedMinutes > 0) {
            if (roundedMinutes <= 30) {
                // PAST
                for (int i = 0; i < WORD_LENGTHS[8]; i++) {
                    leds[WORDS[8][i]] = CRGB::White;
                }
            } else {
                // TO
                for (int i = 0; i < WORD_LENGTHS[7]; i++) {
                    leds[WORDS[7][i]] = CRGB::White;
                }
                roundedMinutes = 60 - roundedMinutes;
            }
            
            // Handle specific minute patterns
            if (roundedMinutes == 5) {
                // FIVE
                for (int i = 0; i < WORD_LENGTHS[5]; i++) {
                    leds[WORDS[5][i]] = CRGB::White;
                }
            } else if (roundedMinutes == 10) {
                // TEN
                for (int i = 0; i < WORD_LENGTHS[2]; i++) {
                    leds[WORDS[2][i]] = CRGB::White;
                }
            } else if (roundedMinutes == 15) {
                // QUARTER
                for (int i = 0; i < WORD_LENGTHS[3]; i++) {
                    leds[WORDS[3][i]] = CRGB::White;
                }
            } else if (roundedMinutes == 20) {
                // TWENTY
                for (int i = 0; i < WORD_LENGTHS[4]; i++) {
                    leds[WORDS[4][i]] = CRGB::White;
                }
            } else if (roundedMinutes == 25) {
                // TWENTY FIVE
                for (int i = 0; i < WORD_LENGTHS[4]; i++) {
                    leds[WORDS[4][i]] = CRGB::White;
                }
                for (int i = 0; i < WORD_LENGTHS[5]; i++) {
                    leds[WORDS[5][i]] = CRGB::White;
                }
            } else if (roundedMinutes == 30) {
                // HALF
                for (int i = 0; i < WORD_LENGTHS[1]; i++) {
                    leds[WORDS[1][i]] = CRGB::White;
                }
            }
        }
        
        // Display hour
        int hourWordIndex;
        switch (hours) {
            case 1: hourWordIndex = 9; break;
            case 2: hourWordIndex = 11; break;
            case 3: hourWordIndex = 10; break;
            case 4: hourWordIndex = 12; break;
            case 5: hourWordIndex = 13; break;
            case 6: hourWordIndex = 14; break;
            case 7: hourWordIndex = 15; break;
            case 8: hourWordIndex = 16; break;
            case 9: hourWordIndex = 17; break;
            case 10: hourWordIndex = 18; break;
            case 11: hourWordIndex = 19; break;
            case 12: hourWordIndex = 20; break;
        }
        
        // Display hour word
        for (int i = 0; i < WORD_LENGTHS[hourWordIndex]; i++) {
            leds[WORDS[hourWordIndex][i]] = CRGB::White;
        }
        
        // Only display O'CLOCK when it's exactly on the hour
        if (roundedMinutes == 0) {
            for (int i = 0; i < WORD_LENGTHS[21]; i++) {
                leds[WORDS[21][i]] = CRGB::White;
            }
        }
        
        FastLED.show();
    }
}

/**
 * Setup routine
 * 1. Initializes serial communication
 * 2. Configures LED matrix
 * 3. Runs LED test sequence
 * 4. Attempts WiFi connection
 * 5. If WiFi available, syncs time with NTP
 * 6. If WiFi unavailable, starts simulated time at 12:00
 */
void setup() {
    Serial.begin(115200);
    Serial.println("Word Clock Starting...");
    
    // Initialize FastLED
    FastLED.addLeds<WS2812B, DATA_PIN, GRB>(leds, NUM_LEDS);
    FastLED.setBrightness(50);
    
    // Test LEDs first
    testLEDs();
    
    // Connect to WiFi
    connectToWiFi();
    
    if (WiFi.status() == WL_CONNECTED) {
        // Try to sync time with retries
        int syncAttempts = 0;
        while (syncAttempts < 3) {
            Serial.printf("NTP sync attempt %d of 3...\n", syncAttempts + 1);
            waitForSync(10);  // 10 second timeout
            
            if (timeStatus() != timeNotSet) {
                Australia.setLocation("Australia/Sydney");
                Serial.println("Current Sydney time: " + Australia.dateTime());
                simulatedTime = Australia.now();  // Initialize simulated time
                break;
            }
            
            Serial.println("Time sync failed, retrying...");
            delay(1000);
            syncAttempts++;
        }
    } else {
        // Start simulated time at 12:00
        simulatedTime = 43200;  // 12:00:00
    }
}

/**
 * Main loop
 * 1. Handles ezTime events (if connected)
 * 2. Gets current time (real or simulated)
 * 3. Updates display
 * 4. Waits 1 second before next update
 */
void loop() {
    events();  // Handle ezTime events if connected
    
    displayTime(getTime());
    
    delay(1000);
} 