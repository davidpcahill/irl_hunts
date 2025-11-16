/*
 * IRL Hunts Tracker v4 - Complete Game Modes + Emergency System
 * 
 * Features:
 * - Player name display (once set via web)
 * - Game mode display with mode-specific hints
 * - Emergency system with hold+tap trigger
 * - LED patterns for different states
 * - Server timeout warning
 * - Team display for Team Competition
 * - Infection mode tracking
 * 
 * EMERGENCY TRIGGER: Hold button for 2 seconds, then tap 3 times quickly
 * This prevents accidental triggers while allowing quick access
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

// Load configuration - try local first, then default
#if __has_include("config_local.h")
  #include "config_local.h"
  #define CONFIG_SOURCE "config_local.h"
#elif __has_include("config.h")
  #include "config.h"
  #define CONFIG_SOURCE "config.h"
#else
  #error "No config file found! Copy config.h.example to config.h and edit it."
#endif

// Use config values for pins (with fallbacks for backward compatibility)
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

// Timing constants from config (with defaults)
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

SPIClass loraSPI(HSPI);
SX1262 radio = new Module(LORA_NSS, LORA_DIO1, LORA_RST, LORA_BUSY, loraSPI);
SSD1306Wire display(OLED_ADDR, OLED_SDA, OLED_SCL);
HTTPClient http;

String NODE_ID, myName = "", myRole = "unassigned", myStatus = "lobby";
String gamePhase = "lobby", gameMode = "classic", gameModeName = "Classic Hunt", myTeam = "";
bool inSafeZone = false, infectionMode = false, photoRequired = false;
bool emergencyActive = false;
String emergencyBy = "";
String emergencyDeviceId = "";
String emergencyReason = "";
String emergencyLocation = "";
String emergencyNearby = "";
String emergencyPhone = "";
unsigned long emergencyDisplayCycle = 0;
bool consentPhysical = false;
bool consentPhoto = true;
String consentBadge = "";  // Visual badge for LoRa broadcasts
bool wifiConnected = false, serverReachable = false;
bool myReady = false;
unsigned long lastWifiCheck = 0, lastSuccessfulPing = 0;
int consecutivePingFails = 0;

std::map<String, int> playerRssi, beaconRssi;
std::vector<String> knownBeacons;

String currentNotification = "";
unsigned long notificationExpiry = 0;
String notificationType = "info";
int scrollOffset = 0;
unsigned long lastScrollUpdate = 0;

// Capture tracking for better UX
String capturedByName = "";
String capturedByDevice = "";
std::vector<String> myCapturedPrey;  // For predators: list of prey IDs they've caught
std::map<String, String> preyNames;  // Map prey ID to name
std::map<String, bool> preyEscaped;  // Track if prey has escaped
int captureViewIndex = 0;  // For scrolling through captures
unsigned long lastCaptureViewUpdate = 0;
bool showingCaptureList = false;
unsigned long captureListExpiry = 0;

bool lastBtnState = HIGH, btnHeld = false, emergencyHoldComplete = false;
unsigned long btnPressTime = 0, emergencyTapStartTime = 0, lastBtnRelease = 0;
int emergencyTapCount = 0;

unsigned long lastLedUpdate = 0, lastPing = 0, lastBeacon = 0, lastDisplayUpdate = 0;
int ledBlinkState = 0, txCount = 0, rxCount = 0, captureAttempts = 0, escapeCount = 0;
volatile bool rxFlag = false;

void setRxFlag() { rxFlag = true; }

// Notification priority levels (higher = more important)
int getNotificationPriority(const String& type) {
  if (type == "captured") return 100;  // HIGHEST - you're caught!
  if (type == "danger") return 80;
  if (type == "escape") return 70;
  if (type == "warning") return 60;
  if (type == "success") return 50;
  if (type == "info") return 40;
  return 30;
}

void showNotification(const String& msg, const String& type = "info") {
  // Don't override higher priority notifications
  if (hasActiveNotification()) {
    int currentPriority = getNotificationPriority(notificationType);
    int newPriority = getNotificationPriority(type);
    
    // Only override if new notification is higher or equal priority
    // OR if current notification is about to expire (< 500ms left)
    if (newPriority < currentPriority && (notificationExpiry - millis() > 500)) {
      Serial.println("[NOTIF] Blocked low priority: " + msg);
      return;  // Don't override important notification
    }
  }
  
  currentNotification = msg;
  notificationType = type;
  
  // Critical notifications (captured) stay longer
  if (type == "captured") {
    notificationExpiry = millis() + 10000;  // 10 seconds for captured
  } else if (type == "danger" || type == "escape") {
    notificationExpiry = millis() + 5000;   // 5 seconds for danger/escape
  } else {
    notificationExpiry = millis() + NOTIFICATION_DURATION;
  }
  
  Serial.println("[NOTIF] " + type + ": " + msg);
}

bool hasActiveNotification() {
  return currentNotification.length() > 0 && millis() < notificationExpiry;
}

void checkWiFiConnection() {
  bool wasConnected = wifiConnected;
  wifiConnected = (WiFi.status() == WL_CONNECTED);
  
  if (!wifiConnected && wasConnected) {
    showNotification("WiFi lost! Reconnecting...", "danger");
    Serial.println("WiFi connection lost");
  } else if (wifiConnected && !wasConnected) {
    showNotification("WiFi reconnected!", "success");
    Serial.println("WiFi reconnected: " + WiFi.localIP().toString());
    consecutivePingFails = 0; // Reset ping failures on reconnect
  }
  
  if (!wifiConnected && (millis() - lastWifiCheck > WIFI_RECONNECT_INTERVAL)) {
    lastWifiCheck = millis();
    static int reconnectAttempts = 0;
    reconnectAttempts++;
    
    Serial.println("Attempting WiFi reconnect (attempt " + String(reconnectAttempts) + ")...");
    WiFi.disconnect();
    delay(100);
    WiFi.begin(wifi_ssid, wifi_pass);
    
    // Wait a bit for connection
    int attempts = 0;
    while (WiFi.status() != WL_CONNECTED && attempts < 10) {
      delay(500);
      attempts++;
    }
    
    // Reset counter on success
    if (WiFi.status() == WL_CONNECTED) {
      reconnectAttempts = 0;
    }
  }
}

void updateLED() {
  unsigned long now = millis();
  int blinkInterval = 1000;
  if (emergencyActive) blinkInterval = 100;
  else if (!wifiConnected) blinkInterval = 150;
  else if (!serverReachable) blinkInterval = 300;
  else if (myStatus == "captured") blinkInterval = 250;
  else if (inSafeZone) blinkInterval = 2000;
  else if (gamePhase == "running") blinkInterval = 800;
  if (now - lastLedUpdate > blinkInterval) {
    lastLedUpdate = now;
    ledBlinkState = !ledBlinkState;
    digitalWrite(LED_PIN, ledBlinkState ? HIGH : LOW);
  }
}

void drawStatusBar() {
  String statusLine = (myName.length() > 0 && myName.indexOf("Player_") != 0) ? myName.substring(0, 10) : NODE_ID;
  statusLine += wifiConnected ? " [W]" : " [W!]";
  statusLine += serverReachable ? "[S]" : "[S?]";
  if (myTeam.length() > 0 && myTeam != "null") statusLine += " " + myTeam.substring(0, 1);
  // Show consent badge indicator (always show for clarity)
  if (consentBadge.length() > 0) {
    statusLine += " [" + consentBadge + "]";
  }
  if (myReady) statusLine += " R";  // Ready indicator
  display.drawString(0, 0, statusLine);
}

void drawScrollingText(int y, const String& text, int maxWidth = 128) {
  int textWidth = display.getStringWidth(text);
  if (textWidth <= maxWidth) display.drawString(0, y, text);
  else {
    String displayText = text + "   " + text;
    int totalWidth = textWidth + display.getStringWidth("   ");
    int offset = scrollOffset % totalWidth;
    for (int i = 0; i < displayText.length(); i++) {
      int charX = display.getStringWidth(displayText.substring(0, i)) - offset;
      if (charX >= -10 && charX < maxWidth) display.drawString(charX, y, String(displayText[i]));
    }
  }
}

void updateScroll() {
  if (millis() - lastScrollUpdate > SCROLL_SPEED) {
    lastScrollUpdate = millis();
    scrollOffset = (scrollOffset + 1) % 1000;
  }
}

void displayEmergencyScreen() {
  display.clear();
  display.setTextAlignment(TEXT_ALIGN_CENTER);
  display.setFont(ArialMT_Plain_16);
  display.drawString(64, 0, "!!! EMERGENCY !!!");
  display.setFont(ArialMT_Plain_10);
  
  // Cycle through different info every 3 seconds
  int cycle = ((millis() - emergencyDisplayCycle) / 3000) % 4;
  
  switch (cycle) {
    case 0:  // Who needs help
      display.drawString(64, 18, "FIND THIS PLAYER:");
      display.setFont(ArialMT_Plain_16);
      display.drawString(64, 32, emergencyBy);
      display.setFont(ArialMT_Plain_10);
      display.drawString(64, 52, "Device: " + emergencyDeviceId);
      break;
    case 1:  // Phone number (if available)
      display.drawString(64, 18, "EMERGENCY CONTACT:");
      display.setFont(ArialMT_Plain_16);
      if (emergencyPhone.length() > 0) {
        display.drawString(64, 32, emergencyPhone);
      } else {
        display.drawString(64, 32, "Not provided");
      }
      display.setFont(ArialMT_Plain_10);
      display.drawString(64, 52, "CALL TO LOCATE");
      break;
    case 2:  // Location info
      display.drawString(64, 18, "LAST KNOWN LOCATION:");
      display.drawString(64, 32, emergencyLocation.length() > 0 ? emergencyLocation : "Unknown");
      display.drawString(64, 48, "HELP THEM NOW!");
      break;
    case 3:  // Nearby players
      display.drawString(64, 18, "NEARBY PLAYERS:");
      display.drawString(64, 32, emergencyNearby.length() > 0 ? emergencyNearby : "None detected");
      display.drawString(64, 48, emergencyReason.length() > 0 ? emergencyReason.substring(0, 20) : "");
      break;
  }
  
  display.display();
}

void displayNotificationScreen() {
  display.clear();
  display.setTextAlignment(TEXT_ALIGN_CENTER);
  display.setFont(ArialMT_Plain_16);
  String header = "INFO";
  if (notificationType == "danger") header = "! ALERT !";
  else if (notificationType == "warning") header = "WARNING";
  else if (notificationType == "success") header = "SUCCESS";
  display.drawString(64, 5, header);
  display.setFont(ArialMT_Plain_10);
  String msg = currentNotification;
  if (msg.length() > 21) {
    int split = msg.lastIndexOf(' ', 21);
    if (split == -1) split = 21;
    display.drawString(64, 28, msg.substring(0, split));
    if (msg.length() > split + 1) display.drawString(64, 41, msg.substring(split + 1));
  } else display.drawString(64, 32, msg);
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


void displayCaptureList() {
  display.clear();
  display.setTextAlignment(TEXT_ALIGN_CENTER);
  display.setFont(ArialMT_Plain_16);
  display.drawString(64, 0, "YOUR CAPTURES");
  display.setFont(ArialMT_Plain_10);
  
  if (myCapturedPrey.size() == 0) {
    display.drawString(64, 25, "No captures yet");
    display.drawString(64, 40, "Hunt some prey!");
  } else {
    int startIdx = captureViewIndex % myCapturedPrey.size();
    String preyId = myCapturedPrey[startIdx];
    String preyName = preyNames.count(preyId) ? preyNames[preyId] : preyId;
    bool escaped = preyEscaped.count(preyId) ? preyEscaped[preyId] : false;
    
    display.drawString(64, 18, String(startIdx + 1) + "/" + String(myCapturedPrey.size()));
    display.setFont(ArialMT_Plain_16);
    display.drawString(64, 32, preyName);
    display.setFont(ArialMT_Plain_10);
    
    if (escaped) {
      display.drawString(64, 50, "⚠️ ESCAPED! Hunt again!");
    } else {
      display.drawString(64, 50, "✓ Still captured");
    }
  }
  
  display.setTextAlignment(TEXT_ALIGN_LEFT);
  display.display();
}

void displayCapturedStatus() {
  display.clear();
  display.setTextAlignment(TEXT_ALIGN_CENTER);
  display.setFont(ArialMT_Plain_16);
  display.drawString(64, 0, "!! CAPTURED !!");
  display.setFont(ArialMT_Plain_10);
  
  if (capturedByName.length() > 0) {
    display.drawString(64, 20, "By: " + capturedByName);
  }
  
  display.setFont(ArialMT_Plain_16);
  display.drawString(64, 35, "FIND SAFE ZONE!");
  display.setFont(ArialMT_Plain_10);
  display.drawString(64, 54, "Get to beacon to escape");
  
  display.setTextAlignment(TEXT_ALIGN_LEFT);
  display.display();
}

void displayThreatAssessment() {
  display.clear();
  display.setTextAlignment(TEXT_ALIGN_CENTER);
  display.setFont(ArialMT_Plain_16);
  display.drawString(64, 0, "THREAT LEVEL");
  display.setFont(ArialMT_Plain_10);
  
  if (playerRssi.size() == 0) {
    display.drawString(64, 25, "✓ NO THREATS");
    display.drawString(64, 40, "Area appears clear");
  } else {
    int closest = -999;
    String closestId = "";
    int dangerCount = 0;
    
    for (auto& p : playerRssi) {
      if (p.second > closest) {
        closest = p.second;
        closestId = p.first;
      }
      if (p.second > -70) dangerCount++;
    }
    
    if (closest > -50) {
      display.drawString(64, 18, "🚨 EXTREME DANGER!");
      display.drawString(64, 32, "Predator: " + closestId);
      display.drawString(64, 46, String(closest) + "dB - RUN NOW!");
    } else if (closest > -65) {
      display.drawString(64, 18, "⚠️ HIGH THREAT");
      display.drawString(64, 32, dangerCount > 1 ? String(dangerCount) + " predators nearby" : "Predator nearby");
      display.drawString(64, 46, "Closest: " + String(closest) + "dB");
    } else {
      display.drawString(64, 18, "⚡ MODERATE RISK");
      display.drawString(64, 32, String(playerRssi.size()) + " detected");
      display.drawString(64, 46, "Stay alert!");
    }
  }
  
  display.setTextAlignment(TEXT_ALIGN_LEFT);
  display.display();
}

void displayMain() {
  if (emergencyActive) { displayEmergencyScreen(); return; }
  if (!serverReachable && (millis() - lastSuccessfulPing) > SERVER_TIMEOUT_THRESHOLD) { displayServerTimeout(); return; }
  
  // CAPTURED status takes ABSOLUTE priority for prey
  if (myRole == "prey" && myStatus == "captured" && !hasActiveNotification()) {
    displayCapturedStatus();
    return;
  }
  
  // Show capture list if predator requested it
  if (showingCaptureList && millis() < captureListExpiry) {
    displayCaptureList();
    return;
  } else {
    showingCaptureList = false;
  }
  
  if (hasActiveNotification()) { displayNotificationScreen(); return; }
  
  display.clear();
  display.setTextAlignment(TEXT_ALIGN_LEFT);
  display.setFont(ArialMT_Plain_10);
  
  if (myRole == "unassigned") {
    drawStatusBar();
    display.drawString(0, 14, "ROLE NOT SET");
    display.drawString(0, 26, "Open phone browser:");
    drawScrollingText(38, String(server_url));
    display.drawString(0, 50, "Enter ID: " + NODE_ID);
    display.display();
    return;
  }
  
  drawStatusBar();
  
  String line2 = "";
  if (myRole == "pred") {
    if (infectionMode) line2 = "PREDATOR (Infect!)";
    else if (photoRequired) line2 = "PREDATOR (Photo first!)";
    else {
      // Show capture count for predators
      int activeCaptures = 0;
      for (auto& prey : myCapturedPrey) {
        if (!preyEscaped[prey]) activeCaptures++;
      }
      if (activeCaptures > 0) {
        line2 = "PRED | " + String(activeCaptures) + " captured";
      } else {
        line2 = "PREDATOR (Hunt!)";
      }
    }
  } else if (myRole == "prey") {
    if (myStatus == "captured") {
      line2 = "!! CAPTURED !!";
    } else if (infectionMode) {
      line2 = "PREY (Don't get infected!)";
    } else {
      line2 = "PREY (Survive!)";
    }
  }
  display.drawString(0, 12, line2);
  
  String line3 = gameModeName;
  if (gamePhase == "running") line3 += " [LIVE]";
  else if (gamePhase == "paused") line3 += " [PAUSED]";
  else if (gamePhase == "countdown") line3 += " [STARTING]";
  else if (gamePhase == "ended") line3 += " [ENDED]";
  else line3 += " [" + gamePhase + "]";
  drawScrollingText(24, line3);
  
  String line4 = "";
  if (myStatus == "captured" && myRole == "prey") {
    if (capturedByName.length() > 0) {
      line4 = "By: " + capturedByName + " | FIND BEACON!";
    } else {
      line4 = "CAPTURED! Find safe zone!";
    }
  } else if (inSafeZone) {
    line4 = "🏠 SAFE ZONE";
  } else {
    line4 = "Status: " + myStatus;
  }
  display.drawString(0, 36, line4);
  
  // Show battery if available
  float batt = getBatteryVoltage();
  if (batt > 2.5 && batt < 4.5) {
    String battStr = String(batt, 1) + "V";
    int battX = 128 - display.getStringWidth(battStr);
    display.drawString(battX, 36, battStr);
  }
  
  String line5 = "";
  if (myRole == "pred") {
    // Predator sees targets
    if (playerRssi.size() == 0) {
      line5 = "No prey detected";
    } else {
      int strongest = -999; String strongestId = "";
      for (auto& p : playerRssi) {
        if (p.second > strongest) { strongest = p.second; strongestId = p.first; }
      }
      if (playerRssi.size() == 1) {
        line5 = "🎯 Target: " + strongestId + " " + String(strongest) + "dB";
      } else {
        line5 = "🎯 " + String(playerRssi.size()) + " prey | Closest: " + String(strongest) + "dB";
      }
    }
  } else {
    // Prey sees threats
    if (myStatus == "captured") {
      line5 = "BTN = Show escape info";
    } else if (playerRssi.size() == 0) {
      line5 = "✓ No threats nearby";
    } else {
      int strongest = -999;
      for (auto& p : playerRssi) {
        if (p.second > strongest) strongest = p.second;
      }
      if (strongest > -60) {
        line5 = "⚠️ DANGER! " + String(playerRssi.size()) + " pred " + String(strongest) + "dB";
      } else {
        line5 = "⚠️ " + String(playerRssi.size()) + " pred nearby | " + String(strongest) + "dB";
      }
    }
  }
  drawScrollingText(48, line5);
  display.display();
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

void displayEmergencyConfirm() {
  display.clear();
  display.setTextAlignment(TEXT_ALIGN_CENTER);
  display.setFont(ArialMT_Plain_16);
  display.drawString(64, 10, "EMERGENCY?");
  display.setFont(ArialMT_Plain_10);
  display.drawString(64, 32, "Tap " + String(EMERGENCY_TAP_COUNT - emergencyTapCount) + " more times");
  display.drawString(64, 46, "to confirm");
  display.display();
}

void sendBeacon() {
  String broadcastRole = (myRole == "unassigned") ? "unknown" : myRole;
  // Include consent badge in broadcast: ID|role|consent
  String packet = NODE_ID + "|" + broadcastRole + "|" + consentBadge;
  
  int state = radio.transmit(packet);
  if (state == RADIOLIB_ERR_NONE) {
    txCount++;
    // Only log every 10th TX to reduce spam
    if (txCount % 10 == 1) {
      Serial.print("TX #");
      Serial.print(txCount);
      Serial.print(": ");
      Serial.print(packet);
      Serial.print(" | Nearby players: ");
      Serial.println(playerRssi.size());
    }
  } else {
    Serial.print("TX ERROR: ");
    Serial.println(state);
  }
  
  radio.startReceive();
}

// Track predator approach trends
std::map<String, int> lastPredRssi;

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
      
      // CRITICAL FIX: Parse consent badge BEFORE checking type!
      // Packet format: ID|role|consent (e.g., "T1234|pred|STD")
      String type = typeAndConsent;
      String otherConsent = "STD";
      int sep2 = typeAndConsent.indexOf('|');
      if (sep2 > 0) {
        type = typeAndConsent.substring(0, sep2);  // Extract just the role
        otherConsent = typeAndConsent.substring(sep2 + 1);  // Extract consent badge
      }
      
      if (id != NODE_ID) {
        if (type == "SAFEZONE" || type == "BEACON") {
          beaconRssi[id] = (int)rssi;
        } else if (type == "pred" || type == "prey" || type == "unknown") {
          bool isNewPlayer = (playerRssi.count(id) == 0);
          int oldRssi = isNewPlayer ? -999 : playerRssi[id];
          playerRssi[id] = (int)rssi;
          rxCount++;
          
          // Debug: Log received player beacon
          Serial.print("RX from ");
          Serial.print(id);
          Serial.print(" (");
          Serial.print(type);
          Serial.print(") RSSI: ");
          Serial.print((int)rssi);
          Serial.print("dB | Total nearby: ");
          Serial.println(playerRssi.size());
          
          // NEW PLAYER DETECTION - Priority alerts!
          if (isNewPlayer && !emergencyActive && gamePhase == "running") {
            if (myRole == "prey" && type == "pred") {
              showNotification("⚠️ NEW PREDATOR DETECTED! " + id, "danger");
            } else if (myRole == "pred" && type == "prey") {
              showNotification("🎯 NEW PREY SPOTTED! " + id, "success");
            }
          }
          
          // Proximity warnings for prey (but NOT if already captured!)
          if (!emergencyActive && myRole == "prey" && type == "pred" && !inSafeZone && myStatus != "captured") {
            bool approaching = (oldRssi != -999 && (int)rssi > oldRssi + 3);
            int predCount = 0;
            for (auto& p : playerRssi) {
              if (p.second > -80) predCount++;
            }
            
            // Lower priority warnings - captured status will override these
            if (rssi > -50) {
              showNotification("🚨 CRITICAL! Predator RIGHT HERE!", "danger");
            } else if (rssi > -55) {
              if (predCount > 1) {
                showNotification("🚨 DANGER! Multiple threats very close!", "danger");
              } else {
                showNotification("🚨 DANGER! Predator very close!", "danger");
              }
            } else if (rssi > -65) {
              if (approaching) {
                showNotification("⚠️ Predator approaching fast!", "warning");
              } else {
                showNotification("⚠️ Predator nearby (" + String(playerRssi.size()) + " total)", "warning");
              }
            } else if (rssi > -75 && approaching) {
              showNotification("📡 Predator detected, moving closer", "info");
            }
          }
          
          // Alert predators about multiple prey
          if (!emergencyActive && myRole == "pred" && type == "prey" && !isNewPlayer) {
            if (rssi > -60 && oldRssi < -70) {
              showNotification("🎯 Prey " + id + " now in range!", "success");
            }
          }
        }
      }
    }
  }
  radio.startReceive();
}

// Clean up stale player entries (not heard in 30 seconds)
void cleanupStalePlayerRssi() {
  static unsigned long lastCleanup = 0;
  if (millis() - lastCleanup < 10000) return; // Only check every 10 seconds
  lastCleanup = millis();
  
  // Remove entries that haven't been updated recently
  // Since we can't track timestamps per entry easily, we'll just
  // let natural packet reception refresh the map
  // If map grows too large (memory concern), clear it
  if (playerRssi.size() > 50) {
    playerRssi.clear();
    Serial.println("Cleared large playerRssi map");
  }
}

void pingServer() {
  checkWiFiConnection();
  if (!wifiConnected) { serverReachable = false; return; }
  
  StaticJsonDocument<1024> doc;
  doc["device_id"] = NODE_ID;
  JsonObject pRssi = doc.createNestedObject("player_rssi");
  for (auto& p : playerRssi) pRssi[p.first] = p.second;
  JsonObject bRssi = doc.createNestedObject("beacon_rssi");
  for (auto& b : beaconRssi) bRssi[b.first] = b.second;
  String json; serializeJson(doc, json);
  
  http.begin(String(server_url) + "/api/tracker/ping");
  http.addHeader("Content-Type", "application/json");
  http.setTimeout(5000);
  int code = http.POST(json);
  
  if (code == 200) {
    serverReachable = true;
    consecutivePingFails = 0;
    lastSuccessfulPing = millis();
    String resp = http.getString();
    StaticJsonDocument<1024> respDoc;
    if (!deserializeJson(respDoc, resp)) {
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
      // Handle team - check for null before converting to string
      String newTeam = "";
      if (!respDoc["team"].isNull()) {
        newTeam = respDoc["team"].as<String>();
      }
      bool newConsentPhysical = respDoc["consent_physical"] | false;
      bool newConsentPhoto = respDoc["consent_photo"] | true;
      
      // Handle consent_badge - check for null and provide default
      String newConsentBadge = "STD";
      if (!respDoc["consent_badge"].isNull()) {
        newConsentBadge = respDoc["consent_badge"].as<String>();
      }
      bool newReady = respDoc["ready"] | false;
      
      if (newName.length() > 0) myName = newName;
      if (newConsentBadge.length() > 0) consentBadge = newConsentBadge;
      myReady = newReady;
      if (gameMode != newMode && newMode.length() > 0) {
        gameMode = newMode; gameModeName = newModeName;
        showNotification("Mode: " + newModeName, "info");
      }
      infectionMode = newInfection; photoRequired = newPhotoReq;
      if (newTeam.length() > 0 && myTeam != newTeam) {
        myTeam = newTeam;
        showNotification("Team: " + myTeam, "info");
      } else if (newTeam.length() == 0) myTeam = "";
      
      if (newEmergency && !emergencyActive) {
        emergencyActive = true; 
        emergencyBy = newEmergencyBy;
        emergencyDisplayCycle = millis();
        
        // Parse emergency info if available
        if (respDoc.containsKey("emergency_info")) {
          JsonObject eInfo = respDoc["emergency_info"];
          if (!eInfo["device_id"].isNull()) {
            emergencyDeviceId = eInfo["device_id"].as<String>();
          }
          if (!eInfo["reason"].isNull()) {
            emergencyReason = eInfo["reason"].as<String>();
          }
          if (!eInfo["location"].isNull()) {
            emergencyLocation = eInfo["location"].as<String>();
          }
          if (eInfo.containsKey("nearby")) {
            JsonArray nearby = eInfo["nearby"];
            emergencyNearby = "";
            for (int i = 0; i < nearby.size() && i < 3; i++) {
              if (i > 0) emergencyNearby += ", ";
              emergencyNearby += nearby[i].as<String>();
            }
          }
          if (!eInfo["phone"].isNull()) {
            emergencyPhone = eInfo["phone"].as<String>();
          } else {
            emergencyPhone = "";
          }
        }
        
        showNotification("EMERGENCY! Help " + emergencyBy + "!", "danger");
      } else if (!newEmergency && emergencyActive) {
        emergencyActive = false; 
        emergencyBy = "";
        emergencyDeviceId = "";
        emergencyReason = "";
        emergencyLocation = "";
        emergencyNearby = "";
        emergencyPhone = "";
        showNotification("Emergency cleared", "success");
      }
      if (gamePhase != newPhase) {
        gamePhase = newPhase;
        if (newPhase == "running") showNotification("Game started!", "success");
        else if (newPhase == "paused") showNotification("Game paused", "warning");
        else if (newPhase == "ended") showNotification("Game ended!", "info");
      }
      if (myStatus != newStatus) {
        String oldStatus = myStatus; myStatus = newStatus;
        if (newStatus == "captured") {
          // Get capture info from response
          if (!respDoc["captured_by_name"].isNull()) {
            capturedByName = respDoc["captured_by_name"].as<String>();
          }
          if (!respDoc["captured_by_device"].isNull()) {
            capturedByDevice = respDoc["captured_by_device"].as<String>();
          }
          showNotification("CAPTURED by " + (capturedByName.length() > 0 ? capturedByName : "predator") + "!", "captured");
        } else if (oldStatus == "captured" && newStatus == "active") {
          escapeCount++;
          capturedByName = "";
          capturedByDevice = "";
          showNotification("YOU ESCAPED! You're free!", "escape");
        }
      }
      
      // Update capture list for predators
      if (myRole == "pred" && respDoc.containsKey("my_captures")) {
        JsonArray captures = respDoc["my_captures"];
        
        // Check for new captures or escapes
        for (JsonVariant cap : captures) {
          String preyId = cap["device_id"].as<String>();
          String preyName = cap["name"].as<String>();
          bool escaped = cap["escaped"].as<bool>();
          
          // Store prey name
          preyNames[preyId] = preyName;
          
          // Check if this is a new capture
          bool found = false;
          for (auto& existing : myCapturedPrey) {
            if (existing == preyId) {
              found = true;
              // Check if they escaped (and we didn't know)
              if (escaped && !preyEscaped[preyId]) {
                preyEscaped[preyId] = true;
                showNotification("⚠️ " + preyName + " ESCAPED!", "warning");
              }
              break;
            }
          }
          
          if (!found) {
            // New capture!
            myCapturedPrey.push_back(preyId);
            preyEscaped[preyId] = escaped;
            if (!escaped) {
              showNotification("🎯 Captured " + preyName + "!", "success");
            }
          }
        }
      }
      if (myRole != newRole && newRole != "unassigned" && newRole.length() > 0) {
        String oldRole = myRole; myRole = newRole;
        if (oldRole == "prey" && newRole == "pred") showNotification("INFECTED! Now PREDATOR!", "danger");
        else showNotification("Role: " + myRole, "success");
      }
      if (inSafeZone != newSafe) {
        inSafeZone = newSafe;
        showNotification(newSafe ? "Entered SAFE ZONE" : "Left safe zone!", newSafe ? "success" : "warning");
      }
      if (respDoc.containsKey("notifications")) {
        JsonArray notifs = respDoc["notifications"];
        for (JsonVariant n : notifs) {
          String msg = n["message"].as<String>();
          String type = n["type"].as<String>();
          showNotification(msg, type.length() > 0 ? type : "info");
        }
      }
      if (respDoc.containsKey("beacons")) {
        knownBeacons.clear();
        JsonArray beacons = respDoc["beacons"];
        for (JsonVariant b : beacons) knownBeacons.push_back(b.as<String>());
      }
    }
  } else {
    consecutivePingFails++;
    if (consecutivePingFails >= 3 && serverReachable) {
      serverReachable = false;
      showNotification("Server connection lost", "warning");
    }
    // Log specific HTTP errors
    if (code < 0) {
      Serial.println("HTTP Error: " + http.errorToString(code));
    } else if (code == 404) {
      Serial.println("Server endpoint not found (404)");
    } else if (code == 500) {
      Serial.println("Server internal error (500)");
    } else {
      Serial.println("HTTP Code: " + String(code));
    }
  }
  http.end();
  // DON'T clear playerRssi here - we need it for display and capture attempts
  // Old entries will be naturally replaced when new packets arrive
  // Only clear beaconRssi as it's repopulated each cycle
  beaconRssi.clear();
}

void attemptCapture() {
  Serial.println("=== CAPTURE ATTEMPT ===");
  Serial.print("Role: "); Serial.println(myRole);
  Serial.print("Game Phase: "); Serial.println(gamePhase);
  Serial.print("Emergency: "); Serial.println(emergencyActive ? "YES" : "NO");
  Serial.print("In Safe Zone: "); Serial.println(inSafeZone ? "YES" : "NO");
  Serial.print("Players in RSSI map: "); Serial.println(playerRssi.size());
  
  if (myRole != "pred") { showNotification("Only predators capture", "warning"); return; }
  if (gamePhase != "running") { showNotification("Game not active!", "warning"); return; }
  if (emergencyActive) { showNotification("EMERGENCY - No captures!", "danger"); return; }
  if (inSafeZone) { showNotification("Can't capture from safe zone", "warning"); return; }
  if (playerRssi.empty()) { 
    Serial.println("ERROR: playerRssi is EMPTY - no one detected!");
    showNotification("No prey detected nearby", "info"); 
    return; 
  }
  
  int bestRssi = -999; String targetId = "";
  for (auto& p : playerRssi) {
    if (p.second > bestRssi) { bestRssi = p.second; targetId = p.first; }
  }
  captureAttempts++;
  if (photoRequired) {
    showNotification("Capturing... (Photo required!)", "warning");
  } else {
    showNotification("Attempting capture...", "info");
  }
  
  StaticJsonDocument<256> doc;
  doc["pred_id"] = NODE_ID; doc["prey_id"] = targetId; doc["rssi"] = bestRssi;
  String json; serializeJson(doc, json);
  
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
      displayBigText(infectionMode ? "INFECTED!" : "CAPTURED!", targetId);
      delay(2000);
    } else showNotification("Failed: " + msg, "warning");
  } else showNotification("Server error", "danger");
  http.end();
}

void triggerEmergency() {
  showNotification("Sending emergency alert...", "danger");
  StaticJsonDocument<256> doc;
  doc["device_id"] = NODE_ID;
  doc["reason"] = "Emergency triggered from tracker device";
  String json; serializeJson(doc, json);
  
  http.begin(String(server_url) + "/api/tracker/emergency");
  http.addHeader("Content-Type", "application/json");
  http.setTimeout(5000);
  int code = http.POST(json);
  if (code == 200) {
    showNotification("EMERGENCY SENT! Help coming!", "success");
    displayBigText("EMERGENCY", "ALERT SENT!");
    delay(3000);
  } else showNotification("Failed to send emergency", "danger");
  http.end();
  emergencyHoldComplete = false; emergencyTapCount = 0;
}

void handleButton() {
  bool btnState = digitalRead(BUTTON_PIN);
  unsigned long now = millis();
  
  if (btnState == LOW && lastBtnState == HIGH) {
    btnPressTime = now; btnHeld = false;
    if (emergencyHoldComplete) {
      emergencyTapCount++;
      if (emergencyTapCount >= EMERGENCY_TAP_COUNT) triggerEmergency();
      else displayEmergencyConfirm();
    }
  }
  
  if (btnState == LOW && !btnHeld) {
    unsigned long holdTime = now - btnPressTime;
    if (holdTime >= EMERGENCY_HOLD_TIME && !emergencyHoldComplete) {
      emergencyHoldComplete = true; emergencyTapCount = 0; emergencyTapStartTime = now;
      showNotification("Release & tap " + String(EMERGENCY_TAP_COUNT) + "x for emergency", "warning");
    }
  }
  
  if (btnState == HIGH && lastBtnState == LOW) {
    unsigned long holdTime = now - btnPressTime;
    lastBtnRelease = now;
    if (!emergencyHoldComplete && holdTime < EMERGENCY_HOLD_TIME) {
      if (myRole == "pred") {
        // Toggle between capture attempt and viewing capture list
        static bool lastWasCapture = true;
        if (playerRssi.size() > 0 && lastWasCapture) {
          attemptCapture();
          lastWasCapture = false;
        } else if (myCapturedPrey.size() > 0) {
          // Show capture list
          showingCaptureList = true;
          captureListExpiry = millis() + 5000;  // Show for 5 seconds
          captureViewIndex++;  // Cycle through captures
          lastWasCapture = true;
        } else {
          attemptCapture();  // No captures yet, just try to capture
          lastWasCapture = false;
        }
      } else if (myRole == "prey") {
        if (myStatus == "captured") {
          // Show detailed capture info
          String msg = "CAPTURED";
          if (capturedByName.length() > 0) {
            msg += " by " + capturedByName;
          }
          msg += "! Find a SAFE ZONE beacon to escape. Look for SZ beacons nearby!";
          showNotification(msg, "captured");
        } else if (inSafeZone) {
          showNotification("✓ SAFE ZONE! You're protected here. " + String(playerRssi.size()) + " signals detected", "success");
        } else {
          // Show threat assessment
          displayThreatAssessment();
          delay(3000);  // Show for 3 seconds
        }
      } else {
        showNotification("Set role at " + String(server_url), "info");
      }
    }
    btnHeld = true;
  }
  
  if (emergencyHoldComplete && (now - emergencyTapStartTime > EMERGENCY_TAP_WINDOW)) {
    if (emergencyTapCount < EMERGENCY_TAP_COUNT) {
      emergencyHoldComplete = false; emergencyTapCount = 0;
      showNotification("Emergency cancelled", "info");
    }
  }
  lastBtnState = btnState;
}

void setup() {
  Serial.begin(115200); delay(500);
  Serial.println("\n========================================");
  Serial.println("IRL Hunts Tracker v4.1");
  Serial.println("Build: " __DATE__ " " __TIME__);
  Serial.println("Config: " CONFIG_SOURCE);
  Serial.println("========================================");
  
  pinMode(BUTTON_PIN, INPUT_PULLUP);
  pinMode(LED_PIN, OUTPUT); digitalWrite(LED_PIN, HIGH);
  pinMode(VEXT_PIN, OUTPUT); digitalWrite(VEXT_PIN, LOW); delay(100);
  pinMode(OLED_RST, OUTPUT);
  digitalWrite(OLED_RST, LOW); delay(50);
  digitalWrite(OLED_RST, HIGH); delay(100);
  
  Wire.begin(OLED_SDA, OLED_SCL);
  display.init(); display.flipScreenVertically();
  display.setFont(ArialMT_Plain_10);
  displayStartup("Initializing...");
  
  uint64_t mac = ESP.getEfuseMac();
  uint16_t uniqueId = (uint16_t)(mac & 0xFFFF) ^ (uint16_t)((mac >> 32) & 0xFFFF) ^ (uint16_t)(mac >> 16);
  char buf[8]; snprintf(buf, sizeof(buf), "T%04X", uniqueId);
  NODE_ID = String(buf);
  myName = "Player_" + NODE_ID.substring(NODE_ID.length() - 4);
  Serial.println("Device ID: " + NODE_ID);
  displayBigText(NODE_ID, "Your Tracker ID"); delay(2000);
  
  displayStartup("Starting LoRa radio...");
  pinMode(LORA_NSS, OUTPUT); digitalWrite(LORA_NSS, HIGH);
  pinMode(LORA_RST, OUTPUT); digitalWrite(LORA_RST, HIGH);
  loraSPI.begin(LORA_SCK, LORA_MISO, LORA_MOSI, LORA_NSS);
  int state = radio.begin(LORA_FREQ);
  if (state != RADIOLIB_ERR_NONE) {
    displayBigText("LoRa FAILED!", "Error: " + String(state));
    while (1) { digitalWrite(LED_PIN, !digitalRead(LED_PIN)); delay(200); }
  }
  radio.setSpreadingFactor(7); radio.setBandwidth(125.0);
  radio.setCodingRate(5); radio.setSyncWord(0x34);
  radio.setOutputPower(14); radio.setDio1Action(setRxFlag);
  radio.startReceive();
  Serial.println("LoRa OK: " + String(LORA_FREQ) + " MHz");
  
  displayStartup("Connecting to WiFi...");
  WiFi.mode(WIFI_STA); WiFi.setHostname(NODE_ID.c_str());
  WiFi.begin(wifi_ssid, wifi_pass);
  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < 20) {
    delay(500); attempts++;
    displayStartup("WiFi... " + String(attempts) + "/20");
  }
  if (WiFi.status() == WL_CONNECTED) {
    wifiConnected = true; lastSuccessfulPing = millis();
    Serial.println("WiFi OK: " + WiFi.localIP().toString());
    displayBigText("WiFi Connected!", WiFi.localIP().toString());
  } else {
    wifiConnected = false;
    displayBigText("WiFi Failed", "Will retry automatically");
  }
  delay(1500);
  
  displayBigText("Ready!", "ID: " + NODE_ID);
  Serial.println("Ready! Emergency: Hold 2s + tap 3x");
  delay(2000);
  
  // Initialize capture tracking
  myCapturedPrey.clear();
  preyNames.clear();
  preyEscaped.clear();
  capturedByName = "";
  capturedByDevice = "";
  
  showNotification("Open browser to set your role", "info");
}

float getBatteryVoltage() {
  // Heltec V3 battery monitoring
  // Uses internal ADC with voltage divider on board
  // GPIO1 is connected to battery through voltage divider
  
  // Configure ADC for battery reading
  analogSetPinAttenuation(1, ADC_11db);  // Full range 0-3.3V
  
  int adcValue = analogRead(1);
  
  // Heltec V3 has a voltage divider: 390K / 100K
  // So actual voltage = ADC voltage * (390 + 100) / 100 = ADC * 4.9
  // But ADC reads 0-4095 for 0-3.3V
  float adcVoltage = (adcValue / 4095.0) * 3.3;
  float batteryVoltage = adcVoltage * 4.9;
  
  // Sanity check - LiPo should be 3.0V - 4.2V
  if (batteryVoltage < 2.5 || batteryVoltage > 4.5) {
    return 0.0;  // Invalid reading
  }
  
  return batteryVoltage;
}

void checkBattery() {
  float voltage = getBatteryVoltage();
  static bool lowBatteryWarned = false;
  
  if (voltage < 3.3 && !lowBatteryWarned) {
    showNotification("LOW BATTERY! Charge soon!", "danger");
    lowBatteryWarned = true;
    Serial.println("Battery low: " + String(voltage) + "V");
  } else if (voltage > 3.5) {
    lowBatteryWarned = false;
  }
}

void loop() {
  unsigned long now = millis();
  receivePackets();
  handleButton();
  if (now - lastBeacon > BEACON_INTERVAL) { lastBeacon = now; sendBeacon(); }
  if (now - lastPing > PING_INTERVAL) { lastPing = now; pingServer(); }
  if (now - lastDisplayUpdate > DISPLAY_UPDATE_INTERVAL) {
    lastDisplayUpdate = now; updateScroll(); displayMain();
  }
  updateLED();
  cleanupStalePlayerRssi();
  
  // Memory monitoring (every 5 minutes)
  static unsigned long lastMemCheck = 0;
  if (now - lastMemCheck > 300000) {
    lastMemCheck = now;
    Serial.println("Free heap: " + String(ESP.getFreeHeap()) + " bytes");
    Serial.println("Uptime: " + String(millis() / 60000) + " minutes");
    Serial.println("TX: " + String(txCount) + " | RX: " + String(rxCount));
    checkBattery();  // Also check battery
  }
  
  delay(10);
}
