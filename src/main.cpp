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
#include "config.h"
#include <ArduinoOTA.h>
#include "favicon.h"

// LED configuration
CRGB leds[NUM_LEDS];

// Timezone
Timezone Australia;

// Add WiFiManager instance
WiFiManager wm;

// Add brightness settings structure
struct BrightnessSettings {
    int darkBrightness = 5;     // Changed from 20
    int lightBrightness = 25;   // Changed from 255
    int threshold = 2600;       // Changed from 2000
} brightnessSettings;

// Add function declarations at the top with others
int readLightLevel();
void updateBrightness();
void bindServerCallback();

// Add OTA setup function
void setupOTA() {
    ArduinoOTA.setHostname(OTA_HOSTNAME);
    ArduinoOTA.setPassword(OTA_PASSWORD);
    
    ArduinoOTA.onStart([]() {
        Serial.println("OTA: Start");
        FastLED.clear(true);  // Clear LEDs during update
    });
    
    ArduinoOTA.onEnd([]() {
        Serial.println("\nOTA: End");
    });
    
    ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
        Serial.printf("OTA Progress: %u%%\r", (progress / (total / 100)));
    });
    
    ArduinoOTA.onError([](ota_error_t error) {
        Serial.printf("Error[%u]: ", error);
        if (error == OTA_AUTH_ERROR) Serial.println("Auth Failed");
        else if (error == OTA_BEGIN_ERROR) Serial.println("Begin Failed");
        else if (error == OTA_CONNECT_ERROR) Serial.println("Connect Failed");
        else if (error == OTA_RECEIVE_ERROR) Serial.println("Receive Failed");
        else if (error == OTA_END_ERROR) Serial.println("End Failed");
    });
    
    ArduinoOTA.begin();
    Serial.println("OTA ready");
}

// Add OTA function declaration at top
void setupOTA();

// Add this function declaration at the top
void showProgress(int step);

// Add near the top with other globals
const char* COMMON_TIMEZONES[] = {
    "Africa/Cairo",
    "America/Chicago",
    "America/Los_Angeles",
    "America/New_York",
    "America/Toronto",
    "Asia/Dubai",
    "Asia/Hong_Kong",
    "Asia/Singapore",
    "Asia/Tokyo",
    "Australia/Adelaide",
    "Australia/Brisbane",
    "Australia/Melbourne",
    "Australia/Perth",
    "Australia/Sydney",
    "Europe/Amsterdam",
    "Europe/Berlin",
    "Europe/London",
    "Europe/Paris",
    "Pacific/Auckland"
};

// Add validation function
bool isValidTimezone(const String& tz) {
    // Check common timezones first
    for (const char* validTz : COMMON_TIMEZONES) {
        if (tz == validTz) return true;
    }
    
    // Basic format validation
    if (tz.indexOf('/') == -1) return false;  // Must contain region/city format
    if (tz.length() < 7) return false;        // Minimum length (e.g., "US/East")
    if (tz.indexOf(' ') != -1) return false;  // No spaces allowed
    
    return true;
}

// At the top of the file with other globals
const char* BRIGHTNESS_PAGE_HTML = R"(
    <!DOCTYPE html>
    <html>
    <head>
        <meta name='viewport' content='width=device-width, initial-scale=1'>
        <title>Brightness Settings</title>
        <link rel="icon" type="image/x-icon" href="/favicon.ico?v=1">
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
            }
            .setting label { 
                flex: 0 0 150px;
                margin-right: 10px; 
            }
            .setting input { 
                width: 100px;
                margin-right: 10px;
            }
            .setting .current {
                color: #666;
                font-size: 0.9em;
                margin-left: 10px;
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
            .help {
                font-size: 0.8em;
                color: #666;
                margin-left: 10px;
            }
        </style>
    </head>
    <body>
        <div class='container'>
            <h2 style='text-align: center;'>Brightness Settings</h2>
            
            <div class='status'>
                <div>Current Time: <span id='time'>--:--</span></div>
                <div>
                    Room Light Level: <span id='lightLevel'>--</span>
                    <label style="margin-left: 15px;">
                        <input type="checkbox" id="fastReadout"> Fast updates
                    </label>
                </div>
                <div>Current Brightness: <span id='brightness'>--</span></div>
            </div>

            <form id='brightnessForm'>
                <div class='setting'>
                    <label>Dark Mode Brightness:</label>
                    <input type='number' name='darkBrightness' min='0' max='255' required>
                    <span class='current'>(Current: <span id='currentDark'>--</span>)</span>
                    <div class='help'>Range: 0-255. Recommended: 1-10 for dark rooms.</div>
                </div>
                
                <div class='setting'>
                    <label>Light Mode Brightness:</label>
                    <input type='number' name='lightBrightness' min='0' max='255' required>
                    <span class='current'>(Current: <span id='currentLight'>--</span>)</span>
                    <div class='help'>Range: 0-255. Recommended: 20-50 for bright rooms.</div>
                </div>
                
                <div class='setting'>
                    <label>Light/Dark Threshold:</label>
                    <input type='number' name='threshold' min='0' max='4095' required>
                    <span class='current'>(Current: <span id='currentThreshold'>--</span>)</span>
                    <div class='help'>Range: 0-4095. Higher values mean the room needs to be brighter to trigger light mode.</div>
                </div>
                
                <div class='setting'>
                    <label>Timezone:</label>
                    <input type='text' name='timezone' list='timezones' required>
                    <datalist id='timezones'>
                        <option value="Africa/Cairo">Africa/Cairo</option>
                        <option value="America/Chicago">US Central</option>
                        <option value="America/Los_Angeles">US Pacific</option>
                        <option value="America/New_York">US Eastern</option>
                        <option value="America/Toronto">Eastern Canada</option>
                        <option value="Asia/Dubai">Dubai</option>
                        <option value="Asia/Hong_Kong">Hong Kong</option>
                        <option value="Asia/Singapore">Singapore</option>
                        <option value="Asia/Tokyo">Japan</option>
                        <option value="Australia/Adelaide">Adelaide</option>
                        <option value="Australia/Brisbane">Brisbane</option>
                        <option value="Australia/Melbourne">Melbourne</option>
                        <option value="Australia/Perth">Perth</option>
                        <option value="Australia/Sydney">Sydney</option>
                        <option value="Europe/Amsterdam">Netherlands</option>
                        <option value="Europe/Berlin">Germany</option>
                        <option value="Europe/London">UK</option>
                        <option value="Europe/Paris">France</option>
                        <option value="Pacific/Auckland">New Zealand</option>
                    </datalist>
                    <span class='current'>(Current: <span id='currentTimezone'>--</span>)</span>
                    <div class='help'>
                        Enter timezone from the <a href="https://en.wikipedia.org/wiki/List_of_tz_database_time_zones" target="_blank">tz database</a> 
                        or select from common options. Format: Region/City (e.g., "America/New_York")
                    </div>
                </div>
                
                <div class='buttons'>
                    <button type='submit'>Save Settings</button>
                    <button type='button' class='back' onclick='window.location.href="/"'>Back</button>
                </div>
            </form>
        </div>

        <script>
            let updateInterval = 5000;
            let updateTimer = null;
            let inputsInitialized = false;  // Track if inputs have been initialized

            // Update status
            function updateStatus() {
                fetch('/api/status')
                    .then(r => r.json())
                    .then(data => {
                        document.getElementById('lightLevel').textContent = data.lightLevel;
                        document.getElementById('brightness').textContent = data.currentBrightness;
                        document.getElementById('time').textContent = new Date().toLocaleTimeString();
                        
                        // Update current values display
                        document.getElementById('currentDark').textContent = data.settings.darkBrightness;
                        document.getElementById('currentLight').textContent = data.settings.lightBrightness;
                        document.getElementById('currentThreshold').textContent = data.settings.threshold;
                        document.getElementById('currentTimezone').textContent = data.timezone;
                        
                        // Set input values only on first load
                        if (!inputsInitialized) {
                            document.querySelector('[name="darkBrightness"]').value = data.settings.darkBrightness;
                            document.querySelector('[name="lightBrightness"]').value = data.settings.lightBrightness;
                            document.querySelector('[name="threshold"]').value = data.settings.threshold;
                            document.querySelector('[name="timezone"]').value = data.timezone;
                            inputsInitialized = true;
                        }
                    })
                    .catch(console.error);
            }
            
            // Handle fast readout toggle
            document.getElementById('fastReadout').onchange = function(e) {
                clearInterval(updateTimer);
                updateInterval = e.target.checked ? 1000 : 5000;
                updateTimer = setInterval(updateStatus, updateInterval);
            };
            
            // Initial update and start interval
            updateStatus();
            updateTimer = setInterval(updateStatus, updateInterval);
            
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
    // Add favicon route
    wm.server->on("/favicon.ico", HTTP_GET, []() {
        wm.server->send_P(200, "image/x-icon", (const char*)esp32wordclockBW_32x32_bmp, esp32wordclockBW_32x32_bmp_len);
    });
    
    // Brightness configuration page
    wm.server->on("/brightness", HTTP_GET, []() {
        wm.server->send(200, "text/html", BRIGHTNESS_PAGE_HTML);
    });
    
    // Get all status information
    wm.server->on("/api/status", HTTP_GET, []() {
        if (DEBUG_LEVEL > 1) Serial.println("GET /api/status");
        String json = "{";
        json += "\"lightLevel\":" + String(readLightLevel()) + ",";
        json += "\"currentBrightness\":" + String(FastLED.getBrightness()) + ",";
        json += "\"timezone\":\"" + String(DEFAULT_TIMEZONE) + "\",";  // Use the IANA identifier instead
        json += "\"settings\":{";
        json += "\"darkBrightness\":" + String(brightnessSettings.darkBrightness) + ",";
        json += "\"lightBrightness\":" + String(brightnessSettings.lightBrightness) + ",";
        json += "\"threshold\":" + String(brightnessSettings.threshold);
        json += "}}";
        wm.server->send(200, "application/json", json);
    });
    
    // Save brightness settings
    wm.server->on("/api/saveBrightness", HTTP_POST, []() {
        if (DEBUG_LEVEL > 0) {
            Serial.println("POST /api/saveBrightness");
            if (wm.server->args() > 0) {
                Serial.println("Args:");
                for (int i = 0; i < wm.server->args(); i++) {
                    Serial.printf("  %s: %s\n", 
                        wm.server->argName(i).c_str(), 
                        wm.server->arg(i).c_str());
                }
            }
        }
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
        if (wm.server->hasArg("timezone")) {
            String newTimezone = wm.server->arg("timezone");
            if (isValidTimezone(newTimezone)) {
                if (Australia.setLocation(newTimezone)) {
                    changed = true;
                    waitForSync(10);
                } else {
                    wm.server->send(400, "text/plain", "Invalid timezone");
                    return;
                }
            } else {
                wm.server->send(400, "text/plain", "Invalid timezone format");
                return;
            }
        }
        
        if (changed) {
            updateBrightness();
        }
        
        wm.server->send(200, "text/plain", "OK");
    });
}

// Then modify connectToWiFi()
void connectToWiFi() {
    Serial.println("Starting WiFiManager...");
    
    // Set portal title and theme
    wm.setTitle("WordClock");
    wm.setClass("invert");
    
    // Add custom HTML to the portal at the bottom
    const char* customHTML = R"(
        <br/>
        <form action='/brightness' method='get'>
            <button>Configure Brightness</button>
        </form>
        <div id='time-display' style='
            text-align: center;
            padding: 10px;
            color: #444;
            margin-top: 20px;
        '>
            Current Time: <span id='current-time'>--:--</span>
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
    wm.setCaptivePortalEnable(true);
    wm.setConfigPortalTimeout(180);
    wm.setShowInfoUpdate(false);  // Hide the default info/update buttons
    
    // Set up the callback for adding our routes
    wm.setWebServerCallback(bindServerCallback);
    
    // Add custom HTML to the portal
    wm.setCustomHeadElement(customHTML);
    
    // Try to connect using saved credentials or defaults
    bool connected = wm.autoConnect(WIFI_AP_NAME, WIFI_AP_PASSWORD);
    
    // If default credentials are set, try them
    if (!connected && strlen(DEFAULT_WIFI_SSID) > 0) {
        Serial.println("Trying default credentials...");
        WiFi.begin(DEFAULT_WIFI_SSID, DEFAULT_WIFI_PASSWORD);
        delay(5000);  // Give it time to connect
        connected = (WiFi.status() == WL_CONNECTED);
    }
    
    if (!connected) {
        Serial.println("Failed to connect");
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

// Add a variable to track last brightness update
unsigned long lastBrightnessCheck = 0;
#define BRIGHTNESS_CHECK_INTERVAL 1000  // Check every second

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
    showProgress(0);  // Show "IT IS ONE"
    
    // Test LEDs
    testLEDs();
    showProgress(1);  // Show "IT IS TWO"
    
    // Connect to WiFi
    connectToWiFi();
    showProgress(2);  // Show "IT IS THREE"
    
    if (WiFi.status() == WL_CONNECTED) {
        setupOTA();
        showProgress(3);  // Show "IT IS FOUR"
        
        // Try to sync time with retries
        int syncAttempts = 0;
        int progressCount = 4;  // Start at FIVE
        
        while (syncAttempts < 3) {
            Serial.printf("NTP sync attempt %d of 3...\n", syncAttempts + 1);
            showProgress(progressCount++);  // Show next number and increment
            waitForSync(10);
            
            if (timeStatus() != timeNotSet) {
                Australia.setLocation("Australia/Sydney");
                Serial.println("Current Sydney time: " + Australia.dateTime());
                simulatedTime = Australia.now();
                break;
            }
            
            Serial.println("Time sync failed, retrying...");
            delay(1000);
            syncAttempts++;
        }
        
        showProgress(5);  // Show final number (SIX) before starting
        delay(1000);  // Show final progress state briefly
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
        ArduinoOTA.handle();  // Handle OTA updates
        wm.process();         // Keep WiFiManager running
    }
    
    // Check if it's time to update brightness
    unsigned long currentMillis = millis();
    if (currentMillis - lastBrightnessCheck >= BRIGHTNESS_CHECK_INTERVAL) {
        updateBrightness();
        lastBrightnessCheck = currentMillis;
    }
    
    events();
    displayTime(getTime());
    delay(1000);
}

void showBootAnimation() {
    // Numbers 1-12 in sequence
    const int NUMBERS[] = {9, 11, 10, 12, 13, 14, 15, 16, 17, 18, 19, 20};  // Word indices for 1-12
    
    for (int i = 0; i < 12; i++) {
        fill_solid(leds, NUM_LEDS, CRGB::Black);  // Clear display
        
        // Show "IT IS"
        for (int j = 0; j < WORD_LENGTHS[0]; j++) {
            leds[WORDS[0][j]] = CRGB::White;
        }
        
        // Show current number
        for (int j = 0; j < WORD_LENGTHS[NUMBERS[i]]; j++) {
            leds[WORDS[NUMBERS[i]][j]] = CRGB::White;
        }
        
        FastLED.show();
        delay(500);  // Show each number for half a second
    }
    fill_solid(leds, NUM_LEDS, CRGB::Black);
    FastLED.show();
}

// Add the function implementation
void showProgress(int step) {
    // Numbers in order of display for setup progress
    const int PROGRESS_NUMBERS[] = {9, 11, 10, 12, 13, 14};  // Word indices for ONE through SIX
    
    if (step >= 0 && step < 6) {  // We have 6 progress steps
        fill_solid(leds, NUM_LEDS, CRGB::Black);
        
        // Show progress number only
        for (int j = 0; j < WORD_LENGTHS[PROGRESS_NUMBERS[step]]; j++) {
            leds[WORDS[PROGRESS_NUMBERS[step]][j]] = CRGB::White;
        }
        
        FastLED.show();
    }
} 