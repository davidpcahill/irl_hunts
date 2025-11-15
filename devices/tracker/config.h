/*
 * IRL Hunts Device Configuration
 * 
 * INSTRUCTIONS:
 * 1. Copy this file to config_local.h
 * 2. Edit config_local.h with your settings
 * 3. The .ino file will include config_local.h if it exists
 * 
 * IMPORTANT: Add config_local.h to .gitignore!
 */

#ifndef CONFIG_H
#define CONFIG_H

// =============================================================================
// WIFI SETTINGS - EDIT THESE!
// =============================================================================

#define WIFI_SSID "YOUR_WIFI_NAME"
#define WIFI_PASS "YOUR_WIFI_PASSWORD"

// =============================================================================
// SERVER SETTINGS - EDIT THIS!
// =============================================================================

#define SERVER_URL "http://YOUR_SERVER_IP:5000"

// =============================================================================
// LORA SETTINGS
// =============================================================================

// Frequency (MHz) - MUST MATCH YOUR REGION!
// Americas: 915.0
// Europe: 868.0
// Asia: 433.0
#define LORA_FREQUENCY 915.0

// Spreading Factor (7-12, lower = faster)
#define LORA_SF 7

// Bandwidth (kHz)
#define LORA_BW 125.0

// Coding Rate (5-8)
#define LORA_CR 5

// Sync Word (must match all devices)
#define LORA_SYNC_WORD 0x34

// Transmit Power (dBm, 2-20)
#define LORA_TX_POWER 14

// =============================================================================
// TIMING SETTINGS (milliseconds)
// =============================================================================

// How often to broadcast presence
#define BEACON_INTERVAL 3000

// How often to ping server (tracker only)
#define PING_INTERVAL 5000

// How often to update display
#define DISPLAY_UPDATE_INTERVAL 100

// Text scroll speed
#define SCROLL_SPEED 150

// WiFi reconnection interval
#define WIFI_RECONNECT_INTERVAL 15000

// Notification display duration
#define NOTIFICATION_DURATION 3000

// Server timeout warning threshold
#define SERVER_TIMEOUT_THRESHOLD 45000

// =============================================================================
// EMERGENCY SETTINGS
// =============================================================================

// Hold time for emergency (milliseconds)
#define EMERGENCY_HOLD_TIME 2000

// Number of taps after hold
#define EMERGENCY_TAP_COUNT 3

// Time window for taps (milliseconds)
#define EMERGENCY_TAP_WINDOW 2000

// =============================================================================
// HARDWARE PINS (Heltec V3)
// =============================================================================

// LoRa module
#define PIN_LORA_NSS    8
#define PIN_LORA_DIO1   14
#define PIN_LORA_RST    12
#define PIN_LORA_BUSY   13
#define PIN_LORA_MOSI   10
#define PIN_LORA_MISO   11
#define PIN_LORA_SCK    9

// OLED display
#define PIN_OLED_SDA    17
#define PIN_OLED_SCL    18
#define PIN_OLED_RST    21
#define OLED_I2C_ADDR   0x3C

// General
#define PIN_VEXT        36
#define PIN_BUTTON      0
#define PIN_LED         35

// Battery ADC
#define PIN_BATTERY_ADC 1

// =============================================================================
// BATTERY MONITORING
// =============================================================================

// Voltage divider ratio (Heltec V3: 390K/100K = 4.9)
#define BATTERY_DIVIDER_RATIO 4.9

// Battery thresholds (voltage)
#define BATTERY_FULL 4.2
#define BATTERY_LOW 3.3
#define BATTERY_CRITICAL 3.0

// =============================================================================
// DEVICE IDENTIFICATION
// =============================================================================

// Set to 1 to use custom ID instead of MAC-based
#define USE_CUSTOM_ID 0

// Custom device ID (only if USE_CUSTOM_ID = 1)
#define CUSTOM_DEVICE_ID "T0001"

#endif // CONFIG_H
