/*
 * IRL Hunts Tracker v5 - Interactive Edition
 * 
 * ENHANCED FEATURES:
 * - Button cycling through 6 information screens
 * - Expressive LED patterns for status, consent, role
 * - Nearby players detailed list with threat assessment
 * - Personal statistics and score tracking
 * - Recent events/notifications history (last 10)
 * - Chat message display (last 5 messages)
 * - Long-press actions (hold 1s for screen cycle, 2s+ for emergency)
 * - Role-specific LED colors and patterns
 * 
 * BUTTON CONTROLS:
 * - Short press (<500ms): Role-specific action OR cycle screens
 * - Medium press (500ms-1s): Cycle to next info screen
 * - Long hold (2s+): Emergency trigger sequence
 * 
 * SCREENS (cycle with medium press):
 * 1. Main Status (default)
 * 2. Nearby Players List
 * 3. Personal Stats
 * 4. Recent Events
 * 5. Chat Messages
 * 6. Game Info / Help
 * 
 * LED PATTERNS:
 * - Solid colors indicate consent level
 * - Blink rate indicates game status
 * - Pattern indicates role (pred=fast, prey=slow, safe=breathing)
 * 
 * LIBRARIES NEEDED:
 * 1. "ESP8266 and ESP32 OLED driver for SSD1306 displays" by ThingPulse
 * 2. "RadioLib" by Jan Gromes  
 * 3. "ArduinoJson" by Benoit Blanchon
 */

#include <SPI.h>
#include <RadioLib.h>
#include <Wire.h>
#include "SSD1306Wire.h"
#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <map>
#include <vector>

// Load configuration
#if __has_include("config_local.h")
  #include "config_local.h"
  #define CONFIG_SOURCE "config_local.h"
#elif __has_include("config.h")
  #include "config.h"
  #define CONFIG_SOURCE "config.h"
#else
  #error "No config file found! Copy config.h.example to config.h and edit it."
#endif

// Hardware pins
#ifndef PIN_LORA_NSS
  #define PIN_LORA_NSS 8
#endif

#define LORA_NSS    PIN_LORA_NSS
#define LORA_DIO1   14
#define LORA_RST    12
#define LORA_BUSY   13
#define LORA_MOSI   10
#define LORA_MISO   11
#define LORA_SCK    9
#define OLED_SDA    17
#define OLED_SCL    18
#define OLED_RST    21
#define OLED_ADDR   0x3C
#define VEXT_PIN    36
#define BUTTON_PIN  0
#define LED_PIN     35
#define LORA_FREQ   915.0

// WiFi and Server settings from config
#ifndef WIFI_SSID
  #error "WIFI_SSID not defined! Check your config file."
#endif
#ifndef WIFI_PASS
  #error "WIFI_PASS not defined! Check your config file."
#endif
#ifndef SERVER_URL
  #error "SERVER_URL not defined! Check your config file."
#endif

const char* wifi_ssid = WIFI_SSID;
const char* wifi_pass = WIFI_PASS;
const char* server_url = SERVER_URL;

// Timing constants
#ifndef PING_INTERVAL
  #define PING_INTERVAL 5000
#endif
#ifndef BEACON_INTERVAL
  #define BEACON_INTERVAL 3000
#endif
#ifndef DISPLAY_UPDATE_INTERVAL
  #define DISPLAY_UPDATE_INTERVAL 100
#endif
#ifndef SCROLL_SPEED
  #define SCROLL_SPEED 150
#endif
#ifndef WIFI_RECONNECT_INTERVAL
  #define WIFI_RECONNECT_INTERVAL 15000
#endif
#ifndef NOTIFICATION_DURATION
  #define NOTIFICATION_DURATION 3000
#endif
#ifndef SERVER_TIMEOUT_THRESHOLD
  #define SERVER_TIMEOUT_THRESHOLD 45000
#endif
#ifndef EMERGENCY_HOLD_TIME
  #define EMERGENCY_HOLD_TIME 2000
#endif
#ifndef EMERGENCY_TAP_COUNT
  #define EMERGENCY_TAP_COUNT 3
#endif
#ifndef EMERGENCY_TAP_WINDOW
  #define EMERGENCY_TAP_WINDOW 2000
#endif

const unsigned long ping_interval = PING_INTERVAL;
const unsigned long beacon_interval = BEACON_INTERVAL;
const unsigned long display_update_interval = DISPLAY_UPDATE_INTERVAL;
const unsigned long scroll_speed = SCROLL_SPEED;
const unsigned long wifi_reconnect_interval = WIFI_RECONNECT_INTERVAL;
const unsigned long notification_duration = NOTIFICATION_DURATION;
const unsigned long server_timeout_threshold = SERVER_TIMEOUT_THRESHOLD;
const unsigned long emergency_hold_time = EMERGENCY_HOLD_TIME;
const int emergency_tap_count = EMERGENCY_TAP_COUNT;
const unsigned long emergency_tap_window = EMERGENCY_TAP_WINDOW;

// Hardware objects
SPIClass loraSPI(HSPI);
SX1262 radio = new Module(LORA_NSS, LORA_DIO1, LORA_RST, LORA_BUSY, loraSPI);
SSD1306Wire display(OLED_ADDR, OLED_SDA, OLED_SCL);
HTTPClient http;

// =============================================================================
// SCREEN MANAGEMENT
// =============================================================================
enum ScreenType {
  SCREEN_MAIN = 0,
  SCREEN_NEARBY,
  SCREEN_STATS,
  SCREEN_EVENTS,
  SCREEN_CHAT,
  SCREEN_HELP,
  SCREEN_COUNT  // Total number of screens
};

ScreenType currentScreen = SCREEN_MAIN;
unsigned long lastScreenChange = 0;
unsigned long screenAutoReturn = 15000;  // Return to main after 15s of inactivity

// =============================================================================
// PLAYER DATA
// =============================================================================
String NODE_ID, myName = "", myRole = "unassigned", myStatus = "lobby";
String gamePhase = "lobby", gameMode = "classic", gameModeName = "Classic Hunt", myTeam = "";
bool inSafeZone = false, infectionMode = false, photoRequired = false;
bool wifiConnected = false, serverReachable = false;
bool myReady = false;
int myPoints = 0, myCaptures = 0, myEscapes = 0, myTimeCaptured = 0, mySightings = 0;

// Emergency state
bool emergencyActive = false;
String emergencyBy = "";
String emergencyDeviceId = "";
String emergencyReason = "";
String emergencyLocation = "";
String emergencyNearby = "";
String emergencyPhone = "";
unsigned long emergencyDisplayCycle = 0;

// Consent
bool consentPhysical = false;
bool consentPhoto = true;
bool consentLocation = true;
String consentBadge = "STD";

// Capture tracking
String capturedByName = "";
String capturedByDevice = "";
std::vector<String> myCapturedPrey;
std::map<String, String> preyNames;
std::map<String, bool> preyEscaped;

// =============================================================================
// NEARBY PLAYERS & RSSI TRACKING
// =============================================================================
struct NearbyPlayer {
  String id;
  String role;
  int rssi;
  unsigned long lastSeen;
  String consent;
};

std::map<String, NearbyPlayer> nearbyPlayers;
std::map<String, int> beaconRssi;
std::vector<String> knownBeacons;

// =============================================================================
// EVENT & MESSAGE HISTORY
// =============================================================================
struct GameEvent {
  String type;
  String message;
  String timeStr;
};

struct ChatMessage {
  String from;
  String content;
  String timeStr;
};

std::vector<GameEvent> recentEvents;
std::vector<ChatMessage> recentMessages;
const int MAX_EVENTS = 10;
const int MAX_MESSAGES = 5;

// =============================================================================
// NOTIFICATIONS
// =============================================================================
struct Notification {
  String message;
  String type;
  unsigned long expiry;
  int priority;
};

Notification currentNotif = {"", "info", 0, 0};

int getNotificationPriority(const String& type) {
  if (type == "captured") return 100;
  if (type == "danger") return 80;
  if (type == "escape") return 70;
  if (type == "warning") return 60;
  if (type == "success") return 50;
  if (type == "info") return 40;
  return 30;
}

void showNotification(const String& msg, const String& type = "info") {
  int newPriority = getNotificationPriority(type);
  
  if (currentNotif.expiry > millis() && newPriority < currentNotif.priority) {
    return;  // Don't override higher priority
  }
  
  currentNotif.message = msg;
  currentNotif.type = type;
  currentNotif.priority = newPriority;
  
  if (type == "captured") currentNotif.expiry = millis() + 10000;
  else if (type == "danger" || type == "escape") currentNotif.expiry = millis() + 5000;
  else currentNotif.expiry = millis() + NOTIFICATION_DURATION;
  
  Serial.println("[NOTIF] " + type + ": " + msg);
  
  // Add to events history
  addEvent(type, msg);
}

bool hasActiveNotification() {
  return currentNotif.message.length() > 0 && millis() < currentNotif.expiry;
}

void addEvent(const String& type, const String& message) {
  GameEvent evt;
  evt.type = type;
  evt.message = message;
  
  // Simple time string (minutes since start)
  unsigned long mins = millis() / 60000;
  evt.timeStr = String(mins) + "m ago";
  
  recentEvents.insert(recentEvents.begin(), evt);
  if (recentEvents.size() > MAX_EVENTS) {
    recentEvents.pop_back();
  }
}

void addChatMessage(const String& from, const String& content) {
  ChatMessage msg;
  msg.from = from;
  msg.content = content;
  
  unsigned long mins = millis() / 60000;
  msg.timeStr = String(mins) + "m";
  
  recentMessages.insert(recentMessages.begin(), msg);
  if (recentMessages.size() > MAX_MESSAGES) {
    recentMessages.pop_back();
  }
}

// =============================================================================
// LED CONTROL - EXPRESSIVE PATTERNS
// =============================================================================
unsigned long lastLedUpdate = 0;
int ledState = 0;
int ledBrightness = 0;
bool ledIncreasing = true;

void updateLED() {
  unsigned long now = millis();
  int interval = 1000;
  
  // Emergency: Rapid strobe
  if (emergencyActive) {
    interval = 100;
    if (now - lastLedUpdate > interval) {
      lastLedUpdate = now;
      ledState = !ledState;
      digitalWrite(LED_PIN, ledState ? HIGH : LOW);
    }
    return;
  }
  
  // Captured: Fast blink
  if (myStatus == "captured") {
    interval = 200;
    if (now - lastLedUpdate > interval) {
      lastLedUpdate = now;
      ledState = !ledState;
      digitalWrite(LED_PIN, ledState ? HIGH : LOW);
    }
    return;
  }
  
  // Safe zone: Breathing effect (slow pulse)
  if (inSafeZone) {
    if (now - lastLedUpdate > 30) {  // Update every 30ms for smooth breathing
      lastLedUpdate = now;
      if (ledIncreasing) {
        ledBrightness += 5;
        if (ledBrightness >= 255) {
          ledBrightness = 255;
          ledIncreasing = false;
        }
      } else {
        ledBrightness -= 5;
        if (ledBrightness <= 0) {
          ledBrightness = 0;
          ledIncreasing = true;
        }
      }
      // ESP32 doesn't have analogWrite, use digital approximation
      digitalWrite(LED_PIN, ledBrightness > 127 ? HIGH : LOW);
    }
    return;
  }
  
  // Role-based patterns
  if (myRole == "pred") {
    // Predator: Double blink pattern
    interval = 300;
    if (now - lastLedUpdate > interval) {
      lastLedUpdate = now;
      ledState++;
      if (ledState > 3) ledState = 0;
      digitalWrite(LED_PIN, (ledState == 0 || ledState == 2) ? HIGH : LOW);
    }
  } else if (myRole == "prey") {
    // Prey: Slow single blink
    interval = 1500;
    if (now - lastLedUpdate > interval) {
      lastLedUpdate = now;
      ledState = !ledState;
      digitalWrite(LED_PIN, ledState ? HIGH : LOW);
    }
  } else {
    // Unassigned: Very slow
    interval = 3000;
    if (now - lastLedUpdate > interval) {
      lastLedUpdate = now;
      ledState = !ledState;
      digitalWrite(LED_PIN, ledState ? HIGH : LOW);
    }
  }
  
  // Connection issues: Override with fast blink
  if (!wifiConnected) {
    interval = 150;
    if (now - lastLedUpdate > interval) {
      lastLedUpdate = now;
      ledState = !ledState;
      digitalWrite(LED_PIN, ledState ? HIGH : LOW);
    }
  } else if (!serverReachable) {
    interval = 400;
    if (now - lastLedUpdate > interval) {
      lastLedUpdate = now;
      ledState = !ledState;
      digitalWrite(LED_PIN, ledState ? HIGH : LOW);
    }
  }
}

// =============================================================================
// BUTTON HANDLING - ENHANCED
// =============================================================================
bool lastBtnState = HIGH;
unsigned long btnPressTime = 0;
bool btnProcessed = false;
bool emergencyHoldComplete = false;
unsigned long emergencyTapStartTime = 0;
int emergencyTapCounter = 0;
unsigned long lastBtnRelease = 0;

#define SHORT_PRESS_MAX 500
#define MEDIUM_PRESS_MAX 1500

void handleButton() {
  bool btnState = digitalRead(BUTTON_PIN);
  unsigned long now = millis();
  
  // Button just pressed
  if (btnState == LOW && lastBtnState == HIGH) {
    btnPressTime = now;
    btnProcessed = false;
    
    // Check for emergency tap sequence
    if (emergencyHoldComplete) {
      emergencyTapCounter++;
      if (emergencyTapCounter >= EMERGENCY_TAP_COUNT) {
        triggerEmergency();
      } else {
        displayEmergencyConfirm();
      }
    }
  }
  
  // Button held down - check for long press actions
  if (btnState == LOW && !btnProcessed) {
    unsigned long holdTime = now - btnPressTime;
    
    // Emergency trigger sequence (2s hold)
    if (holdTime >= EMERGENCY_HOLD_TIME && !emergencyHoldComplete) {
      emergencyHoldComplete = true;
      emergencyTapCounter = 0;
      emergencyTapStartTime = now;
      showNotification("Release & tap " + String(EMERGENCY_TAP_COUNT) + "x for emergency", "warning");
      btnProcessed = true;
    }
    // Screen cycle hint (1s hold)
    else if (holdTime >= 1000 && holdTime < EMERGENCY_HOLD_TIME && !emergencyHoldComplete) {
      // Show hint that releasing now will cycle screen
      // (Don't process yet, wait for release)
    }
  }
  
  // Button released
  if (btnState == HIGH && lastBtnState == LOW) {
    unsigned long holdTime = now - btnPressTime;
    lastBtnRelease = now;
    
    if (!btnProcessed && !emergencyHoldComplete) {
      if (holdTime < SHORT_PRESS_MAX) {
        // SHORT PRESS: Role-specific action or cycle subscreens
        handleShortPress();
      } else if (holdTime < MEDIUM_PRESS_MAX) {
        // MEDIUM PRESS: Cycle to next main screen
        cycleScreen();
      }
    }
    btnProcessed = true;
  }
  
  // Emergency tap timeout
  if (emergencyHoldComplete && (now - emergencyTapStartTime > EMERGENCY_TAP_WINDOW)) {
    if (emergencyTapCounter < EMERGENCY_TAP_COUNT) {
      emergencyHoldComplete = false;
      emergencyTapCounter = 0;
      showNotification("Emergency cancelled", "info");
    }
  }
  
  lastBtnState = btnState;
}

void handleShortPress() {
  // Context-sensitive action based on current screen
  switch (currentScreen) {
    case SCREEN_MAIN:
      if (myRole == "pred") {
        attemptCapture();
      } else if (myRole == "prey") {
        if (myStatus == "captured") {
          showNotification("FIND SAFE ZONE BEACON TO ESCAPE!", "warning");
        } else {
          // Quick threat check
          displayThreatLevel();
        }
      } else {
        showNotification("Set role at " + String(server_url), "info");
      }
      break;
      
    case SCREEN_NEARBY:
      // Cycle through nearby players detail
      cycleNearbyDetail();
      break;
      
    case SCREEN_STATS:
      // Refresh stats
      showNotification("Stats refreshed", "info");
      break;
      
    case SCREEN_EVENTS:
      // Clear old events
      if (recentEvents.size() > 3) {
        recentEvents.erase(recentEvents.begin() + 3, recentEvents.end());
        showNotification("Old events cleared", "info");
      }
      break;
      
    case SCREEN_CHAT:
      // Show chat hint
      showNotification("Chat: use phone dashboard", "info");
      break;
      
    case SCREEN_HELP:
      // Return to main
      currentScreen = SCREEN_MAIN;
      lastScreenChange = millis();
      break;
  }
}

void cycleScreen() {
  currentScreen = (ScreenType)((currentScreen + 1) % SCREEN_COUNT);
  lastScreenChange = millis();
  
  String screenNames[] = {"MAIN", "NEARBY", "STATS", "EVENTS", "CHAT", "HELP"};
  showNotification("Screen: " + screenNames[currentScreen], "info");
}

int nearbyDetailIndex = 0;

void cycleNearbyDetail() {
  if (nearbyPlayers.size() == 0) {
    showNotification("No players nearby", "info");
    return;
  }
  
  nearbyDetailIndex = (nearbyDetailIndex + 1) % nearbyPlayers.size();
}

void displayThreatLevel() {
  if (nearbyPlayers.size() == 0) {
    showNotification("✓ No threats detected", "success");
    return;
  }
  
  int threatCount = 0;
  int closestRssi = -999;
  
  for (auto& p : nearbyPlayers) {
    if (p.second.role == "pred") {
      threatCount++;
      if (p.second.rssi > closestRssi) {
        closestRssi = p.second.rssi;
      }
    }
  }
  
  if (threatCount == 0) {
    showNotification("✓ No predators nearby", "success");
  } else if (closestRssi > -50) {
    showNotification("🚨 CRITICAL! " + String(threatCount) + " pred @ " + String(closestRssi) + "dB", "danger");
  } else if (closestRssi > -65) {
    showNotification("⚠️ DANGER! " + String(threatCount) + " pred nearby", "danger");
  } else {
    showNotification("📡 " + String(threatCount) + " pred detected", "warning");
  }
}

// =============================================================================
// DISPLAY FUNCTIONS
// =============================================================================
int scrollOffset = 0;
unsigned long lastScrollUpdate = 0;

void updateScroll() {
  if (millis() - lastScrollUpdate > SCROLL_SPEED) {
    lastScrollUpdate = millis();
    scrollOffset = (scrollOffset + 1) % 1000;
  }
}

void drawScrollingText(int y, const String& text, int maxWidth = 128) {
  int textWidth = display.getStringWidth(text);
  if (textWidth <= maxWidth) {
    display.drawString(0, y, text);
  } else {
    String displayText = text + "   " + text;
    int totalWidth = textWidth + display.getStringWidth("   ");
    int offset = scrollOffset % totalWidth;
    for (int i = 0; i < displayText.length(); i++) {
      int charX = display.getStringWidth(displayText.substring(0, i)) - offset;
      if (charX >= -10 && charX < maxWidth) {
        display.drawString(charX, y, String(displayText[i]));
      }
    }
  }
}

void drawStatusBar() {
  String statusLine = "";
  
  // Name or ID
  if (myName.length() > 0 && myName.indexOf("Player_") != 0) {
    statusLine = myName.substring(0, 8);
  } else {
    statusLine = NODE_ID;
  }
  
  // Connection status
  statusLine += wifiConnected ? " [W" : " [W!";
  statusLine += serverReachable ? "S]" : "S?]";
  
  // Consent badge
  if (consentBadge.length() > 0 && consentBadge != "STD") {
    statusLine += " [" + consentBadge + "]";
  }
  
  // Ready status
  if (myReady) statusLine += " R";
  
  display.drawString(0, 0, statusLine);
  
  // Screen indicator on right
  String screenIndicator = String(currentScreen + 1) + "/" + String(SCREEN_COUNT);
  int indicatorWidth = display.getStringWidth(screenIndicator);
  display.drawString(128 - indicatorWidth, 0, screenIndicator);
}

void drawBottomHint() {
  display.setTextAlignment(TEXT_ALIGN_CENTER);
  display.drawString(64, 54, "BTN=Action | Hold=NextScreen");
  display.setTextAlignment(TEXT_ALIGN_LEFT);
}

void displayMain() {
  display.clear();
  display.setTextAlignment(TEXT_ALIGN_LEFT);
  display.setFont(ArialMT_Plain_10);
  
  drawStatusBar();
  
  // Role and mode
  String line2 = "";
  if (myRole == "pred") {
    if (infectionMode) line2 = "PREDATOR [Infect]";
    else if (photoRequired) line2 = "PREDATOR [Photo first]";
    else line2 = "PREDATOR [Hunt]";
  } else if (myRole == "prey") {
    if (myStatus == "captured") line2 = "!! CAPTURED !!";
    else line2 = "PREY [Survive]";
  } else {
    line2 = "UNASSIGNED";
  }
  display.drawString(0, 12, line2);
  
  // Game status
  String line3 = gameModeName;
  if (gamePhase == "running") line3 += " LIVE";
  else if (gamePhase == "paused") line3 += " PAUSED";
  else line3 += " " + gamePhase;
  drawScrollingText(24, line3);
  
  // Status line
  String line4 = "";
  if (myStatus == "captured" && myRole == "prey") {
    line4 = "By: " + (capturedByName.length() > 0 ? capturedByName : "???");
  } else if (inSafeZone) {
    line4 = "🏠 IN SAFE ZONE";
  } else {
    line4 = "Pts: " + String(myPoints) + " | " + myStatus;
  }
  display.drawString(0, 36, line4);
  
  // Nearby players summary
  String line5 = "";
  if (nearbyPlayers.size() == 0) {
    line5 = "No players detected";
  } else {
    int predCount = 0, preyCount = 0;
    int bestRssi = -999;
    for (auto& p : nearbyPlayers) {
      if (p.second.role == "pred") predCount++;
      else if (p.second.role == "prey") preyCount++;
      if (p.second.rssi > bestRssi) bestRssi = p.second.rssi;
    }
    
    if (myRole == "pred") {
      line5 = "🎯 " + String(preyCount) + " prey | Best: " + String(bestRssi) + "dB";
    } else {
      line5 = "⚠️ " + String(predCount) + " pred | Closest: " + String(bestRssi) + "dB";
    }
  }
  drawScrollingText(48, line5);
  
  display.display();
}

void displayNearbyPlayers() {
  display.clear();
  display.setTextAlignment(TEXT_ALIGN_LEFT);
  display.setFont(ArialMT_Plain_10);
  
  display.drawString(0, 0, "NEARBY PLAYERS (" + String(nearbyPlayers.size()) + ")");
  
  if (nearbyPlayers.size() == 0) {
    display.setTextAlignment(TEXT_ALIGN_CENTER);
    display.drawString(64, 25, "No players detected");
    display.drawString(64, 40, "Area appears clear");
    display.setTextAlignment(TEXT_ALIGN_LEFT);
  } else {
    // Convert to sorted vector
    std::vector<NearbyPlayer> sorted;
    for (auto& p : nearbyPlayers) {
      sorted.push_back(p.second);
    }
    // Sort by RSSI (closest first)
    for (int i = 0; i < sorted.size() - 1; i++) {
      for (int j = i + 1; j < sorted.size(); j++) {
        if (sorted[j].rssi > sorted[i].rssi) {
          NearbyPlayer temp = sorted[i];
          sorted[i] = sorted[j];
          sorted[j] = temp;
        }
      }
    }
    
    // Display up to 4 players
    int y = 12;
    for (int i = 0; i < min((int)sorted.size(), 4); i++) {
      String line = sorted[i].id + " " + sorted[i].role.substring(0, 4);
      line += " " + String(sorted[i].rssi) + "dB";
      if (sorted[i].consent.length() > 0 && sorted[i].consent != "STD") {
        line += " [" + sorted[i].consent + "]";
      }
      display.drawString(0, y, line);
      y += 11;
    }
    
    if (sorted.size() > 4) {
      display.drawString(0, 54, "+" + String(sorted.size() - 4) + " more...");
    }
  }
  
  display.display();
}

void displayStats() {
  display.clear();
  display.setTextAlignment(TEXT_ALIGN_LEFT);
  display.setFont(ArialMT_Plain_10);
  
  display.drawString(0, 0, "MY STATISTICS");
  
  display.drawString(0, 14, "Points: " + String(myPoints));
  
  if (myRole == "pred") {
    display.drawString(0, 26, "Captures: " + String(myCaptures));
    display.drawString(0, 38, "Sightings: " + String(mySightings));
    
    int activeCaptures = 0;
    for (auto& prey : myCapturedPrey) {
      if (!preyEscaped[prey]) activeCaptures++;
    }
    display.drawString(0, 50, "Holding: " + String(activeCaptures) + " prey");
  } else if (myRole == "prey") {
    display.drawString(0, 26, "Escapes: " + String(myEscapes));
    display.drawString(0, 38, "Caught: " + String(myTimeCaptured) + "x");
    display.drawString(0, 50, "Sightings: " + String(mySightings));
  } else {
    display.drawString(0, 26, "Select role to play!");
    display.drawString(0, 38, "Open: " + String(server_url));
  }
  
  display.display();
}

void displayEvents() {
  display.clear();
  display.setTextAlignment(TEXT_ALIGN_LEFT);
  display.setFont(ArialMT_Plain_10);
  
  display.drawString(0, 0, "RECENT EVENTS (" + String(recentEvents.size()) + ")");
  
  if (recentEvents.size() == 0) {
    display.setTextAlignment(TEXT_ALIGN_CENTER);
    display.drawString(64, 30, "No events yet");
    display.setTextAlignment(TEXT_ALIGN_LEFT);
  } else {
    int y = 12;
    for (int i = 0; i < min((int)recentEvents.size(), 4); i++) {
      String line = recentEvents[i].type.substring(0, 8) + ": ";
      line += recentEvents[i].message.substring(0, 14);
      display.drawString(0, y, line);
      y += 11;
    }
  }
  
  display.display();
}

void displayChat() {
  display.clear();
  display.setTextAlignment(TEXT_ALIGN_LEFT);
  display.setFont(ArialMT_Plain_10);
  
  display.drawString(0, 0, "CHAT MESSAGES");
  
  if (recentMessages.size() == 0) {
    display.setTextAlignment(TEXT_ALIGN_CENTER);
    display.drawString(64, 25, "No messages yet");
    display.drawString(64, 40, "Use phone to chat");
    display.setTextAlignment(TEXT_ALIGN_LEFT);
  } else {
    int y = 12;
    for (int i = 0; i < min((int)recentMessages.size(), 3); i++) {
      String from = recentMessages[i].from.substring(0, 10);
      String msg = recentMessages[i].content.substring(0, 18);
      display.drawString(0, y, from + ":");
      y += 10;
      display.drawString(0, y, " " + msg);
      y += 12;
    }
  }
  
  display.display();
}

void displayHelp() {
  display.clear();
  display.setTextAlignment(TEXT_ALIGN_LEFT);
  display.setFont(ArialMT_Plain_10);
  
  display.drawString(0, 0, "CONTROLS & HELP");
  display.drawString(0, 12, "Short press: Action");
  display.drawString(0, 22, "Hold 1s: Next screen");
  display.drawString(0, 32, "Hold 2s+3tap: EMERGENCY");
  display.drawString(0, 44, "ID: " + NODE_ID);
  display.drawString(0, 54, "URL: " + String(server_url).substring(0, 20));
  
  display.display();
}

void displayEmergency() {
  display.clear();
  display.setTextAlignment(TEXT_ALIGN_CENTER);
  display.setFont(ArialMT_Plain_16);
  display.drawString(64, 0, "!!! EMERGENCY !!!");
  display.setFont(ArialMT_Plain_10);
  
  int cycle = ((millis() - emergencyDisplayCycle) / 3000) % 4;
  
  switch (cycle) {
    case 0:
      display.drawString(64, 18, "FIND PLAYER:");
      display.setFont(ArialMT_Plain_16);
      display.drawString(64, 32, emergencyBy);
      display.setFont(ArialMT_Plain_10);
      display.drawString(64, 52, "Device: " + emergencyDeviceId);
      break;
    case 1:
      display.drawString(64, 18, "PHONE:");
      display.setFont(ArialMT_Plain_16);
      display.drawString(64, 32, emergencyPhone.length() > 0 ? emergencyPhone : "Not provided");
      display.setFont(ArialMT_Plain_10);
      display.drawString(64, 52, "CALL TO LOCATE");
      break;
    case 2:
      display.drawString(64, 18, "LOCATION:");
      display.drawString(64, 32, emergencyLocation.length() > 0 ? emergencyLocation : "Unknown");
      display.drawString(64, 48, "HELP THEM NOW!");
      break;
    case 3:
      display.drawString(64, 18, "NEARBY:");
      display.drawString(64, 32, emergencyNearby.length() > 0 ? emergencyNearby : "None detected");
      break;
  }
  
  display.display();
}

void displayNotification() {
  display.clear();
  display.setTextAlignment(TEXT_ALIGN_CENTER);
  display.setFont(ArialMT_Plain_16);
  
  String header = "INFO";
  if (currentNotif.type == "danger" || currentNotif.type == "captured") header = "! ALERT !";
  else if (currentNotif.type == "warning") header = "WARNING";
  else if (currentNotif.type == "success") header = "SUCCESS";
  else if (currentNotif.type == "escape") header = "ESCAPED!";
  
  display.drawString(64, 5, header);
  display.setFont(ArialMT_Plain_10);
  
  String msg = currentNotif.message;
  if (msg.length() > 21) {
    int split = msg.lastIndexOf(' ', 21);
    if (split == -1) split = 21;
    display.drawString(64, 28, msg.substring(0, split));
    if (msg.length() > split + 1) {
      display.drawString(64, 41, msg.substring(split + 1));
    }
  } else {
    display.drawString(64, 32, msg);
  }
  
  display.display();
}

void displayEmergencyConfirm() {
  display.clear();
  display.setTextAlignment(TEXT_ALIGN_CENTER);
  display.setFont(ArialMT_Plain_16);
  display.drawString(64, 10, "EMERGENCY?");
  display.setFont(ArialMT_Plain_10);
  display.drawString(64, 32, "Tap " + String(EMERGENCY_TAP_COUNT - emergencyTapCounter) + " more times");
  display.drawString(64, 46, "to confirm");
  display.display();
}

void displayServerTimeout() {
  display.clear();
  display.setTextAlignment(TEXT_ALIGN_CENTER);
  display.setFont(ArialMT_Plain_16);
  display.drawString(64, 10, "! SERVER !");
  display.drawString(64, 30, "OFFLINE");
  display.setFont(ArialMT_Plain_10);
  unsigned long timeoutSecs = (millis() - lastSuccessfulPing) / 1000;
  display.drawString(64, 50, "No response " + String(timeoutSecs) + "s");
  display.display();
}

void displayScreen() {
  // Priority overrides
  if (emergencyActive) {
    displayEmergency();
    return;
  }
  
  if (!serverReachable && (millis() - lastSuccessfulPing) > SERVER_TIMEOUT_THRESHOLD) {
    displayServerTimeout();
    return;
  }
  
  if (hasActiveNotification()) {
    displayNotification();
    return;
  }
  
  // Auto-return to main screen after inactivity
  if (currentScreen != SCREEN_MAIN && (millis() - lastScreenChange > screenAutoReturn)) {
    currentScreen = SCREEN_MAIN;
  }
  
  // Display current screen
  switch (currentScreen) {
    case SCREEN_MAIN:
      displayMain();
      break;
    case SCREEN_NEARBY:
      displayNearbyPlayers();
      break;
    case SCREEN_STATS:
      displayStats();
      break;
    case SCREEN_EVENTS:
      displayEvents();
      break;
    case SCREEN_CHAT:
      displayChat();
      break;
    case SCREEN_HELP:
      displayHelp();
      break;
    default:
      displayMain();
      break;
  }
}

// =============================================================================
// NETWORKING
// =============================================================================
unsigned long lastWifiCheck = 0, lastSuccessfulPing = 0;
int consecutivePingFails = 0;

void checkWiFiConnection() {
  bool wasConnected = wifiConnected;
  wifiConnected = (WiFi.status() == WL_CONNECTED);
  
  if (!wifiConnected && wasConnected) {
    showNotification("WiFi lost! Reconnecting...", "danger");
  } else if (wifiConnected && !wasConnected) {
    showNotification("WiFi reconnected!", "success");
    consecutivePingFails = 0;
  }
  
  if (!wifiConnected && (millis() - lastWifiCheck > WIFI_RECONNECT_INTERVAL)) {
    lastWifiCheck = millis();
    WiFi.disconnect();
    delay(100);
    WiFi.begin(wifi_ssid, wifi_pass);
    
    int attempts = 0;
    while (WiFi.status() != WL_CONNECTED && attempts < 10) {
      delay(500);
      attempts++;
    }
  }
}

// =============================================================================
// LORA COMMUNICATION
// =============================================================================
volatile bool rxFlag = false;
void setRxFlag() { rxFlag = true; }

int txCount = 0, rxCount = 0;

void sendBeacon() {
  String broadcastRole = (myRole == "unassigned") ? "unknown" : myRole;
  String packet = NODE_ID + "|" + broadcastRole + "|" + consentBadge;
  
  int state = radio.transmit(packet);
  if (state == RADIOLIB_ERR_NONE) {
    txCount++;
    if (txCount % 10 == 1) {
      Serial.println("TX #" + String(txCount) + ": " + packet);
    }
  }
  
  radio.startReceive();
}

void receivePackets() {
  if (!rxFlag) return;
  rxFlag = false;
  
  String packet;
  int state = radio.readData(packet);
  
  if (state == RADIOLIB_ERR_NONE && packet.length() > 0) {
    float rssi = radio.getRSSI();
    int sep = packet.indexOf('|');
    
    if (sep > 0) {
      String id = packet.substring(0, sep);
      String typeAndConsent = packet.substring(sep + 1);
      
      String type = typeAndConsent;
      String consent = "STD";
      int sep2 = typeAndConsent.indexOf('|');
      if (sep2 > 0) {
        type = typeAndConsent.substring(0, sep2);
        consent = typeAndConsent.substring(sep2 + 1);
      }
      
      if (id != NODE_ID) {
        if (type == "SAFEZONE" || type == "BEACON") {
          beaconRssi[id] = (int)rssi;
        } else if (type == "pred" || type == "prey" || type == "unknown") {
          bool isNew = (nearbyPlayers.count(id) == 0);
          
          NearbyPlayer np;
          np.id = id;
          np.role = type;
          np.rssi = (int)rssi;
          np.lastSeen = millis();
          np.consent = consent;
          
          nearbyPlayers[id] = np;
          rxCount++;
          
          Serial.println("RX from " + id + " (" + type + ") RSSI: " + String((int)rssi) + "dB");
          
          // New player alerts
          if (isNew && !emergencyActive && gamePhase == "running") {
            if (myRole == "prey" && type == "pred") {
              showNotification("⚠️ NEW PREDATOR: " + id, "danger");
            } else if (myRole == "pred" && type == "prey") {
              showNotification("🎯 NEW PREY: " + id, "success");
            }
          }
          
          // Proximity warnings for prey
          if (!emergencyActive && myRole == "prey" && type == "pred" && !inSafeZone && myStatus != "captured") {
            if (rssi > -50) {
              showNotification("🚨 CRITICAL! Pred RIGHT HERE!", "danger");
            } else if (rssi > -60) {
              showNotification("🚨 DANGER! Pred very close!", "danger");
            } else if (rssi > -70) {
              showNotification("⚠️ Pred nearby (" + String((int)rssi) + "dB)", "warning");
            }
          }
        }
      }
    }
  }
  
  radio.startReceive();
}

void cleanupStaleNearby() {
  static unsigned long lastCleanup = 0;
  if (millis() - lastCleanup < 10000) return;
  lastCleanup = millis();
  
  std::vector<String> toRemove;
  for (auto& p : nearbyPlayers) {
    if (millis() - p.second.lastSeen > 30000) {
      toRemove.push_back(p.first);
    }
  }
  
  for (auto& id : toRemove) {
    nearbyPlayers.erase(id);
    Serial.println("Removed stale player: " + id);
  }
}

// =============================================================================
// SERVER COMMUNICATION
// =============================================================================
unsigned long lastPing = 0;

void pingServer() {
  checkWiFiConnection();
  if (!wifiConnected) {
    serverReachable = false;
    return;
  }
  
  StaticJsonDocument<1024> doc;
  doc["device_id"] = NODE_ID;
  
  JsonObject pRssi = doc.createNestedObject("player_rssi");
  for (auto& p : nearbyPlayers) {
    pRssi[p.first] = p.second.rssi;
  }
  
  JsonObject bRssi = doc.createNestedObject("beacon_rssi");
  for (auto& b : beaconRssi) {
    bRssi[b.first] = b.second;
  }
  
  String json;
  serializeJson(doc, json);
  
  http.begin(String(server_url) + "/api/tracker/ping");
  http.addHeader("Content-Type", "application/json");
  http.setTimeout(5000);
  int code = http.POST(json);
  
  if (code == 200) {
    serverReachable = true;
    consecutivePingFails = 0;
    lastSuccessfulPing = millis();
    
    String resp = http.getString();
    StaticJsonDocument<2048> respDoc;
    
    if (!deserializeJson(respDoc, resp)) {
      // Update player state
      String newPhase = respDoc["phase"].as<String>();
      String newStatus = respDoc["status"].as<String>();
      String newRole = respDoc["role"].as<String>();
      String newName = respDoc["name"].as<String>();
      bool newSafe = respDoc["in_safe_zone"];
      String newMode = respDoc["game_mode"].as<String>();
      String newModeName = respDoc["game_mode_name"].as<String>();
      bool newEmergency = respDoc["emergency"];
      String newEmergencyBy = respDoc["emergency_by"].as<String>();
      bool newInfection = respDoc["infection_mode"];
      bool newPhotoReq = respDoc["photo_required"];
      bool newReady = respDoc["ready"] | false;
      
      String newTeam = "";
      if (!respDoc["team"].isNull()) {
        newTeam = respDoc["team"].as<String>();
      }
      
      String newConsentBadge = "STD";
      if (!respDoc["consent_badge"].isNull()) {
        newConsentBadge = respDoc["consent_badge"].as<String>();
      }
      
      // Update local state
      if (newName.length() > 0) myName = newName;
      if (newConsentBadge.length() > 0) consentBadge = newConsentBadge;
      myReady = newReady;
      
      if (gameMode != newMode && newMode.length() > 0) {
        gameMode = newMode;
        gameModeName = newModeName;
        showNotification("Mode: " + newModeName, "info");
      }
      
      infectionMode = newInfection;
      photoRequired = newPhotoReq;
      
      if (newTeam.length() > 0 && myTeam != newTeam) {
        myTeam = newTeam;
        showNotification("Team: " + myTeam, "info");
      }
      
      // Emergency handling
      if (newEmergency && !emergencyActive) {
        emergencyActive = true;
        emergencyBy = newEmergencyBy;
        emergencyDisplayCycle = millis();
        
        if (respDoc.containsKey("emergency_info")) {
          JsonObject eInfo = respDoc["emergency_info"];
          if (!eInfo["device_id"].isNull()) emergencyDeviceId = eInfo["device_id"].as<String>();
          if (!eInfo["reason"].isNull()) emergencyReason = eInfo["reason"].as<String>();
          if (!eInfo["location"].isNull()) emergencyLocation = eInfo["location"].as<String>();
          if (!eInfo["phone"].isNull()) emergencyPhone = eInfo["phone"].as<String>();
          
          if (eInfo.containsKey("nearby")) {
            JsonArray nearby = eInfo["nearby"];
            emergencyNearby = "";
            for (int i = 0; i < nearby.size() && i < 3; i++) {
              if (i > 0) emergencyNearby += ", ";
              emergencyNearby += nearby[i].as<String>();
            }
          }
        }
        
        showNotification("EMERGENCY! Help " + emergencyBy + "!", "danger");
      } else if (!newEmergency && emergencyActive) {
        emergencyActive = false;
        emergencyBy = "";
        emergencyDeviceId = "";
        showNotification("Emergency cleared", "success");
      }
      
      // Game phase changes
      if (gamePhase != newPhase) {
        gamePhase = newPhase;
        if (newPhase == "running") showNotification("Game started!", "success");
        else if (newPhase == "paused") showNotification("Game paused", "warning");
        else if (newPhase == "ended") showNotification("Game ended!", "info");
      }
      
      // Status changes
      if (myStatus != newStatus) {
        String oldStatus = myStatus;
        myStatus = newStatus;
        
        if (newStatus == "captured") {
          if (!respDoc["captured_by_name"].isNull()) {
            capturedByName = respDoc["captured_by_name"].as<String>();
          }
          if (!respDoc["captured_by_device"].isNull()) {
            capturedByDevice = respDoc["captured_by_device"].as<String>();
          }
          myTimeCaptured++;
          showNotification("CAPTURED by " + (capturedByName.length() > 0 ? capturedByName : "predator") + "!", "captured");
        } else if (oldStatus == "captured" && newStatus == "active") {
          myEscapes++;
          capturedByName = "";
          capturedByDevice = "";
          showNotification("YOU ESCAPED! You're free!", "escape");
        }
      }
      
      // Role changes
      if (myRole != newRole && newRole != "unassigned" && newRole.length() > 0) {
        String oldRole = myRole;
        myRole = newRole;
        if (oldRole == "prey" && newRole == "pred") {
          showNotification("INFECTED! Now PREDATOR!", "danger");
        } else {
          showNotification("Role: " + myRole, "success");
        }
      }
      
      // Safe zone changes
      if (inSafeZone != newSafe) {
        inSafeZone = newSafe;
        showNotification(newSafe ? "Entered SAFE ZONE" : "Left safe zone!", newSafe ? "success" : "warning");
      }
      
      // Update stats
      if (respDoc.containsKey("points")) myPoints = respDoc["points"];
      if (respDoc.containsKey("captures")) myCaptures = respDoc["captures"];
      if (respDoc.containsKey("escapes")) myEscapes = respDoc["escapes"];
      if (respDoc.containsKey("sightings")) mySightings = respDoc["sightings"];
      
      // Process notifications
      if (respDoc.containsKey("notifications")) {
        JsonArray notifs = respDoc["notifications"];
        for (JsonVariant n : notifs) {
          String msg = n["message"].as<String>();
          String type = n["type"].as<String>();
          showNotification(msg, type.length() > 0 ? type : "info");
        }
      }
      
      // Process chat messages
      if (respDoc.containsKey("messages")) {
        JsonArray msgs = respDoc["messages"];
        for (JsonVariant m : msgs) {
          String from = m["from"].as<String>();
          String content = m["content"].as<String>();
          addChatMessage(from, content);
        }
      }
      
      // Update capture list for predators
      if (myRole == "pred" && respDoc.containsKey("my_captures")) {
        JsonArray captures = respDoc["my_captures"];
        for (JsonVariant cap : captures) {
          String preyId = cap["device_id"].as<String>();
          String preyName = cap["name"].as<String>();
          bool escaped = cap["escaped"].as<bool>();
          
          preyNames[preyId] = preyName;
          
          bool found = false;
          for (auto& existing : myCapturedPrey) {
            if (existing == preyId) {
              found = true;
              if (escaped && !preyEscaped[preyId]) {
                preyEscaped[preyId] = true;
                showNotification("⚠️ " + preyName + " ESCAPED!", "warning");
              }
              break;
            }
          }
          
          if (!found) {
            myCapturedPrey.push_back(preyId);
            preyEscaped[preyId] = escaped;
            if (!escaped) {
              myCaptures++;
              showNotification("🎯 Captured " + preyName + "!", "success");
            }
          }
        }
      }
      
      // Update beacons list
      if (respDoc.containsKey("beacons")) {
        knownBeacons.clear();
        JsonArray beacons = respDoc["beacons"];
        for (JsonVariant b : beacons) {
          knownBeacons.push_back(b.as<String>());
        }
      }
    }
  } else {
    consecutivePingFails++;
    if (consecutivePingFails >= 3 && serverReachable) {
      serverReachable = false;
      showNotification("Server connection lost", "warning");
    }
  }
  
  http.end();
  beaconRssi.clear();
}

// =============================================================================
// GAME ACTIONS
// =============================================================================
int captureAttempts = 0;

void attemptCapture() {
  if (myRole != "pred") {
    showNotification("Only predators can capture", "warning");
    return;
  }
  if (gamePhase != "running") {
    showNotification("Game not running", "warning");
    return;
  }
  if (emergencyActive) {
    showNotification("EMERGENCY active - no captures", "danger");
    return;
  }
  if (inSafeZone) {
    showNotification("Can't capture from safe zone", "warning");
    return;
  }
  
  // Find best target
  int bestRssi = -999;
  String targetId = "";
  
  for (auto& p : nearbyPlayers) {
    if (p.second.role == "prey" && p.second.rssi > bestRssi) {
      bestRssi = p.second.rssi;
      targetId = p.first;
    }
  }
  
  if (targetId.length() == 0) {
    showNotification("No prey detected nearby", "info");
    return;
  }
  
  captureAttempts++;
  showNotification("Attempting capture...", "info");
  
  StaticJsonDocument<256> doc;
  doc["pred_id"] = NODE_ID;
  doc["prey_id"] = targetId;
  doc["rssi"] = bestRssi;
  
  String json;
  serializeJson(doc, json);
  
  http.begin(String(server_url) + "/api/tracker/capture");
  http.addHeader("Content-Type", "application/json");
  http.setTimeout(5000);
  int code = http.POST(json);
  
  if (code == 200) {
    String resp = http.getString();
    StaticJsonDocument<256> respDoc;
    deserializeJson(respDoc, resp);
    bool success = respDoc["success"];
    String msg = respDoc["message"].as<String>();
    
    if (success) {
      showNotification(infectionMode ? "INFECTED!" : "CAPTURE SUCCESS!", "success");
    } else {
      showNotification("Failed: " + msg, "warning");
    }
  } else {
    showNotification("Server error", "danger");
  }
  
  http.end();
}

void triggerEmergency() {
  showNotification("Sending emergency alert...", "danger");
  
  StaticJsonDocument<256> doc;
  doc["device_id"] = NODE_ID;
  doc["reason"] = "Emergency triggered from tracker";
  
  String json;
  serializeJson(doc, json);
  
  http.begin(String(server_url) + "/api/tracker/emergency");
  http.addHeader("Content-Type", "application/json");
  http.setTimeout(5000);
  int code = http.POST(json);
  
  if (code == 200) {
    showNotification("EMERGENCY SENT! Help coming!", "success");
  } else {
    showNotification("Failed to send emergency", "danger");
  }
  
  http.end();
  emergencyHoldComplete = false;
  emergencyTapCounter = 0;
}

// =============================================================================
// UTILITIES
// =============================================================================
float getBatteryVoltage() {
  analogSetPinAttenuation(1, ADC_11db);
  int adcValue = analogRead(1);
  float adcVoltage = (adcValue / 4095.0) * 3.3;
  float batteryVoltage = adcVoltage * 4.9;
  
  if (batteryVoltage < 2.5 || batteryVoltage > 4.5) {
    return 0.0;
  }
  return batteryVoltage;
}

void checkBattery() {
  float voltage = getBatteryVoltage();
  static bool lowWarned = false;
  
  if (voltage < 3.3 && !lowWarned && voltage > 0) {
    showNotification("LOW BATTERY! Charge soon!", "danger");
    lowWarned = true;
  } else if (voltage > 3.5) {
    lowWarned = false;
  }
}

void displayStartup(const String& msg) {
  display.clear();
  display.setTextAlignment(TEXT_ALIGN_CENTER);
  display.setFont(ArialMT_Plain_10);
  display.drawString(64, 25, msg);
  display.display();
}

void displayBigText(const String& line1, const String& line2 = "") {
  display.clear();
  display.setTextAlignment(TEXT_ALIGN_CENTER);
  display.setFont(ArialMT_Plain_16);
  display.drawString(64, 15, line1);
  if (line2.length() > 0) {
    display.setFont(ArialMT_Plain_10);
    display.drawString(64, 38, line2);
  }
  display.display();
}

// =============================================================================
// SETUP
// =============================================================================
void setup() {
  Serial.begin(115200);
  delay(500);
  
  Serial.println("\n========================================");
  Serial.println("IRL Hunts Tracker v5.0 - Interactive");
  Serial.println("Build: " __DATE__ " " __TIME__);
  Serial.println("Config: " CONFIG_SOURCE);
  Serial.println("========================================");
  
  // Initialize pins
  pinMode(BUTTON_PIN, INPUT_PULLUP);
  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, HIGH);
  pinMode(VEXT_PIN, OUTPUT);
  digitalWrite(VEXT_PIN, LOW);
  delay(100);
  
  // OLED reset sequence
  pinMode(OLED_RST, OUTPUT);
  digitalWrite(OLED_RST, LOW);
  delay(50);
  digitalWrite(OLED_RST, HIGH);
  delay(100);
  
  // Initialize display
  Wire.begin(OLED_SDA, OLED_SCL);
  display.init();
  display.flipScreenVertically();
  display.setFont(ArialMT_Plain_10);
  displayStartup("Initializing...");
  
  // Generate unique ID
  uint64_t mac = ESP.getEfuseMac();
  uint16_t uniqueId = (uint16_t)(mac & 0xFFFF) ^ (uint16_t)((mac >> 32) & 0xFFFF) ^ (uint16_t)(mac >> 16);
  char buf[8];
  snprintf(buf, sizeof(buf), "T%04X", uniqueId);
  NODE_ID = String(buf);
  myName = "Player_" + NODE_ID.substring(NODE_ID.length() - 4);
  
  Serial.println("Device ID: " + NODE_ID);
  displayBigText(NODE_ID, "Your Tracker ID");
  delay(2000);
  
  // LoRa initialization
  displayStartup("Starting LoRa radio...");
  pinMode(LORA_NSS, OUTPUT);
  digitalWrite(LORA_NSS, HIGH);
  pinMode(LORA_RST, OUTPUT);
  digitalWrite(LORA_RST, HIGH);
  
  loraSPI.begin(LORA_SCK, LORA_MISO, LORA_MOSI, LORA_NSS);
  int state = radio.begin(LORA_FREQ);
  
  if (state != RADIOLIB_ERR_NONE) {
    displayBigText("LoRa FAILED!", "Error: " + String(state));
    while (1) {
      digitalWrite(LED_PIN, !digitalRead(LED_PIN));
      delay(200);
    }
  }
  
  radio.setSpreadingFactor(7);
  radio.setBandwidth(125.0);
  radio.setCodingRate(5);
  radio.setSyncWord(0x34);
  radio.setOutputPower(14);
  radio.setDio1Action(setRxFlag);
  radio.startReceive();
  
  Serial.println("LoRa OK: " + String(LORA_FREQ) + " MHz");
  
  // WiFi connection
  displayStartup("Connecting to WiFi...");
  WiFi.mode(WIFI_STA);
  WiFi.setHostname(NODE_ID.c_str());
  WiFi.begin(wifi_ssid, wifi_pass);
  
  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < 20) {
    delay(500);
    attempts++;
    displayStartup("WiFi... " + String(attempts) + "/20");
  }
  
  if (WiFi.status() == WL_CONNECTED) {
    wifiConnected = true;
    lastSuccessfulPing = millis();
    Serial.println("WiFi OK: " + WiFi.localIP().toString());
    displayBigText("WiFi Connected!", WiFi.localIP().toString());
  } else {
    wifiConnected = false;
    displayBigText("WiFi Failed", "Will retry automatically");
  }
  delay(1500);
  
  // Ready message
  displayBigText("Ready!", "ID: " + NODE_ID);
  Serial.println("Ready!");
  Serial.println("Controls: Short=Action, Hold=Screen, 2s+3tap=Emergency");
  delay(2000);
  
  showNotification("Open " + String(server_url) + " to set role", "info");
}

// =============================================================================
// MAIN LOOP
// =============================================================================
unsigned long lastBeacon = 0, lastDisplayUpdate = 0;

void loop() {
  unsigned long now = millis();
  
  // Handle LoRa reception
  receivePackets();
  
  // Handle button input
  handleButton();
  
  // Send beacon periodically
  if (now - lastBeacon > BEACON_INTERVAL) {
    lastBeacon = now;
    sendBeacon();
  }
  
  // Ping server periodically
  if (now - lastPing > PING_INTERVAL) {
    lastPing = now;
    pingServer();
  }
  
  // Update display
  if (now - lastDisplayUpdate > DISPLAY_UPDATE_INTERVAL) {
    lastDisplayUpdate = now;
    updateScroll();
    displayScreen();
  }
  
  // Update LED
  updateLED();
  
  // Cleanup stale nearby players
  cleanupStaleNearby();
  
  // Periodic health check
  static unsigned long lastHealth = 0;
  if (now - lastHealth > 300000) {  // Every 5 minutes
    lastHealth = now;
    Serial.println("=== TRACKER HEALTH ===");
    Serial.println("ID: " + NODE_ID);
    Serial.println("Role: " + myRole);
    Serial.println("Status: " + myStatus);
    Serial.println("Points: " + String(myPoints));
    Serial.println("Nearby: " + String(nearbyPlayers.size()));
    Serial.println("TX: " + String(txCount) + " | RX: " + String(rxCount));
    Serial.println("Free Heap: " + String(ESP.getFreeHeap()) + " bytes");
    Serial.println("Uptime: " + String(millis() / 60000) + " minutes");
    checkBattery();
    Serial.println("====================");
  }
  
  delay(10);
}
