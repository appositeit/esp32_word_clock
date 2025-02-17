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
#include <WiFiManager.h>

// LED configuration
#define NUM_LEDS 64
#define DATA_PIN 10
CRGB leds[NUM_LEDS];

// Timezone
Timezone Australia;

// Add WiFiManager instance
WiFiManager wm;

// Add after LED configuration
#define LIGHT_SENSOR_PIN 2  // ADC pin for light sensor
#define LIGHT_SAMPLES 10    // Number of samples to average

// Add brightness settings structure
struct BrightnessSettings {
    int darkBrightness = 20;
    int lightBrightness = 255;
    int threshold = 2000;
} brightnessSettings;

// Add function declarations at the top with others
int readLightLevel();
void updateBrightness();
void bindServerCallback();

// At the top of the file with other globals
const char* BRIGHTNESS_PAGE_HTML = R"(
    <!DOCTYPE html>
    <html>
    <head>
        <meta name='viewport' content='width=device-width, initial-scale=1'>
        <title>Brightness Settings</title>
        <style>
            body { font-family: Arial; margin: 20px; background: #f0f0f0; }
            .container { 
                background: white;
                padding: 20px;
                border-radius: 4px;
                max-width: 600px;
                margin: 0 auto;
                box-shadow: 0 2px 4px rgba(0,0,0,0.1);
            }
            .setting {
                margin: 15px 0;
                display: flex;
                align-items: center;
                justify-content: space-between;
            }
            label { margin-right: 10px; }
            input {
                width: 100px;
                padding: 5px;
                border: 1px solid #ddd;
                border-radius: 4px;
            }
            .status {
                background: #f8f8f8;
                padding: 15px;
                border-radius: 4px;
                margin: 15px 0;
                text-align: center;
            }
            .buttons {
                margin-top: 20px;
                text-align: center;
            }
            button {
                padding: 10px 20px;
                margin: 0 5px;
                background: #1fa3ec;
                color: white;
                border: none;
                border-radius: 4px;
                cursor: pointer;
            }
            button.back { background: #666; }
            button:hover { opacity: 0.9; }
        </style>
    </head>
    <body>
        <div class='container'>
            <h2 style='text-align: center;'>Brightness Settings</h2>
            
            <div class='status'>
                <div>Current Time: <span id='time'>--:--</span></div>
                <div>Room Light Level: <span id='lightLevel'>--</span></div>
                <div>Current Brightness: <span id='brightness'>--</span></div>
            </div>

            <form id='brightnessForm'>
                <div class='setting'>
                    <label>Dark Mode Brightness:</label>
                    <input type='number' name='darkBrightness' min='0' max='255'>
                </div>
                
                <div class='setting'>
                    <label>Light Mode Brightness:</label>
                    <input type='number' name='lightBrightness' min='0' max='255'>
                </div>
                
                <div class='setting'>
                    <label>Light/Dark Threshold:</label>
                    <input type='number' name='threshold' min='0'>
                </div>
                
                <div class='buttons'>
                    <button type='submit'>Save Settings</button>
                    <button type='button' class='back' onclick='window.location.href="/"'>Back</button>
                </div>
            </form>
        </div>

        <script>
            // Update status every 5 seconds
            function updateStatus() {
                fetch('/api/status')
                    .then(r => r.json())
                    .then(data => {
                        document.getElementById('lightLevel').textContent = data.lightLevel;
                        document.getElementById('brightness').textContent = data.currentBrightness;
                        document.getElementById('time').textContent = new Date().toLocaleTimeString();
                        
                        // Update form values with current settings
                        document.querySelector('[name="darkBrightness"]').value = data.settings.darkBrightness;
                        document.querySelector('[name="lightBrightness"]').value = data.settings.lightBrightness;
                        document.querySelector('[name="threshold"]').value = data.settings.threshold;
                    })
                    .catch(console.error);
            }
            
            // Initial update and start interval
            updateStatus();
            setInterval(updateStatus, 5000);
            
            // Handle form submission
            document.getElementById('brightnessForm').onsubmit = function(e) {
                e.preventDefault();
                const formData = new FormData(e.target);
                fetch('/api/saveBrightness', {
                    method: 'POST',
                    body: formData
                }).then(() => {
                    alert('Settings saved');
                    updateStatus();  // Refresh display after save
                }).catch(err => {
                    alert('Error saving settings');
                    console.error(err);
                });
            };
        </script>
    </body>
    </html>
)";

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
        delay(25);  // Reduced from 100ms to 25ms per LED
    }
    fill_solid(leds, NUM_LEDS, CRGB::White);  // Flash all
    FastLED.show();
    delay(250);  // Quick flash
    fill_solid(leds, NUM_LEDS, CRGB::Black);
    FastLED.show();
}

// Add this function before connectToWiFi()
void bindServerCallback() {
    Serial.println("Binding server routes...");
    
    // Brightness configuration page
    wm.server->on("/brightness", HTTP_GET, []() {
        wm.server->send(200, "text/html", BRIGHTNESS_PAGE_HTML);
    });
    
    // Get all status information
    wm.server->on("/api/status", HTTP_GET, []() {
        String json = "{";
        json += "\"lightLevel\":" + String(readLightLevel()) + ",";
        json += "\"currentBrightness\":" + String(FastLED.getBrightness()) + ",";
        json += "\"settings\":{";
        json += "\"darkBrightness\":" + String(brightnessSettings.darkBrightness) + ",";
        json += "\"lightBrightness\":" + String(brightnessSettings.lightBrightness) + ",";
        json += "\"threshold\":" + String(brightnessSettings.threshold);
        json += "}}";
        wm.server->send(200, "application/json", json);
    });
    
    // Save brightness settings
    wm.server->on("/api/saveBrightness", HTTP_POST, []() {
        bool changed = false;
        
        if (wm.server->hasArg("darkBrightness")) {
            brightnessSettings.darkBrightness = wm.server->arg("darkBrightness").toInt();
            changed = true;
        }
        if (wm.server->hasArg("lightBrightness")) {
            brightnessSettings.lightBrightness = wm.server->arg("lightBrightness").toInt();
            changed = true;
        }
        if (wm.server->hasArg("threshold")) {
            brightnessSettings.threshold = wm.server->arg("threshold").toInt();
            changed = true;
        }
        
        if (changed) {
            updateBrightness();
        }
        
        wm.server->send(200, "text/plain", "OK");
    });
    
    Serial.println("Routes bound successfully");
}

// Then modify connectToWiFi()
void connectToWiFi() {
    Serial.println("Starting WiFiManager...");
    
    // Add custom HTML with time display and brightness button
    const char* customHTML = R"(
        <div id='time-display' style='
            background: white;
            padding: 10px;
            border-radius: 4px;
            margin: 10px 0;
            text-align: center;
            font-size: 1.2em;
        '>
            Current Time: <span id='current-time'>--:--</span>
        </div>
        <div style='text-align: center; margin: 10px 0;'>
            <button onclick='window.location.href="/brightness"' style='
                padding: 10px 20px;
                font-size: 1.1em;
                background: #1fa3ec;
                color: white;
                border: none;
                border-radius: 4px;
                cursor: pointer;
            '>Configure Brightness</button>
        </div>
        <script>
            function updateTime() {
                const now = new Date();
                const timeStr = now.toLocaleTimeString();
                document.getElementById('current-time').textContent = timeStr;
            }
            updateTime();
            setInterval(updateTime, 1000);
        </script>
    )";
    
    // Configure WiFiManager
    wm.setDebugOutput(true);
    wm.setCaptivePortalEnable(true);
    wm.setConfigPortalTimeout(180);
    
    // Set up the callback for adding our routes
    wm.setWebServerCallback(bindServerCallback);
    
    // Add custom HTML to the portal
    wm.setCustomHeadElement(customHTML);
    
    // Try to connect using saved credentials
    bool connected = wm.autoConnect("WordClock-AP", "password123");
    
    if (!connected) {
        Serial.println("Failed to connect and hit timeout");
        delay(3000);
        ESP.restart();
    }
    
    // Start the config portal in non-blocking mode
    wm.startWebPortal();
    Serial.println("Web portal started");
    
    Serial.println("\nWiFi connected");
    Serial.println("IP address: ");
    Serial.println(WiFi.localIP());
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

// Add these functions after testLEDs()
int readLightLevel() {
    int total = 0;
    for(int i = 0; i < LIGHT_SAMPLES; i++) {
        total += analogRead(LIGHT_SENSOR_PIN);
        delay(10);
    }
    return total / LIGHT_SAMPLES;
}

void updateBrightness() {
    int lightLevel = readLightLevel();
    int newBrightness;
    
    if (lightLevel < brightnessSettings.threshold) {
        newBrightness = brightnessSettings.darkBrightness;
    } else {
        newBrightness = brightnessSettings.lightBrightness;
    }
    
    FastLED.setBrightness(newBrightness);
    FastLED.show();
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
    if (WiFi.status() == WL_CONNECTED) {
        wm.process();  // Add this to keep WiFiManager running
    }
    
    events();
    displayTime(getTime());
    delay(1000);
} 