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

#define LORA_NSS    8
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

const char* WIFI_SSID = "YOUR_WIFI_NAME";
const char* WIFI_PASS = "YOUR_WIFI_PASSWORD";
const char* SERVER_URL = "http://YOUR_SERVER_IP:5000";

const unsigned long PING_INTERVAL = 5000;
const unsigned long BEACON_INTERVAL = 3000;
const unsigned long DISPLAY_UPDATE_INTERVAL = 100;
const unsigned long SCROLL_SPEED = 150;
const unsigned long WIFI_RECONNECT_INTERVAL = 15000;  // Faster reconnect
const unsigned long NOTIFICATION_DURATION = 3000;
const unsigned long SERVER_TIMEOUT_THRESHOLD = 45000;
const unsigned long EMERGENCY_HOLD_TIME = 2000;
const int EMERGENCY_TAP_COUNT = 3;
const unsigned long EMERGENCY_TAP_WINDOW = 2000;

SPIClass loraSPI(HSPI);
SX1262 radio = new Module(LORA_NSS, LORA_DIO1, LORA_RST, LORA_BUSY, loraSPI);
SSD1306Wire display(OLED_ADDR, OLED_SDA, OLED_SCL);
HTTPClient http;

String NODE_ID, myName = "", myRole = "unassigned", myStatus = "lobby";
String gamePhase = "lobby", gameMode = "classic", gameModeName = "Classic Hunt", myTeam = "";
bool inSafeZone = false, infectionMode = false, photoRequired = false;
bool emergencyActive = false;
String emergencyBy = "";
bool wifiConnected = false, serverReachable = false;
unsigned long lastWifiCheck = 0, lastSuccessfulPing = 0;
int consecutivePingFails = 0;

std::map<String, int> playerRssi, beaconRssi;
std::vector<String> knownBeacons;

String currentNotification = "";
unsigned long notificationExpiry = 0;
String notificationType = "info";
int scrollOffset = 0;
unsigned long lastScrollUpdate = 0;

bool lastBtnState = HIGH, btnHeld = false, emergencyHoldComplete = false;
unsigned long btnPressTime = 0, emergencyTapStartTime = 0, lastBtnRelease = 0;
int emergencyTapCount = 0;

unsigned long lastLedUpdate = 0, lastPing = 0, lastBeacon = 0, lastDisplayUpdate = 0;
int ledBlinkState = 0, txCount = 0, rxCount = 0, captureAttempts = 0, escapeCount = 0;
volatile bool rxFlag = false;

void setRxFlag() { rxFlag = true; }

void showNotification(const String& msg, const String& type = "info") {
  currentNotification = msg;
  notificationType = type;
  notificationExpiry = millis() + NOTIFICATION_DURATION;
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
    Serial.println("Attempting WiFi reconnect...");
    WiFi.disconnect();
    delay(100);
    WiFi.begin(WIFI_SSID, WIFI_PASS);
    
    // Wait a bit for connection
    int attempts = 0;
    while (WiFi.status() != WL_CONNECTED && attempts < 10) {
      delay(500);
      attempts++;
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
  if (myTeam.length() > 0) statusLine += " " + myTeam.substring(0, 1);
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
  display.drawString(64, 20, "GAME PAUSED");
  display.drawString(64, 32, "Player: " + emergencyBy);
  display.drawString(64, 44, "HELP THEM NOW!");
  display.drawString(64, 54, "Only mod can clear");
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

void displayMain() {
  if (emergencyActive) { displayEmergencyScreen(); return; }
  if (!serverReachable && (millis() - lastSuccessfulPing) > SERVER_TIMEOUT_THRESHOLD) { displayServerTimeout(); return; }
  if (hasActiveNotification()) { displayNotificationScreen(); return; }
  
  display.clear();
  display.setTextAlignment(TEXT_ALIGN_LEFT);
  display.setFont(ArialMT_Plain_10);
  
  if (myRole == "unassigned") {
    drawStatusBar();
    display.drawString(0, 14, "ROLE NOT SET");
    display.drawString(0, 26, "Open phone browser:");
    drawScrollingText(38, String(SERVER_URL));
    display.drawString(0, 50, "Enter ID: " + NODE_ID);
    display.display();
    return;
  }
  
  drawStatusBar();
  
  String line2 = "";
  if (myRole == "pred") {
    if (infectionMode) line2 = "PREDATOR (Infect!)";
    else if (photoRequired) line2 = "PREDATOR (Photo first!)";
    else line2 = "PREDATOR (Hunt!)";
  } else if (myRole == "prey") {
    line2 = infectionMode ? "PREY (Don't get infected!)" : "PREY (Survive!)";
  }
  display.drawString(0, 12, line2);
  
  String line3 = gameModeName;
  if (gamePhase == "running") line3 += " [LIVE]";
  else if (gamePhase == "paused") line3 += " [PAUSED]";
  else if (gamePhase == "countdown") line3 += " [STARTING]";
  else if (gamePhase == "ended") line3 += " [ENDED]";
  else line3 += " [" + gamePhase + "]";
  drawScrollingText(24, line3);
  
  String line4 = "Status: " + myStatus;
  if (inSafeZone) {
    line4 = "🏠 IN SAFE ZONE";
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
  if (playerRssi.size() == 0) line5 = "No players nearby";
  else {
    int strongest = -999; String strongestId = "";
    for (auto& p : playerRssi) {
      if (p.second > strongest) { strongest = p.second; strongestId = p.first; }
    }
    line5 = (myRole == "pred" ? "Target: " : "Threat: ") + strongestId + " (" + String(strongest) + "dB)";
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
  String packet = NODE_ID + "|" + broadcastRole;
  radio.transmit(packet);
  radio.startReceive();
  txCount++;
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
      String type = packet.substring(sep + 1);
      if (id != NODE_ID) {
        if (type == "SAFEZONE" || type == "BEACON") {
          beaconRssi[id] = (int)rssi;
        } else if (type == "pred" || type == "prey" || type == "unknown") {
          int oldRssi = playerRssi.count(id) ? playerRssi[id] : -999;
          playerRssi[id] = (int)rssi;
          rxCount++;
          
          // Proximity warnings for prey
          if (!emergencyActive && myRole == "prey" && type == "pred" && !inSafeZone) {
            bool approaching = (oldRssi != -999 && (int)rssi > oldRssi + 3);
            
            if (rssi > -50) {
              showNotification("CRITICAL! Predator RIGHT HERE!", "danger");
            } else if (rssi > -55) {
              showNotification("DANGER! Predator very close!", "danger");
            } else if (rssi > -65) {
              if (approaching) {
                showNotification("WARNING! Predator approaching fast!", "danger");
              } else {
                showNotification("Predator nearby", "warning");
              }
            } else if (rssi > -75 && approaching) {
              showNotification("Predator detected, moving closer", "warning");
            }
          }
        }
      }
    }
  }
  radio.startReceive();
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
  
  http.begin(String(SERVER_URL) + "/api/tracker/ping");
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
      String newTeam = respDoc["team"].as<String>();
      
      if (newName.length() > 0) myName = newName;
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
        emergencyActive = true; emergencyBy = newEmergencyBy;
        showNotification("EMERGENCY! Help " + emergencyBy + "!", "danger");
      } else if (!newEmergency && emergencyActive) {
        emergencyActive = false; emergencyBy = "";
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
        if (newStatus == "captured") showNotification("YOU WERE CAPTURED!", "danger");
        else if (oldStatus == "captured" && newStatus == "active") {
          escapeCount++;
          showNotification("YOU ESCAPED!", "success");
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
  playerRssi.clear();
  beaconRssi.clear(); // Clear to prevent memory buildup
}

void attemptCapture() {
  if (myRole != "pred") { showNotification("Only predators capture", "warning"); return; }
  if (gamePhase != "running") { showNotification("Game not active!", "warning"); return; }
  if (emergencyActive) { showNotification("EMERGENCY - No captures!", "danger"); return; }
  if (inSafeZone) { showNotification("Can't capture from safe zone", "warning"); return; }
  if (playerRssi.empty()) { showNotification("No prey detected nearby", "info"); return; }
  
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
  
  http.begin(String(SERVER_URL) + "/api/tracker/capture");
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
  
  http.begin(String(SERVER_URL) + "/api/tracker/emergency");
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
      if (myRole == "pred") attemptCapture();
      else if (myRole == "prey") {
        if (myStatus == "captured") {
          if (infectionMode) {
            showNotification("INFECTED! You're now a predator!", "danger");
          } else {
            showNotification("CAPTURED! Find safe zone NOW!", "danger");
          }
        } else if (inSafeZone) {
          showNotification("SAFE! Recover here. " + String(playerRssi.size()) + " nearby", "success");
        } else if (playerRssi.size() > 0) {
          int closest = -999;
          for (auto& p : playerRssi) {
            if (p.second > closest) closest = p.second;
          }
          if (closest > -60) {
            showNotification("DANGER! Predator very close!", "danger");
          } else {
            showNotification("Threat nearby (" + String(closest) + "dB)", "warning");
          }
        } else {
          showNotification("Safe for now. Stay alert!", "info");
        }
      } else showNotification("Set role at " + String(SERVER_URL), "info");
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
  WiFi.begin(WIFI_SSID, WIFI_PASS);
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
  showNotification("Open browser to set your role", "info");
}

float getBatteryVoltage() {
  // Heltec V3 battery monitoring on ADC pin
  // Note: This is approximate, may need calibration
  int adcValue = analogRead(1);  // ADC1 for battery
  float voltage = (adcValue / 4095.0) * 3.3 * 2;  // Voltage divider
  return voltage;
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
