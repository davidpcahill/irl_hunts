# 🔩 IRL Hunts - Hardware Guide

Everything you need to know about the hardware that powers IRL Hunts.

---

## 📟 Heltec WiFi LoRa 32 V3

The core device for both player trackers and safe zone beacons.

### Why This Device?

✅ **Built-in LoRa radio** - Long range wireless
✅ **WiFi capability** - Server communication
✅ **OLED display** - Visual feedback
✅ **ESP32 processor** - Powerful and fast
✅ **USB-C** - Easy charging/programming
✅ **Affordable** - ~$20-25 each
✅ **Arduino compatible** - Easy to program

### Specifications

| Feature | Details |
|---------|---------|
| Processor | ESP32-S3 |
| Flash | 8MB |
| RAM | 512KB |
| LoRa Chip | SX1262 |
| Frequency | 433/868/915 MHz |
| Range | 1km+ line-of-sight |
| Display | 0.96" OLED 128x64 |
| WiFi | 802.11 b/g/n |
| USB | USB-C |
| Battery | 3.7V LiPo supported |

### Physical Layout

```
    [OLED Display]
    128 x 64 pixels
    
    [PRG Button]  [RST Button]
    (User input)  (Reset device)
    
    [USB-C Port]
    Power & Programming
    
    [LoRa Antenna Connector]
    SMA or IPEX
```

---

## 🛒 Purchasing

### Where to Buy

**Official Store:**
- [Heltec Automation](https://heltec.org/)
- Direct from manufacturer

**Online Retailers:**
- Amazon
- AliExpress
- Banggood
- Mouser
- DigiKey

### What to Buy

**Per Player Tracker (required):**
- 1x Heltec WiFi LoRa 32 V3
- 1x LoRa antenna (usually included)
- 1x USB-C cable

**Per Safe Zone Beacon (required):**
- 1x Heltec WiFi LoRa 32 V3
- 1x LoRa antenna
- 1x USB battery pack (5V 1A+)

**Optional:**
- Protective cases
- Lanyards/clips
- Extra antennas
- Spare devices

### Recommended Quantities

| Group Size | Player Trackers | Safe Zone Beacons |
|------------|-----------------|-------------------|
| Small (5-8) | 8 | 2-3 |
| Medium (10-15) | 15 | 3-4 |
| Large (20+) | 25 | 5-6 |

**Always have 2-3 spare trackers** for failures or additions.

---

## 🔌 Power Options

### USB-C Power
- 5V input via USB-C
- Power from laptop/charger
- Good for development
- Limited mobility

### LiPo Battery
- 3.7V single cell
- JST PH 2.0 connector
- Typical: 1000-3000mAh
- **Recommended for gameplay**

### Battery Runtime

| Battery Size | Tracker Runtime | Beacon Runtime |
|--------------|-----------------|----------------|
| 1000mAh | ~4-6 hours | ~8-10 hours |
| 2000mAh | ~8-12 hours | ~16-20 hours |
| 3000mAh | ~12-18 hours | ~24+ hours |

*Varies based on screen brightness, transmit power, WiFi usage*

### USB Battery Packs
For safe zone beacons (stationary):
- Any 5V USB power bank
- 10000mAh+ recommended
- Check auto-shutoff (some turn off with low draw)

---

## 📡 LoRa Antenna

**Critical for performance!**

### Included Antenna
- Spring antenna (common)
- PCB antenna (some models)
- Basic performance

### Upgraded Options
- SMA rubber duck antenna
- Higher gain = longer range
- Tuned for your frequency

### Frequency Matching

| Region | Frequency | Antenna |
|--------|-----------|---------|
| Americas | 915 MHz | 915 MHz antenna |
| Europe | 868 MHz | 868 MHz antenna |
| Asia | 433 MHz | 433 MHz antenna |

**⚠️ Always match antenna frequency to radio setting!**

### Range Expectations

| Conditions | Expected Range |
|------------|----------------|
| Line of sight, open field | 1-2 km |
| Urban, some obstacles | 200-500m |
| Indoor, walls | 50-100m |
| Dense buildings | 20-50m |

---

## 🔧 Connections

### OLED Display
- I2C communication
- SDA: GPIO 17
- SCL: GPIO 18
- Reset: GPIO 21
- Address: 0x3C

### LoRa Module
- SPI communication
- NSS: GPIO 8
- DIO1: GPIO 14
- RST: GPIO 12
- BUSY: GPIO 13
- MOSI: GPIO 10
- MISO: GPIO 11
- SCK: GPIO 9

### User Button
- PRG button: GPIO 0
- Active LOW
- Internal pull-up

### LED
- White LED: GPIO 35
- Active HIGH

### Power Control
- VEXT: GPIO 36
- Controls external power
- Set LOW to enable

---

## 💻 Programming

### Required Software

1. **Arduino IDE 2.x**
   - [Download](https://arduino.cc)
   
2. **ESP32 Board Support**
   ```
   https://espressif.github.io/arduino-esp32/package_esp32_index.json
   ```

3. **Libraries:**
   - ESP8266 and ESP32 OLED driver (ThingPulse)
   - RadioLib (Jan Gromes)
   - ArduinoJson (Benoit Blanchon)

### Board Selection
Tools → Board → ESP32 Arduino → "Heltec WiFi LoRa 32(V3)"

### Upload Settings
- Upload Speed: 921600
- Flash Mode: QIO
- Flash Size: 8MB
- Partition: Default 4MB with spiffs

### First Upload

1. Connect device via USB-C
2. Select correct COM port
3. Press and hold PRG button
4. Click RST button (while holding PRG)
5. Release PRG button
6. Click Upload in IDE

*Most newer devices auto-enter boot mode*

---

## 🛡️ Protective Cases

### 3D Printed
- Custom fit
- Ventilation holes
- Button access
- Antenna port

### Commercial Options
- Generic project boxes
- Custom ESP32 cases
- Waterproof enclosures

### DIY Solutions
- Heat shrink tubing
- Electrical tape wrap
- Foam padding

### Mounting
- Lanyards (neck or wrist)
- Belt clips
- Arm bands
- Pocket carry

**Considerations:**
- Don't block antenna
- Allow button access
- Protect OLED screen
- Maintain ventilation

---

## 🔋 Battery Management

### Charging
- Onboard TP4054 charger
- Charges via USB-C
- Red LED = charging
- Green LED = full

### Battery Safety
⚠️ **LiPo batteries are sensitive!**
- Don't over-discharge (built-in protection)
- Don't puncture
- Store at room temperature
- Use matched voltage (3.7V)

### Low Battery Behavior
- Display dims
- WiFi may disconnect
- LoRa range decreases
- Eventually shuts down

**Monitor battery during game!**

---

## 📊 Performance Tuning

### LoRa Parameters

**Spreading Factor (SF)**
- SF7: Fastest, shortest range
- SF12: Slowest, longest range
- Game uses SF7 (speed matters)

**Bandwidth**
- 125 kHz: Standard
- 250 kHz: Faster, shorter range
- Game uses 125 kHz

**Coding Rate**
- 4/5: Least error correction
- 4/8: Most error correction
- Game uses 4/5

**Transmit Power**
- 2-20 dBm
- Higher = more range, more battery
- Game uses 14 dBm

### WiFi Settings
- 2.4 GHz only (no 5 GHz)
- DHCP for easy setup
- Hostname set to device ID

---

## 🔧 Troubleshooting

### Device Won't Power On
1. Check USB connection
2. Try different cable
3. Battery charged?
4. Press RST button

### Display Blank
1. VEXT power enabled?
2. OLED reset sequence?
3. I2C address correct (0x3C)?
4. Wiring (if external)

### LoRa Not Working
1. Antenna connected?
2. Frequency correct?
3. RadioLib initialized?
4. Check Serial Monitor

### WiFi Connection Fails
1. SSID/password correct?
2. 2.4 GHz network?
3. In range of router?
4. Too many devices on network?

### Upload Fails
1. Correct COM port?
2. Board selected properly?
3. Try boot mode (PRG+RST)
4. Driver installed?

---

## 🏗️ Scaling Up

### Many Devices
- Flash identical code
- Auto-unique IDs
- Same WiFi network
- Consider router capacity

### Large Play Area
- More safe zone beacons
- Consider WiFi range
- LoRa reaches far
- Server location matters

### Dense Environment
- Walls block LoRa
- More beacons needed
- Lower RSSI thresholds
- Test thoroughly

---

## 🔮 Future Hardware Ideas

### GPS Module
- Add location tracking
- Map overlay possible
- More complex captures

### Heart Rate Monitor
- Player intensity tracking
- Health monitoring
- Fairness metrics

### E-Paper Display
- Better battery life
- Sunlight readable
- Slower refresh

### NFC Tags
- Checkpoint system
- Item pickups
- Power-ups

### Sound Module
- Audio feedback
- Capture sounds
- Alerts

---

## 📦 Hardware Checklist

### Per Event

**Server:**
- [ ] Laptop with Python
- [ ] Power supply/charger
- [ ] WiFi access

**Player Trackers:**
- [ ] All devices charged
- [ ] Antennas attached
- [ ] Lanyards/cases ready
- [ ] Spares available

**Safe Zone Beacons:**
- [ ] Devices charged
- [ ] Power banks charged
- [ ] Antennas attached
- [ ] Placement planned

**Support:**
- [ ] USB cables
- [ ] Spare batteries
- [ ] Tools (screwdrivers)
- [ ] Tape/zip ties

---

## 💰 Budget Planning

### Minimum Viable Setup
- 5 trackers: $125
- 2 beacons: $50
- Cables/batteries: $50
- **Total: ~$225**

### Recommended Setup (10 players)
- 12 trackers: $300
- 4 beacons: $100
- Accessories: $100
- **Total: ~$500**

### Large Scale (20+ players)
- 25 trackers: $625
- 6 beacons: $150
- Accessories: $200
- **Total: ~$1000**

*Prices approximate, varies by source*

---

## 🌍 Regional Considerations

### Frequency Regulations
- **Americas**: 915 MHz (FCC Part 15)
- **Europe**: 868 MHz (ETSI)
- **Asia**: Varies by country

**Use legal frequency for your region!**

### Environmental
- Humidity (tropical)
- Temperature extremes
- Rain/water protection
- UV exposure (sun)

---

**Hardware is the foundation of IRL Hunts. Invest in quality devices, maintain them well, and they'll provide endless hours of outdoor gaming fun!**

🔧🎮 **Build it right, play it right!** 🎮🔧
