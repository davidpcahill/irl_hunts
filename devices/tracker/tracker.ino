/*
 * LoRa Tracker for IRL Hunts v2
 * Features: Auto-registration, beacon detection, scrolling text, notifications
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
const unsigned long SCROLL_SPEED = 150;  // ms per pixel scroll

// =========================
// OBJECTS
// =========================
SPIClass loraSPI(HSPI);
SX1262 radio = new Module(LORA_NSS, LORA_DIO1, LORA_RST, LORA_BUSY, loraSPI);
SSD1306Wire display(OLED_ADDR, OLED_SDA, OLED_SCL);
HTTPClient http;

// =========================
// STATE
// =========================
String NODE_ID;
String role = "unassigned";
String status = "lobby";
String gamePhase = "lobby";
bool inSafeZone = false;
String safeZoneBeacon = "";

// RSSI readings
std::map<String, int> playerRssi;
std::map<String, int> beaconRssi;
std::vector<String> knownBeacons;

// Notifications queue
std::vector<String> notifications;
unsigned long notifDisplayUntil = 0;
int currentNotifIndex = 0;

// Scrolling text
String scrollText = "";
int scrollOffset = 0;
unsigned long lastScrollUpdate = 0;
bool needsScroll = false;

// Button
bool lastBtnState = HIGH;
unsigned long btnPressTime = 0;
int btnPressCount = 0;

// Timing
unsigned long lastPing = 0;
unsigned long lastBeacon = 0;
unsigned long lastDisplayUpdate = 0;

// Stats
int txCount = 0;
int rxCount = 0;
int captureAttempts = 0;

// LoRa
volatile bool rxFlag = false;

// =========================
// INTERRUPT
// =========================
void setRxFlag() {
  rxFlag = true;
}

// =========================
// SCROLLING TEXT DISPLAY
// =========================
void drawScrollingText(int y, const String& text, int maxWidth = 128) {
  int textWidth = display.getStringWidth(text);
  
  if (textWidth <= maxWidth) {
    // No scroll needed
    display.drawString(0, y, text);
  } else {
    // Scroll the text
    String displayText = text + "   " + text;  // Repeat for seamless scroll
    int totalWidth = textWidth + display.getStringWidth("   ");
    int offset = scrollOffset % totalWidth;
    
    // Draw clipped
    display.setColor(BLACK);
    display.fillRect(0, y, maxWidth, 12);
    display.setColor(WHITE);
    
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

// =========================
// DISPLAY FUNCTIONS
// =========================
void showNotification(const String& msg) {
  notifications.push_back(msg);
  if (notifications.size() > 10) {
    notifications.erase(notifications.begin());
  }
  notifDisplayUntil = millis() + 4000;  // Show for 4 seconds
}

void displayNotification() {
  if (notifications.empty() || millis() > notifDisplayUntil) {
    return;
  }
  
  display.clear();
  display.setTextAlignment(TEXT_ALIGN_CENTER);
  display.setFont(ArialMT_Plain_16);
  display.drawString(64, 5, "ALERT!");
  display.setFont(ArialMT_Plain_10);
  
  String msg = notifications.back();
  
  // Word wrap long messages
  if (msg.length() > 21) {
    int split = msg.lastIndexOf(' ', 21);
    if (split == -1) split = 21;
    display.drawString(64, 25, msg.substring(0, split));
    display.drawString(64, 38, msg.substring(split + 1));
  } else {
    display.drawString(64, 30, msg);
  }
  
  display.display();
}

void displayMain() {
  if (millis() < notifDisplayUntil && !notifications.empty()) {
    displayNotification();
    return;
  }
  
  display.clear();
  display.setTextAlignment(TEXT_ALIGN_LEFT);
  display.setFont(ArialMT_Plain_10);
  
  // Line 1: ID and Role
  String line1 = NODE_ID + " | " + role;
  if (role == "unassigned") line1 = NODE_ID + " | SET ROLE";
  display.drawString(0, 0, line1);
  
  // Line 2: Status with icon
  String statusIcon = "";
  if (status == "captured") statusIcon = "X ";
  else if (status == "active") statusIcon = "> ";
  else if (inSafeZone) statusIcon = "# ";
  
  String line2 = statusIcon + status;
  if (inSafeZone) line2 += " [SAFE]";
  display.drawString(0, 12, line2);
  
  // Line 3: Game phase and time
  String line3 = "Game: " + gamePhase;
  display.drawString(0, 24, line3);
  
  // Line 4: Nearby signals (scrolling if needed)
  String line4 = "";
  int nearbyCount = playerRssi.size();
  if (nearbyCount == 0) {
    line4 = "No signals";
  } else {
    // Find strongest
    int strongest = -999;
    String strongestId = "";
    for (auto& p : playerRssi) {
      if (p.second > strongest) {
        strongest = p.second;
        strongestId = p.first;
      }
    }
    line4 = String(nearbyCount) + " nearby | Best: " + strongestId + " " + String(strongest) + "dB";
  }
  drawScrollingText(36, line4);
  
  // Line 5: Instructions based on role
  String line5 = "";
  if (role == "pred") {
    line5 = "BTN=Capture | " + String(captureAttempts) + " attempts";
  } else if (role == "prey") {
    line5 = "BTN=Toggle Safe | Escapes: ?";
  } else {
    line5 = "Visit " + String(SERVER_URL);
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
  // Format: ID|TYPE (pred, prey, SAFEZONE)
  String packet = NODE_ID + "|" + role;
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
        } else {
          playerRssi[id] = (int)rssi;
          rxCount++;
          
          // Proximity warning
          if (role == "prey" && type == "pred" && rssi > -55) {
            showNotification("DANGER! Pred " + id + " very close!");
          } else if (role == "prey" && type == "pred" && rssi > -65) {
            showNotification("WARNING! Pred nearby: " + id);
          }
        }
      }
    }
  }
  
  radio.startReceive();
}

// =========================
// SERVER COMMUNICATION
// =========================
void pingServer() {
  if (WiFi.status() != WL_CONNECTED) {
    showNotification("WiFi disconnected!");
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
    String resp = http.getString();
    StaticJsonDocument<1024> respDoc;
    DeserializationError err = deserializeJson(respDoc, resp);
    
    if (!err) {
      // Update state from server
      String newPhase = respDoc["phase"].as<String>();
      String newStatus = respDoc["status"].as<String>();
      String newRole = respDoc["role"].as<String>();
      bool newSafe = respDoc["in_safe_zone"];
      
      // Check for changes
      if (gamePhase != newPhase) {
        gamePhase = newPhase;
        showNotification("Game: " + gamePhase);
      }
      
      if (status != newStatus) {
        String oldStatus = status;
        status = newStatus;
        if (newStatus == "captured") {
          showNotification("YOU WERE CAPTURED!");
        } else if (oldStatus == "captured" && newStatus == "active") {
          showNotification("YOU ESCAPED!");
        }
      }
      
      if (role != newRole && newRole != "unassigned") {
        role = newRole;
        showNotification("Role: " + role);
      }
      
      if (inSafeZone != newSafe) {
        inSafeZone = newSafe;
        if (newSafe) {
          showNotification("Entered SAFE ZONE");
        } else {
          showNotification("Left safe zone!");
        }
      }
      
      // Process notifications from server
      if (respDoc.containsKey("notifications")) {
        JsonArray notifs = respDoc["notifications"];
        for (JsonVariant n : notifs) {
          String msg = n["message"].as<String>();
          showNotification(msg);
        }
      }
      
      // Update known beacons
      if (respDoc.containsKey("beacon_ids")) {
        knownBeacons.clear();
        JsonArray beacons = respDoc["beacon_ids"];
        for (JsonVariant b : beacons) {
          knownBeacons.push_back(b.as<String>());
        }
      }
    }
  } else {
    Serial.println("Ping failed: " + String(code));
  }
  
  http.end();
  
  // Clear readings for next cycle
  playerRssi.clear();
  // Keep beacon readings slightly longer
}

void attemptCapture() {
  if (role != "pred") {
    showNotification("Only preds can capture!");
    return;
  }
  
  if (gamePhase != "running") {
    showNotification("Game not running!");
    return;
  }
  
  if (playerRssi.empty()) {
    showNotification("No targets nearby!");
    return;
  }
  
  // Find closest
  int bestRssi = -999;
  String targetId = "";
  for (auto& p : playerRssi) {
    if (p.second > bestRssi) {
      bestRssi = p.second;
      targetId = p.first;
    }
  }
  
  captureAttempts++;
  showNotification("Attempting capture: " + targetId);
  
  // Send to server
  StaticJsonDocument<256> doc;
  doc["pred_id"] = NODE_ID;
  doc["prey_id"] = targetId;
  doc["rssi"] = bestRssi;
  
  String json;
  serializeJson(doc, json);
  
  http.begin(String(SERVER_URL) + "/api/tracker/capture");
  http.addHeader("Content-Type", "application/json");
  
  int code = http.POST(json);
  
  if (code == 200) {
    String resp = http.getString();
    StaticJsonDocument<256> respDoc;
    deserializeJson(respDoc, resp);
    
    bool success = respDoc["success"];
    String msg = respDoc["message"].as<String>();
    
    if (success) {
      showNotification("CAPTURE SUCCESS!");
      displayBigText("CAPTURED!", targetId);
      delay(2000);
    } else {
      showNotification("Failed: " + msg);
    }
  } else {
    showNotification("Server error: " + String(code));
  }
  
  http.end();
}

// =========================
// BUTTON HANDLING
// =========================
void handleButton() {
  bool btnState = digitalRead(BUTTON_PIN);
  
  if (btnState == LOW && lastBtnState == HIGH) {
    // Button pressed
    btnPressTime = millis();
    btnPressCount++;
    
    // Single press action
    if (role == "pred") {
      attemptCapture();
    } else if (role == "prey") {
      // Toggle manual safe zone (if honor system enabled)
      showNotification("Use website for safe zone");
    } else {
      showNotification("Set your role on website!");
    }
  }
  
  // Long press (3+ seconds) = emergency
  if (btnState == LOW && (millis() - btnPressTime) > 3000) {
    showNotification("EMERGENCY MODE!");
    // Could send emergency signal here
    btnPressTime = millis();  // Reset to avoid repeat
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
  Serial.println("IRL Hunts Tracker v2");
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
  
  // Generate ID
  uint64_t mac = ESP.getEfuseMac();
  uint16_t id1 = (uint16_t)(mac & 0xFFFF);
  uint16_t id2 = (uint16_t)((mac >> 32) & 0xFFFF);
  uint16_t uniqueId = id1 ^ id2 ^ (uint16_t)(mac >> 16);
  
  char buf[8];
  snprintf(buf, sizeof(buf), "T%04X", uniqueId);
  NODE_ID = String(buf);
  
  Serial.println("ID: " + NODE_ID);
  displayBigText(NODE_ID, "Your Tracker ID");
  delay(2000);
  
  // LoRa
  displayStartup("Starting LoRa...");
  pinMode(LORA_NSS, OUTPUT);
  digitalWrite(LORA_NSS, HIGH);
  pinMode(LORA_RST, OUTPUT);
  digitalWrite(LORA_RST, HIGH);
  
  loraSPI.begin(LORA_SCK, LORA_MISO, LORA_MOSI, LORA_NSS);
  
  int state = radio.begin(LORA_FREQ);
  if (state != RADIOLIB_ERR_NONE) {
    displayBigText("LoRa FAILED!", "Error: " + String(state));
    Serial.println("LoRa failed: " + String(state));
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
  displayStartup("Connecting WiFi...");
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
    Serial.println("WiFi OK: " + WiFi.localIP().toString());
    displayBigText("WiFi Connected", WiFi.localIP().toString());
  } else {
    Serial.println("WiFi FAILED");
    displayBigText("WiFi Failed!", "Check credentials");
  }
  delay(1500);
  
  // Final
  displayBigText("Ready!", "ID: " + NODE_ID);
  Serial.println("========================================");
  Serial.println("Ready! Server: " + String(SERVER_URL));
  Serial.println("========================================\n");
  delay(2000);
  
  showNotification("Go to " + String(SERVER_URL));
}

// =========================
// LOOP
// =========================
void loop() {
  unsigned long now = millis();
  
  // Receive LoRa
  receivePackets();
  
  // Button
  handleButton();
  
  // Send beacon
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
  
  // Blink LED based on status
  static unsigned long lastBlink = 0;
  if (now - lastBlink > (status == "captured" ? 200 : 1000)) {
    lastBlink = now;
    digitalWrite(LED_PIN, !digitalRead(LED_PIN));
  }
  
  delay(10);
}
