/*
 * LoRa Tracker for IRL Hunts v3 - Improved UX
 * Fixes: WiFi/Server status separation, clearer notifications, better button behavior
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

// =========================
// HELTEC V3 PINS
// =========================
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

// =========================
// CONFIGURATION - EDIT THESE!
// =========================
#define LORA_FREQ   915.0

const char* WIFI_SSID = "YOUR_WIFI_NAME";        // ← CHANGE THIS
const char* WIFI_PASS = "YOUR_WIFI_PASSWORD";    // ← CHANGE THIS
const char* SERVER_URL = "http://192.168.1.100:5000";  // ← CHANGE TO YOUR SERVER IP

const unsigned long PING_INTERVAL = 5000;
const unsigned long BEACON_INTERVAL = 3000;
const unsigned long DISPLAY_UPDATE_INTERVAL = 100;
const unsigned long SCROLL_SPEED = 150;
const unsigned long WIFI_RECONNECT_INTERVAL = 30000;
const unsigned long NOTIFICATION_DURATION = 3000;

// =========================
// OBJECTS
// =========================
SPIClass loraSPI(HSPI);
SX1262 radio = new Module(LORA_NSS, LORA_DIO1, LORA_RST, LORA_BUSY, loraSPI);
SSD1306Wire display(OLED_ADDR, OLED_SDA, OLED_SCL);
HTTPClient http;

// =========================
// STATE - Using clearer names
// =========================
String NODE_ID;
String myRole = "unassigned";      // Renamed from 'role' to avoid confusion
String myStatus = "lobby";
String gamePhase = "lobby";
bool inSafeZone = false;
String safeZoneBeacon = "";

// Connection status - SEPARATED
bool wifiConnected = false;
bool serverReachable = false;
unsigned long lastWifiCheck = 0;
unsigned long lastSuccessfulPing = 0;
int consecutivePingFails = 0;

// RSSI readings
std::map<String, int> playerRssi;
std::map<String, int> beaconRssi;
std::vector<String> knownBeacons;

// Notifications - Improved system
String currentNotification = "";
unsigned long notificationExpiry = 0;
String notificationType = "info";  // info, warning, danger, success

// Scrolling text
String scrollText = "";
int scrollOffset = 0;
unsigned long lastScrollUpdate = 0;

// Button
bool lastBtnState = HIGH;
unsigned long btnPressTime = 0;
bool btnHeld = false;

// Timing
unsigned long lastPing = 0;
unsigned long lastBeacon = 0;
unsigned long lastDisplayUpdate = 0;

// Stats
int txCount = 0;
int rxCount = 0;
int captureAttempts = 0;
int escapeCount = 0;

// LoRa
volatile bool rxFlag = false;

// =========================
// INTERRUPT
// =========================
void setRxFlag() {
  rxFlag = true;
}

// =========================
// NOTIFICATION SYSTEM - Improved
// =========================
void showNotification(const String& msg, const String& type = "info") {
  currentNotification = msg;
  notificationType = type;
  notificationExpiry = millis() + NOTIFICATION_DURATION;
  Serial.println("[NOTIF] " + type + ": " + msg);
}

void clearNotification() {
  currentNotification = "";
  notificationExpiry = 0;
}

bool hasActiveNotification() {
  return currentNotification.length() > 0 && millis() < notificationExpiry;
}

// =========================
// WIFI MANAGEMENT - Improved
// =========================
void checkWiFiConnection() {
  bool wasConnected = wifiConnected;
  wifiConnected = (WiFi.status() == WL_CONNECTED);
  
  if (!wifiConnected && wasConnected) {
    Serial.println("[WIFI] Connection lost");
    showNotification("WiFi lost - reconnecting...", "warning");
  } else if (wifiConnected && !wasConnected) {
    Serial.println("[WIFI] Reconnected: " + WiFi.localIP().toString());
    showNotification("WiFi reconnected!", "success");
  }
  
  // Auto-reconnect if disconnected for too long
  if (!wifiConnected && (millis() - lastWifiCheck > WIFI_RECONNECT_INTERVAL)) {
    lastWifiCheck = millis();
    Serial.println("[WIFI] Attempting reconnection...");
    WiFi.disconnect();
    delay(100);
    WiFi.begin(WIFI_SSID, WIFI_PASS);
  }
}

// =========================
// DISPLAY FUNCTIONS - Improved Layout
// =========================
void drawStatusBar() {
  // Top status bar showing connection states
  String statusLine = NODE_ID;
  
  // WiFi indicator
  if (wifiConnected) {
    statusLine += " [W]";
  } else {
    statusLine += " [W!]";
  }
  
  // Server indicator
  if (serverReachable) {
    statusLine += "[S]";
  } else {
    statusLine += "[S?]";
  }
  
  // Safe zone indicator
  if (inSafeZone) {
    statusLine += " SAFE";
  }
  
  display.drawString(0, 0, statusLine);
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

void updateScroll() {
  if (millis() - lastScrollUpdate > SCROLL_SPEED) {
    lastScrollUpdate = millis();
    scrollOffset += 1;
    if (scrollOffset > 1000) scrollOffset = 0;
  }
}

void displayNotificationScreen() {
  display.clear();
  display.setTextAlignment(TEXT_ALIGN_CENTER);
  
  // Draw notification type header
  display.setFont(ArialMT_Plain_16);
  if (notificationType == "danger") {
    display.drawString(64, 5, "! ALERT !");
  } else if (notificationType == "warning") {
    display.drawString(64, 5, "WARNING");
  } else if (notificationType == "success") {
    display.drawString(64, 5, "SUCCESS");
  } else {
    display.drawString(64, 5, "INFO");
  }
  
  // Draw message with word wrap
  display.setFont(ArialMT_Plain_10);
  String msg = currentNotification;
  
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

void displayMain() {
  // Show notification if active
  if (hasActiveNotification()) {
    displayNotificationScreen();
    return;
  }
  
  display.clear();
  display.setTextAlignment(TEXT_ALIGN_LEFT);
  display.setFont(ArialMT_Plain_10);
  
  // Special layout for unassigned role - focus on setup instructions
  if (myRole == "unassigned") {
    // Line 1: Device ID and connection status
    drawStatusBar();
    
    // Line 2: Clear instruction
    display.drawString(0, 14, "ROLE NOT SET");
    
    // Line 3: What to do
    display.drawString(0, 26, "Open phone browser:");
    
    // Line 4: Scrolling URL
    drawScrollingText(38, String(SERVER_URL));
    
    // Line 5: Final instruction
    display.drawString(0, 50, "Enter ID: " + NODE_ID);
    
    display.display();
    return;
  }
  
  // Normal display for assigned roles
  // Line 1: Status bar (ID + connection indicators)
  drawStatusBar();
  
  // Line 2: Role and game status
  String line2 = "";
  if (myRole == "pred") {
    line2 = "Role: PREDATOR";
  } else if (myRole == "prey") {
    line2 = "Role: PREY";
  } else {
    line2 = "Role: " + myRole;
  }
  line2 += " | " + myStatus;
  display.drawString(0, 12, line2);
  
  // Line 3: Game phase
  String line3 = "Game: " + gamePhase;
  if (gamePhase == "running") {
    line3 += " (ACTIVE)";
  }
  display.drawString(0, 24, line3);
  
  // Line 4: Nearby signals
  String line4 = "";
  int nearbyCount = playerRssi.size();
  if (nearbyCount == 0) {
    line4 = "Nearby: none detected";
  } else {
    int strongest = -999;
    String strongestId = "";
    for (auto& p : playerRssi) {
      if (p.second > strongest) {
        strongest = p.second;
        strongestId = p.first;
      }
    }
    line4 = String(nearbyCount) + " nearby | Closest: " + strongestId + " (" + String(strongest) + "dB)";
  }
  drawScrollingText(36, line4);
  
  // Line 5: Context-sensitive help
  String line5 = "";
  if (myRole == "pred") {
    if (nearbyCount > 0) {
      line5 = "BTN: Capture nearest | " + String(captureAttempts) + " tries";
    } else {
      line5 = "No targets in range";
    }
  } else if (myRole == "prey") {
    if (myStatus == "captured") {
      line5 = "CAPTURED! Find safe zone to escape";
    } else {
      line5 = "Escapes: " + String(escapeCount) + " | Stay hidden!";
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

// =========================
// LORA FUNCTIONS
// =========================
void sendBeacon() {
  // Only broadcast if we have a valid role
  String broadcastRole = myRole;
  if (broadcastRole == "unassigned") {
    broadcastRole = "unknown";  // Don't broadcast confusing text
  }
  
  String packet = NODE_ID + "|" + broadcastRole;
  radio.transmit(packet);
  radio.startReceive();
  txCount++;
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
      String type = packet.substring(sep + 1);
      
      if (id != NODE_ID) {
        if (type == "SAFEZONE" || type == "BEACON") {
          beaconRssi[id] = (int)rssi;
        } else if (type == "pred" || type == "prey" || type == "unknown") {
          playerRssi[id] = (int)rssi;
          rxCount++;
          
          // Proximity warning for prey
          if (myRole == "prey" && type == "pred") {
            if (rssi > -55) {
              showNotification("DANGER! Predator " + id + " very close!", "danger");
            } else if (rssi > -65) {
              showNotification("Predator approaching: " + id, "warning");
            }
          }
        }
        // Ignore other/malformed types
      }
    }
  }
  
  radio.startReceive();
}

// =========================
// SERVER COMMUNICATION - Improved
// =========================
void pingServer() {
  // Check WiFi first
  checkWiFiConnection();
  
  if (!wifiConnected) {
    serverReachable = false;
    return;
  }
  
  StaticJsonDocument<1024> doc;
  doc["device_id"] = NODE_ID;
  
  // Player RSSI
  JsonObject pRssi = doc.createNestedObject("player_rssi");
  for (auto& p : playerRssi) {
    pRssi[p.first] = p.second;
  }
  
  // Beacon RSSI
  JsonObject bRssi = doc.createNestedObject("beacon_rssi");
  for (auto& b : beaconRssi) {
    bRssi[b.first] = b.second;
  }
  
  String json;
  serializeJson(doc, json);
  
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
    DeserializationError err = deserializeJson(respDoc, resp);
    
    if (!err) {
      // Update state from server
      String newPhase = respDoc["phase"].as<String>();
      String newStatus = respDoc["status"].as<String>();
      String newRole = respDoc["role"].as<String>();
      bool newSafe = respDoc["in_safe_zone"];
      
      // Check for changes and notify
      if (gamePhase != newPhase) {
        gamePhase = newPhase;
        if (newPhase == "running") {
          showNotification("Game started!", "success");
        } else if (newPhase == "paused") {
          showNotification("Game paused", "warning");
        } else if (newPhase == "ended") {
          showNotification("Game ended!", "info");
        }
      }
      
      if (myStatus != newStatus) {
        String oldStatus = myStatus;
        myStatus = newStatus;
        if (newStatus == "captured") {
          showNotification("YOU WERE CAPTURED!", "danger");
        } else if (oldStatus == "captured" && newStatus == "active") {
          escapeCount++;
          showNotification("YOU ESCAPED!", "success");
        }
      }
      
      if (myRole != newRole && newRole != "unassigned" && newRole.length() > 0) {
        myRole = newRole;
        showNotification("Role set: " + myRole, "success");
      }
      
      if (inSafeZone != newSafe) {
        inSafeZone = newSafe;
        if (newSafe) {
          showNotification("Entered SAFE ZONE", "success");
        } else {
          showNotification("Left safe zone - vulnerable!", "warning");
        }
      }
      
      // Process server notifications
      if (respDoc.containsKey("notifications")) {
        JsonArray notifs = respDoc["notifications"];
        for (JsonVariant n : notifs) {
          String msg = n["message"].as<String>();
          String type = n["type"].as<String>();
          if (type.length() == 0) type = "info";
          showNotification(msg, type);
        }
      }
      
      // Update known beacons
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
    Serial.println("[SERVER] Ping failed: " + String(code) + " (fail #" + String(consecutivePingFails) + ")");
    
    // Only mark as unreachable after multiple fails
    if (consecutivePingFails >= 3) {
      if (serverReachable) {
        serverReachable = false;
        showNotification("Server connection lost", "warning");
      }
    }
  }
  
  http.end();
  
  // Clear readings for next cycle
  playerRssi.clear();
  // Keep beacon readings slightly longer
}

void attemptCapture() {
  if (myRole != "pred") {
    showNotification("Only predators can capture", "warning");
    return;
  }
  
  if (gamePhase != "running") {
    showNotification("Game not active", "warning");
    return;
  }
  
  if (playerRssi.empty()) {
    showNotification("No targets in range", "info");
    return;
  }
  
  // Find closest target
  int bestRssi = -999;
  String targetId = "";
  for (auto& p : playerRssi) {
    if (p.second > bestRssi) {
      bestRssi = p.second;
      targetId = p.first;
    }
  }
  
  captureAttempts++;
  showNotification("Capturing " + targetId + "...", "info");
  
  // Send to server
  StaticJsonDocument<256> doc;
  doc["pred_id"] = NODE_ID;
  doc["prey_id"] = targetId;
  doc["rssi"] = bestRssi;
  
  String json;
  serializeJson(doc, json);
  
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
      showNotification("CAPTURE SUCCESS!", "success");
      displayBigText("CAPTURED!", targetId);
      delay(2000);
    } else {
      showNotification("Failed: " + msg, "warning");
    }
  } else {
    showNotification("Server error", "danger");
  }
  
  http.end();
}

// =========================
// BUTTON HANDLING - Improved
// =========================
void handleButton() {
  bool btnState = digitalRead(BUTTON_PIN);
  
  if (btnState == LOW && lastBtnState == HIGH) {
    // Button just pressed
    btnPressTime = millis();
    btnHeld = false;
    
    // Immediate action based on role
    if (myRole == "pred") {
      attemptCapture();
    } else if (myRole == "prey") {
      // Show helpful info instead of confusing message
      if (myStatus == "captured") {
        showNotification("Find safe zone beacon to escape!", "info");
      } else if (inSafeZone) {
        showNotification("You're safe here", "success");
      } else {
        showNotification("Stay alert! Find cover or safe zone", "info");
      }
    } else {
      // Role not set - give clear instruction
      showNotification("Open " + String(SERVER_URL) + " to set role", "info");
    }
  }
  
  // Long press detection (3+ seconds)
  if (btnState == LOW && !btnHeld && (millis() - btnPressTime) > 3000) {
    btnHeld = true;
    showNotification("EMERGENCY - Hold to confirm", "danger");
    // Could implement emergency beacon here
  }
  
  lastBtnState = btnState;
}

// =========================
// SETUP
// =========================
void setup() {
  Serial.begin(115200);
  delay(500);
  
  Serial.println("\n========================================");
  Serial.println("IRL Hunts Tracker v3 - Improved UX");
  Serial.println("========================================");
  
  // Pins
  pinMode(BUTTON_PIN, INPUT_PULLUP);
  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, HIGH);
  pinMode(VEXT_PIN, OUTPUT);
  digitalWrite(VEXT_PIN, LOW);
  delay(100);
  
  // OLED
  pinMode(OLED_RST, OUTPUT);
  digitalWrite(OLED_RST, LOW);
  delay(50);
  digitalWrite(OLED_RST, HIGH);
  delay(100);
  
  Wire.begin(OLED_SDA, OLED_SCL);
  display.init();
  display.flipScreenVertically();
  display.setFont(ArialMT_Plain_10);
  
  displayStartup("Initializing...");
  
  // Generate unique ID
  uint64_t mac = ESP.getEfuseMac();
  uint16_t id1 = (uint16_t)(mac & 0xFFFF);
  uint16_t id2 = (uint16_t)((mac >> 32) & 0xFFFF);
  uint16_t uniqueId = id1 ^ id2 ^ (uint16_t)(mac >> 16);
  
  char buf[8];
  snprintf(buf, sizeof(buf), "T%04X", uniqueId);
  NODE_ID = String(buf);
  
  Serial.println("Device ID: " + NODE_ID);
  displayBigText(NODE_ID, "Your Tracker ID");
  delay(2000);
  
  // LoRa
  displayStartup("Starting LoRa radio...");
  pinMode(LORA_NSS, OUTPUT);
  digitalWrite(LORA_NSS, HIGH);
  pinMode(LORA_RST, OUTPUT);
  digitalWrite(LORA_RST, HIGH);
  
  loraSPI.begin(LORA_SCK, LORA_MISO, LORA_MOSI, LORA_NSS);
  
  int state = radio.begin(LORA_FREQ);
  if (state != RADIOLIB_ERR_NONE) {
    displayBigText("LoRa FAILED!", "Error: " + String(state));
    Serial.println("LoRa init failed: " + String(state));
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
  
  // WiFi
  displayStartup("Connecting to WiFi...");
  WiFi.mode(WIFI_STA);
  WiFi.setHostname(NODE_ID.c_str());
  WiFi.begin(WIFI_SSID, WIFI_PASS);
  
  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < 20) {
    delay(500);
    attempts++;
    displayStartup("WiFi... " + String(attempts) + "/20");
  }
  
  if (WiFi.status() == WL_CONNECTED) {
    wifiConnected = true;
    Serial.println("WiFi OK: " + WiFi.localIP().toString());
    displayBigText("WiFi Connected!", WiFi.localIP().toString());
  } else {
    wifiConnected = false;
    Serial.println("WiFi connection failed");
    displayBigText("WiFi Failed", "Will retry automatically");
  }
  delay(1500);
  
  // Final ready screen
  displayBigText("Ready!", "ID: " + NODE_ID);
  Serial.println("========================================");
  Serial.println("Ready! Server: " + String(SERVER_URL));
  Serial.println("========================================\n");
  delay(2000);
  
  showNotification("Open browser to set your role", "info");
}

// =========================
// LOOP
// =========================
void loop() {
  unsigned long now = millis();
  
  // Receive LoRa packets
  receivePackets();
  
  // Handle button presses
  handleButton();
  
  // Send LoRa beacon
  if (now - lastBeacon > BEACON_INTERVAL) {
    lastBeacon = now;
    sendBeacon();
  }
  
  // Ping server
  if (now - lastPing > PING_INTERVAL) {
    lastPing = now;
    pingServer();
  }
  
  // Update display
  if (now - lastDisplayUpdate > DISPLAY_UPDATE_INTERVAL) {
    lastDisplayUpdate = now;
    updateScroll();
    displayMain();
  }
  
  // LED blink pattern based on status
  static unsigned long lastBlink = 0;
  int blinkInterval = 1000;  // Default: slow blink
  
  if (!wifiConnected) {
    blinkInterval = 100;  // Fast blink: no WiFi
  } else if (!serverReachable) {
    blinkInterval = 300;  // Medium blink: no server
  } else if (myStatus == "captured") {
    blinkInterval = 200;  // Quick blink: captured
  } else if (inSafeZone) {
    blinkInterval = 2000;  // Very slow: safe
  }
  
  if (now - lastBlink > blinkInterval) {
    lastBlink = now;
    digitalWrite(LED_PIN, !digitalRead(LED_PIN));
  }
  
  delay(10);
}
