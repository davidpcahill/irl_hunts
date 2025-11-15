/*
 * Safe Zone Beacon v2
 * Flash to a Heltec V3 and place at safe zone location
 * Broadcasts signal that player trackers detect
 * 
 * LIBRARIES: Same as tracker
 * 1. "ESP8266 and ESP32 OLED driver for SSD1306 displays" by ThingPulse
 * 2. "RadioLib" by Jan Gromes
 */

#include <SPI.h>
#include <RadioLib.h>
#include <Wire.h>
#include "SSD1306Wire.h"

// Load configuration
#if __has_include("config_local.h")
  #include "config_local.h"
  #define CONFIG_SOURCE "config_local.h"
#elif __has_include("config.h")
  #include "config.h"
  #define CONFIG_SOURCE "config.h"
#else
  #warning "No config file found, using defaults"
  #define CONFIG_SOURCE "defaults"
  #define LORA_FREQUENCY 915.0
#endif

// Heltec V3 Pins (from config or defaults)
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
#define LED_PIN     35

// Configuration - MUST MATCH TRACKERS!
#define LORA_FREQ   915.0
const unsigned long BEACON_INTERVAL = 2000;  // Broadcast every 2 sec

// Objects
SPIClass loraSPI(HSPI);
SX1262 radio = new Module(LORA_NSS, LORA_DIO1, LORA_RST, LORA_BUSY, loraSPI);
SSD1306Wire display(OLED_ADDR, OLED_SDA, OLED_SCL);

// State
String BEACON_ID;
unsigned long lastBeacon = 0;
int txCount = 0;

void setup() {
  Serial.begin(115200);
  delay(500);
  
  Serial.println("\n========================================");
  Serial.println("Safe Zone Beacon v2");
  Serial.println("Config: " CONFIG_SOURCE);
  Serial.println("========================================");
  
  // Pins
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
  
  display.clear();
  display.setTextAlignment(TEXT_ALIGN_CENTER);
  display.setFont(ArialMT_Plain_16);
  display.drawString(64, 10, "SAFE ZONE");
  display.setFont(ArialMT_Plain_10);
  display.drawString(64, 35, "BEACON");
  display.display();
  delay(1000);
  
  // Generate unique beacon ID
  uint64_t mac = ESP.getEfuseMac();
  uint16_t id1 = (uint16_t)(mac & 0xFFFF);
  uint16_t id2 = (uint16_t)((mac >> 32) & 0xFFFF);
  uint16_t uniqueId = id1 ^ id2;
  
  char buf[8];
  snprintf(buf, sizeof(buf), "SZ%04X", uniqueId);
  BEACON_ID = String(buf);
  
  Serial.println("Beacon ID: " + BEACON_ID);
  
  display.clear();
  display.setFont(ArialMT_Plain_10);
  display.drawString(64, 0, "SAFE ZONE BEACON");
  display.setFont(ArialMT_Plain_24);
  display.drawString(64, 20, BEACON_ID);
  display.setFont(ArialMT_Plain_10);
  display.drawString(64, 50, "Register this ID!");
  display.display();
  delay(3000);
  
  // LoRa init
  Serial.print("LoRa init... ");
  
  pinMode(LORA_NSS, OUTPUT);
  digitalWrite(LORA_NSS, HIGH);
  pinMode(LORA_RST, OUTPUT);
  digitalWrite(LORA_RST, HIGH);
  
  loraSPI.begin(LORA_SCK, LORA_MISO, LORA_MOSI, LORA_NSS);
  
  int state = radio.begin(LORA_FREQ);
  if (state != RADIOLIB_ERR_NONE) {
    Serial.println("FAILED: " + String(state));
    display.clear();
    display.drawString(64, 20, "LoRa FAILED!");
    display.drawString(64, 35, "Error: " + String(state));
    display.display();
    while (1) {
      digitalWrite(LED_PIN, !digitalRead(LED_PIN));
      delay(200);
    }
  }
  Serial.println("OK");
  
  // MUST match tracker settings!
  radio.setSpreadingFactor(7);
  radio.setBandwidth(125.0);
  radio.setCodingRate(5);
  radio.setSyncWord(0x34);
  radio.setOutputPower(14);
  
  Serial.println("========================================");
  Serial.println("Beacon: " + BEACON_ID);
  Serial.println("Freq: " + String(LORA_FREQ) + " MHz");
  Serial.println("Interval: " + String(BEACON_INTERVAL) + " ms");
  Serial.println("========================================");
  Serial.println("\nIMPORTANT:");
  Serial.println("1. Go to Admin Panel");
  Serial.println("2. Add Safe Zone Beacon");
  Serial.println("3. Enter ID: " + BEACON_ID);
  Serial.println("4. Set name and RSSI threshold\n");
}

void loop() {
  unsigned long now = millis();
  
  if (now - lastBeacon >= BEACON_INTERVAL) {
    lastBeacon = now;
    
    // Beacon packet: ID|SAFEZONE
    String packet = BEACON_ID + "|SAFEZONE";
    
    int state = radio.transmit(packet);
    
    if (state == RADIOLIB_ERR_NONE) {
      txCount++;
      digitalWrite(LED_PIN, !digitalRead(LED_PIN));
      
      Serial.println("TX #" + String(txCount) + ": " + packet);
    } else {
      Serial.println("TX Error: " + String(state));
    }
    
    // Update display
    display.clear();
    display.setTextAlignment(TEXT_ALIGN_CENTER);
    display.setFont(ArialMT_Plain_16);
    display.drawString(64, 0, "SAFE ZONE");
    display.setFont(ArialMT_Plain_10);
    display.drawString(64, 20, BEACON_ID);
    display.drawString(64, 35, "Broadcasts: " + String(txCount));
    display.drawString(64, 48, String(LORA_FREQ, 1) + " MHz");
    display.display();
  }
  
  // Health monitoring (every 5 minutes)
  static unsigned long lastHealth = 0;
  static int errorCount = 0;
  if (now - lastHealth > 300000) {
    lastHealth = now;
    Serial.println("=== BEACON HEALTH ===");
    Serial.println("ID: " + BEACON_ID);
    Serial.println("TX Count: " + String(txCount));
    Serial.println("Errors: " + String(errorCount));
    Serial.println("Free Heap: " + String(ESP.getFreeHeap()) + " bytes");
    Serial.println("Uptime: " + String(millis() / 60000) + " minutes");
    Serial.println("====================");
  }
  
  delay(10);
}
