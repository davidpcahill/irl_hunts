/*
 * Safe Zone Beacon v3 - Enhanced Edition
 * 
 * FEATURES:
 * - Consistent config loading (matches tracker)
 * - Battery monitoring with low-power warnings
 * - Firmware version tracking
 * - Enhanced health reporting
 * - Improved error handling
 * - Status LED patterns
 * 
 * LIBRARIES NEEDED:
 * 1. "ESP8266 and ESP32 OLED driver for SSD1306 displays" by ThingPulse
 * 2. "RadioLib" by Jan Gromes
 */

#include <SPI.h>
#include <RadioLib.h>
#include <Wire.h>
#include "SSD1306Wire.h"

// Firmware version
#define BEACON_VERSION "3.0"
#define BUILD_DATE __DATE__

// Load configuration - same pattern as tracker
#if __has_include("config_local.h")
  #include "config_local.h"
  #define CONFIG_SOURCE "config_local.h"
#else
  #warning "No config_local.h found, using defaults from config.h"
  #if __has_include("config.h")
    #include "config.h"
    #define CONFIG_SOURCE "config.h"
  #else
    #warning "No config files found, using hardcoded defaults"
    #define CONFIG_SOURCE "defaults"
  #endif
#endif

// Hardware pin defaults (Heltec V3)
#ifndef PIN_LORA_NSS
  #define PIN_LORA_NSS 8
#endif
#ifndef PIN_LORA_DIO1
  #define PIN_LORA_DIO1 14
#endif
#ifndef PIN_LORA_RST
  #define PIN_LORA_RST 12
#endif
#ifndef PIN_LORA_BUSY
  #define PIN_LORA_BUSY 13
#endif
#ifndef PIN_LORA_MOSI
  #define PIN_LORA_MOSI 10
#endif
#ifndef PIN_LORA_MISO
  #define PIN_LORA_MISO 11
#endif
#ifndef PIN_LORA_SCK
  #define PIN_LORA_SCK 9
#endif
#ifndef PIN_OLED_SDA
  #define PIN_OLED_SDA 17
#endif
#ifndef PIN_OLED_SCL
  #define PIN_OLED_SCL 18
#endif
#ifndef PIN_OLED_RST
  #define PIN_OLED_RST 21
#endif
#ifndef OLED_I2C_ADDR
  #define OLED_I2C_ADDR 0x3C
#endif
#ifndef PIN_VEXT
  #define PIN_VEXT 36
#endif
#ifndef PIN_LED
  #define PIN_LED 35
#endif
#ifndef PIN_BATTERY_ADC
  #define PIN_BATTERY_ADC 1
#endif

// LoRa defaults - MUST match trackers!
#ifndef LORA_FREQUENCY
  #define LORA_FREQUENCY 915.0  // Americas
#endif
#ifndef LORA_SF
  #define LORA_SF 7
#endif
#ifndef LORA_BW
  #define LORA_BW 125.0
#endif
#ifndef LORA_CR
  #define LORA_CR 5
#endif
#ifndef LORA_SYNC_WORD
  #define LORA_SYNC_WORD 0x34
#endif
#ifndef LORA_TX_POWER
  #define LORA_TX_POWER 14
#endif

// Beacon timing defaults
#ifndef BEACON_BROADCAST_INTERVAL
  #define BEACON_BROADCAST_INTERVAL 2000
#endif

// Battery monitoring
#ifndef BATTERY_DIVIDER_RATIO
  #define BATTERY_DIVIDER_RATIO 4.9
#endif
#ifndef BATTERY_LOW_VOLTAGE
  #define BATTERY_LOW_VOLTAGE 3.3
#endif
#ifndef BATTERY_CRITICAL_VOLTAGE
  #define BATTERY_CRITICAL_VOLTAGE 3.0
#endif

// Hardware objects
SPIClass loraSPI(HSPI);
SX1262 radio = new Module(PIN_LORA_NSS, PIN_LORA_DIO1, PIN_LORA_RST, PIN_LORA_BUSY, loraSPI);
SSD1306Wire display(OLED_I2C_ADDR, PIN_OLED_SDA, PIN_OLED_SCL);

// State variables
String BEACON_ID;
unsigned long lastBeacon = 0;
unsigned long lastHealthCheck = 0;
unsigned long lastBatteryCheck = 0;
unsigned long startTime = 0;
int txCount = 0;
int errorCount = 0;
float lastBatteryVoltage = 0.0;
bool lowBatteryWarning = false;
bool criticalBattery = false;

// LED pattern state
unsigned long lastLedUpdate = 0;
int ledState = 0;
bool ledOn = false;

void setup() {
  Serial.begin(115200);
  delay(500);
  
  startTime = millis();
  
  Serial.println("\n========================================");
  Serial.println("Safe Zone Beacon v" BEACON_VERSION);
  Serial.println("Build: " BUILD_DATE);
  Serial.println("Config: " CONFIG_SOURCE);
  Serial.println("========================================");
  
  // Initialize pins
  pinMode(PIN_LED, OUTPUT);
  digitalWrite(PIN_LED, HIGH);
  pinMode(PIN_VEXT, OUTPUT);
  digitalWrite(PIN_VEXT, LOW);  // Enable external power
  delay(100);
  
  // OLED reset sequence
  pinMode(PIN_OLED_RST, OUTPUT);
  digitalWrite(PIN_OLED_RST, LOW);
  delay(50);
  digitalWrite(PIN_OLED_RST, HIGH);
  delay(100);
  
  // Initialize display
  Wire.begin(PIN_OLED_SDA, PIN_OLED_SCL);
  display.init();
  display.flipScreenVertically();
  
  // Show startup screen
  display.clear();
  display.setTextAlignment(TEXT_ALIGN_CENTER);
  display.setFont(ArialMT_Plain_16);
  display.drawString(64, 5, "SAFE ZONE");
  display.setFont(ArialMT_Plain_10);
  display.drawString(64, 25, "BEACON");
  display.drawString(64, 40, "v" BEACON_VERSION);
  display.drawString(64, 52, "Initializing...");
  display.display();
  delay(1500);
  
  // Generate unique beacon ID from MAC address
  uint64_t mac = ESP.getEfuseMac();
  uint16_t id1 = (uint16_t)(mac & 0xFFFF);
  uint16_t id2 = (uint16_t)((mac >> 32) & 0xFFFF);
  uint16_t uniqueId = id1 ^ id2;
  
  char buf[8];
  snprintf(buf, sizeof(buf), "SZ%04X", uniqueId);
  BEACON_ID = String(buf);
  
  Serial.println("Beacon ID: " + BEACON_ID);
  
  // Show beacon ID prominently
  display.clear();
  display.setFont(ArialMT_Plain_10);
  display.drawString(64, 0, "YOUR BEACON ID:");
  display.setFont(ArialMT_Plain_24);
  display.drawString(64, 18, BEACON_ID);
  display.setFont(ArialMT_Plain_10);
  display.drawString(64, 48, "Register this in Admin!");
  display.display();
  delay(4000);
  
  // Initialize LoRa
  display.clear();
  display.setFont(ArialMT_Plain_10);
  display.drawString(64, 25, "Starting LoRa radio...");
  display.display();
  
  Serial.print("Initializing LoRa... ");
  
  pinMode(PIN_LORA_NSS, OUTPUT);
  digitalWrite(PIN_LORA_NSS, HIGH);
  pinMode(PIN_LORA_RST, OUTPUT);
  digitalWrite(PIN_LORA_RST, HIGH);
  
  loraSPI.begin(PIN_LORA_SCK, PIN_LORA_MISO, PIN_LORA_MOSI, PIN_LORA_NSS);
  
  int state = radio.begin(LORA_FREQUENCY);
  if (state != RADIOLIB_ERR_NONE) {
    Serial.println("FAILED!");
    Serial.println("Error code: " + String(state));
    
    display.clear();
    display.setFont(ArialMT_Plain_16);
    display.drawString(64, 10, "LoRa FAILED!");
    display.setFont(ArialMT_Plain_10);
    display.drawString(64, 35, "Error: " + String(state));
    display.drawString(64, 50, "Check antenna!");
    display.display();
    
    // Error LED pattern - rapid blink
    while (1) {
      digitalWrite(PIN_LED, !digitalRead(PIN_LED));
      delay(200);
    }
  }
  
  Serial.println("OK!");
  
  // Configure LoRa - MUST match tracker settings!
  radio.setSpreadingFactor(LORA_SF);
  radio.setBandwidth(LORA_BW);
  radio.setCodingRate(LORA_CR);
  radio.setSyncWord(LORA_SYNC_WORD);
  radio.setOutputPower(LORA_TX_POWER);
  
  Serial.println("========================================");
  Serial.println("Configuration:");
  Serial.println("  Beacon ID: " + BEACON_ID);
  Serial.println("  Frequency: " + String(LORA_FREQUENCY) + " MHz");
  Serial.println("  SF: " + String(LORA_SF) + " | BW: " + String(LORA_BW) + " kHz");
  Serial.println("  TX Power: " + String(LORA_TX_POWER) + " dBm");
  Serial.println("  Interval: " + String(BEACON_BROADCAST_INTERVAL) + " ms");
  Serial.println("========================================");
  Serial.println("\n⚠️  IMPORTANT:");
  Serial.println("1. Go to Admin Panel");
  Serial.println("2. Click 'Add Safe Zone'");
  Serial.println("3. Enter ID: " + BEACON_ID);
  Serial.println("4. Set name and RSSI threshold\n");
  
  // Check initial battery
  checkBattery();
  
  // Ready!
  display.clear();
  display.setFont(ArialMT_Plain_10);
  display.drawString(64, 0, "BEACON READY");
  display.setFont(ArialMT_Plain_16);
  display.drawString(64, 15, BEACON_ID);
  display.setFont(ArialMT_Plain_10);
  display.drawString(64, 38, String(LORA_FREQUENCY, 1) + " MHz");
  display.drawString(64, 52, "Broadcasting...");
  display.display();
  delay(2000);
}

float getBatteryVoltage() {
  // Configure ADC
  analogSetPinAttenuation(PIN_BATTERY_ADC, ADC_11db);
  
  // Take multiple readings for stability
  int sum = 0;
  for (int i = 0; i < 10; i++) {
    sum += analogRead(PIN_BATTERY_ADC);
    delay(1);
  }
  int adcValue = sum / 10;
  
  // Convert to voltage
  float adcVoltage = (adcValue / 4095.0) * 3.3;
  float batteryVoltage = adcVoltage * BATTERY_DIVIDER_RATIO;
  
  // Sanity check
  if (batteryVoltage < 2.5 || batteryVoltage > 4.5) {
    return 0.0;  // Invalid reading (probably on USB power)
  }
  
  return batteryVoltage;
}

int getBatteryPercentage(float voltage) {
  if (voltage >= 4.2) return 100;
  if (voltage <= 3.0) return 0;
  // Linear approximation
  return (int)((voltage - 3.0) / (4.2 - 3.0) * 100);
}

void checkBattery() {
  float voltage = getBatteryVoltage();
  lastBatteryVoltage = voltage;
  
  if (voltage < 0.1) {
    // Probably on USB power
    lowBatteryWarning = false;
    criticalBattery = false;
    return;
  }
  
  if (voltage < BATTERY_CRITICAL_VOLTAGE) {
    criticalBattery = true;
    lowBatteryWarning = true;
    Serial.println("⚠️  CRITICAL BATTERY: " + String(voltage, 2) + "V");
  } else if (voltage < BATTERY_LOW_VOLTAGE) {
    lowBatteryWarning = true;
    criticalBattery = false;
    Serial.println("⚠️  Low battery: " + String(voltage, 2) + "V");
  } else {
    lowBatteryWarning = false;
    criticalBattery = false;
  }
}

void updateLED() {
  unsigned long now = millis();
  
  if (criticalBattery) {
    // Very fast blink for critical battery
    if (now - lastLedUpdate > 100) {
      lastLedUpdate = now;
      ledOn = !ledOn;
      digitalWrite(PIN_LED, ledOn ? HIGH : LOW);
    }
  } else if (lowBatteryWarning) {
    // Fast blink for low battery
    if (now - lastLedUpdate > 300) {
      lastLedUpdate = now;
      ledOn = !ledOn;
      digitalWrite(PIN_LED, ledOn ? HIGH : LOW);
    }
  } else {
    // Normal operation: double blink pattern
    if (now - lastLedUpdate > 200) {
      lastLedUpdate = now;
      ledState++;
      if (ledState > 5) ledState = 0;
      
      // Double blink: ON-OFF-ON-OFF-pause
      if (ledState == 0 || ledState == 2) {
        digitalWrite(PIN_LED, HIGH);
      } else {
        digitalWrite(PIN_LED, LOW);
      }
    }
  }
}

void updateDisplay() {
  display.clear();
  display.setTextAlignment(TEXT_ALIGN_CENTER);
  
  // Header
  display.setFont(ArialMT_Plain_10);
  if (criticalBattery) {
    display.drawString(64, 0, "!! LOW BATTERY !!");
  } else if (lowBatteryWarning) {
    display.drawString(64, 0, "! Battery Low !");
  } else {
    display.drawString(64, 0, "SAFE ZONE BEACON");
  }
  
  // Beacon ID
  display.setFont(ArialMT_Plain_16);
  display.drawString(64, 12, BEACON_ID);
  
  // Stats
  display.setFont(ArialMT_Plain_10);
  display.drawString(64, 32, "TX: " + String(txCount) + " | Err: " + String(errorCount));
  
  // Frequency
  display.drawString(64, 44, String(LORA_FREQUENCY, 1) + " MHz");
  
  // Battery or uptime
  if (lastBatteryVoltage > 0.1) {
    int pct = getBatteryPercentage(lastBatteryVoltage);
    display.drawString(64, 54, "Batt: " + String(lastBatteryVoltage, 2) + "V (" + String(pct) + "%)");
  } else {
    unsigned long uptimeMin = (millis() - startTime) / 60000;
    display.drawString(64, 54, "Uptime: " + String(uptimeMin) + " min");
  }
  
  display.display();
}

void printHealthReport() {
  unsigned long uptimeMs = millis() - startTime;
  unsigned long uptimeMin = uptimeMs / 60000;
  unsigned long uptimeHr = uptimeMin / 60;
  
  Serial.println("\n=== BEACON HEALTH REPORT ===");
  Serial.println("ID: " + BEACON_ID);
  Serial.println("Version: " BEACON_VERSION);
  Serial.println("Uptime: " + String(uptimeHr) + "h " + String(uptimeMin % 60) + "m");
  Serial.println("TX Count: " + String(txCount));
  Serial.println("Errors: " + String(errorCount));
  Serial.println("Free Heap: " + String(ESP.getFreeHeap()) + " bytes");
  Serial.println("Frequency: " + String(LORA_FREQUENCY) + " MHz");
  
  if (lastBatteryVoltage > 0.1) {
    Serial.println("Battery: " + String(lastBatteryVoltage, 2) + "V (" + 
                   String(getBatteryPercentage(lastBatteryVoltage)) + "%)");
    if (criticalBattery) Serial.println("  ⚠️  CRITICAL - Charge immediately!");
    else if (lowBatteryWarning) Serial.println("  ⚠️  Low - Charge soon");
  } else {
    Serial.println("Battery: USB powered or N/A");
  }
  
  Serial.println("============================\n");
}

void loop() {
  unsigned long now = millis();
  
  // Broadcast beacon signal
  if (now - lastBeacon >= BEACON_BROADCAST_INTERVAL) {
    lastBeacon = now;
    
    // Beacon packet format: ID|SAFEZONE|version
    String packet = BEACON_ID + "|SAFEZONE|" + BEACON_VERSION;
    
    int state = radio.transmit(packet);
    
    if (state == RADIOLIB_ERR_NONE) {
      txCount++;
      
      // Log every 10th transmission
      if (txCount % 10 == 1 || txCount <= 3) {
        Serial.println("TX #" + String(txCount) + ": " + packet);
      }
    } else {
      errorCount++;
      Serial.println("TX Error #" + String(errorCount) + ": Code " + String(state));
      
      // If too many errors, might need to reinitialize
      if (errorCount > 100 && errorCount % 50 == 0) {
        Serial.println("⚠️  High error count, consider restarting beacon");
      }
    }
  }
  
  // Update LED pattern
  updateLED();
  
  // Update display every 2 seconds
  static unsigned long lastDisplayUpdate = 0;
  if (now - lastDisplayUpdate > 2000) {
    lastDisplayUpdate = now;
    updateDisplay();
  }
  
  // Check battery every minute
  if (now - lastBatteryCheck > 60000) {
    lastBatteryCheck = now;
    checkBattery();
  }
  
  // Health report every 5 minutes
  if (now - lastHealthCheck > 300000) {
    lastHealthCheck = now;
    printHealthReport();
  }
  
  delay(10);  // Small delay to prevent tight loop
}
