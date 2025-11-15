# 🔧 IRL Hunts - Complete Setup Guide

This guide walks you through setting up the complete IRL Hunts system from scratch.

---

## 📋 Prerequisites

### Hardware Needed
- **1x Laptop/Computer** - Runs the game server
- **5-10x Heltec WiFi LoRa 32 V3** - Player trackers (~$20 each)
- **2-4x Heltec WiFi LoRa 32 V3** - Safe zone beacons
- **USB-C cables** - For programming devices
- **WiFi network** - All devices connect to this

### Software Needed
- **Python 3.8+** - [Download](https://python.org)
- **Arduino IDE 2.x** - [Download](https://arduino.cc)
- **Web browser** - Chrome, Firefox, Safari
- **Git** (optional) - For version control

---

## 🖥️ Step 1: Server Setup

### 1.1 Install Python Dependencies

```bash
cd irlhunts/server
pip install -r requirements.txt
```

If you get permission errors:
```bash
pip install --user -r requirements.txt
# OR
pip install --break-system-packages -r requirements.txt
```

### 1.2 Configure the Server

Edit `app.py` and change:

```python
# Line 21 - CHANGE THIS PASSWORD!
ADMIN_PASSWORD = "your_secure_password"

# Line 22-24 - Adjust game defaults if needed
DEFAULT_CAPTURE_RSSI = -70      # Higher = closer required
DEFAULT_SAFEZONE_RSSI = -75     # Safe zone detection range
PROXIMITY_ALERT_RSSI = -80      # Warning distance
```

### 1.3 Find Your Server IP

**Windows:**
```cmd
ipconfig
```
Look for `IPv4 Address` under your WiFi adapter (e.g., `192.168.1.100`)

**Mac/Linux:**
```bash
ifconfig
# or
ip addr
```

### 1.4 Start the Server

```bash
python app.py
```

You should see:
```
============================================================
  🎯 IRL Hunts Game Server
============================================================
  Admin Password: your_password
  Server URL: http://0.0.0.0:5000
============================================================
```

### 1.5 Test Access

Open browser to `http://YOUR_IP:5000`

You should see the login page.

---

## 📱 Step 2: Flash Player Trackers

### 2.1 Install Arduino IDE

1. Download from [arduino.cc](https://arduino.cc)
2. Install and open

### 2.2 Add ESP32 Board Support

1. File → Preferences
2. In "Additional Boards Manager URLs", add:
   ```
   https://espressif.github.io/arduino-esp32/package_esp32_index.json
   ```
3. Tools → Board → Boards Manager
4. Search "esp32"
5. Install "esp32 by Espressif Systems"

### 2.3 Install Required Libraries

Sketch → Include Library → Manage Libraries

Install these (search by name):
1. **"ESP8266 and ESP32 OLED driver for SSD1306 displays"** by ThingPulse
2. **"RadioLib"** by Jan Gromes
3. **"ArduinoJson"** by Benoit Blanchon

### 2.4 Configure Tracker Code

Open `devices/tracker/tracker.ino`

Edit these lines (around line 50-52):
```cpp
const char* WIFI_SSID = "YOUR_WIFI_NAME";
const char* WIFI_PASS = "YOUR_WIFI_PASSWORD";
const char* SERVER_URL = "http://YOUR_SERVER_IP:5000";
```

Example:
```cpp
const char* WIFI_SSID = "MyHomeWifi";
const char* WIFI_PASS = "MyPassword123";
const char* SERVER_URL = "http://192.168.1.100:5000";
```

### 2.5 Select Board Settings

1. Connect Heltec V3 via USB-C
2. Tools → Board → ESP32 Arduino → "Heltec WiFi LoRa 32(V3)"
3. Tools → Port → Select the COM port (COM3, /dev/ttyUSB0, etc.)

### 2.6 Upload

1. Click "Upload" button (→ arrow)
2. Wait for compilation and upload
3. Watch Serial Monitor (115200 baud) for output

The device should:
1. Show "Initializing..."
2. Display unique ID (e.g., `T9EF0`)
3. Connect to WiFi
4. Show "Ready!"

### 2.7 Note the Device ID

Each tracker displays its unique ID on the OLED. Write these down:
- Tracker 1: T____
- Tracker 2: T____
- etc.

### 2.8 Repeat for All Trackers

Flash each device with the same code. They auto-generate unique IDs.

---

## 🏠 Step 3: Flash Safe Zone Beacons

### 3.1 Open Beacon Code

Open `devices/beacon/beacon.ino`

### 3.2 Verify Frequency Matches

```cpp
#define LORA_FREQ   915.0  // Must match trackers!
```

**No WiFi configuration needed** - beacons just broadcast.

### 3.3 Upload to Beacon Devices

Same process as trackers:
1. Connect device
2. Select port
3. Upload

### 3.4 Note Beacon IDs

Each beacon displays its ID (e.g., `SZ1A2B`). Write these down:
- Beacon 1 (Oak Tree): SZ____
- Beacon 2 (Building A): SZ____
- etc.

### 3.5 Place at Safe Zones

Put beacons at designated safe zone locations:
- Tree, building, landmark
- Power with USB battery pack
- They continuously broadcast

---

## 🎮 Step 4: Configure Game

### 4.1 Admin Login

1. Go to `http://YOUR_IP:5000`
2. Click "Admin" tab
3. Enter your admin password
4. Click "Admin Login"

### 4.2 Add Safe Zone Beacons

In Admin Panel:
1. Find "Safe Zone Beacons" section
2. Enter beacon ID (e.g., `SZ1A2B`)
3. Enter name (e.g., "Oak Tree")
4. Set RSSI threshold:
   - `-75` = Standard range (recommended)
   - `-65` = Smaller range (must be closer)
   - `-85` = Larger range (detected from farther)
5. Click "Add Safe Zone"

### 4.3 Adjust Game Settings

- **Honor System**: OFF (use beacons only)
- **Role Change in Safe Zone**: ON
- **Capture RSSI**: -70 (adjust based on testing)

### 4.4 Test Beacon Detection

Have someone with a tracker walk near a beacon. They should:
1. See "Entered safe zone" notification
2. Status shows "SAFE" on tracker
3. Event appears in admin log

---

## 👥 Step 5: Players Join

### 5.1 Distribute Trackers

Give each player a tracker device.

### 5.2 Web Login

Players on their phones:
1. Open `http://YOUR_IP:5000`
2. Enter tracker ID from device OLED (e.g., `T9EF0`)
3. Enter nickname (e.g., "ShadowWolf")
4. Click "Enter Game"

### 5.3 Set Profile Picture

In dashboard:
1. Click "📷 Change Photo"
2. Select image
3. Picture appears in profile and leaderboard

### 5.4 Choose Role

- Click "🐰 Prey" or "🦊 Predator"
- Can change in lobby
- Once game starts, need safe zone to switch

### 5.5 Ready Up

Click "✅ Ready to Play"

Admin sees ready count increasing.

---

## 🎯 Step 6: Start Game

### 6.1 Pre-Game Checklist

Admin verifies:
- [ ] All players ready
- [ ] Safe zone beacons registered
- [ ] RSSI thresholds configured
- [ ] Game duration set
- [ ] Countdown time set

### 6.2 Start

1. Set duration (e.g., 30 minutes)
2. Set countdown (e.g., 10 seconds)
3. Click "▶️ Start Game"

### 6.3 Countdown

- Players see countdown notification
- Phase changes to "COUNTDOWN"
- After countdown, game is "RUNNING"

### 6.4 Monitor

Admin watches:
- Event log for captures, escapes
- Player statuses
- Time remaining
- Photo gallery

---

## 🔧 Troubleshooting

### Tracker Won't Connect to WiFi

1. Double-check SSID/password in code
2. WiFi name is case-sensitive
3. 2.4GHz network (not 5GHz)
4. Router not blocking new devices

### Tracker Not Reaching Server

1. Verify SERVER_URL is correct IP
2. Server is running (check terminal)
3. Both on same network
4. No firewall blocking port 5000

### Beacon Not Detected

1. Confirm beacon ID registered in admin
2. Check RSSI threshold isn't too strict
3. Beacon powered and LED blinking
4. Frequencies match (915.0 MHz)

### Capture Not Working

1. Game must be "running" phase
2. Prey not in safe zone
3. RSSI must exceed threshold
4. Check roles are set correctly

### Web Page Not Loading

1. Server running?
2. Correct IP address?
3. Port 5000 open?
4. Same WiFi network?

---

## 📊 Testing Recommendations

### Before Event

1. **Range Test**: Walk around with two trackers, note RSSI vs distance
2. **Safe Zone Test**: Walk in/out of beacon range
3. **Capture Test**: Attempt captures at various distances
4. **Full Run**: Do 10-minute test game with 2-3 people

### During Event

1. **Monitor Admin Panel**: Watch for issues
2. **Check Tracker Batteries**: Ensure charged
3. **Verify Connectivity**: All trackers pinging server
4. **Adjust Settings**: Tweak RSSI if captures too easy/hard

---

## 🎮 Game Day Checklist

### Pre-Game (30 min before)

- [ ] Server laptop charged/plugged in
- [ ] Server running
- [ ] All trackers charged
- [ ] All beacons placed and powered
- [ ] Beacons registered in admin
- [ ] Test capture with volunteers
- [ ] Brief players on rules

### Game Start

- [ ] All players logged in
- [ ] Roles selected
- [ ] Ready status confirmed
- [ ] Safety briefing done
- [ ] Emergency contacts shared
- [ ] Start game!

### Post-Game

- [ ] End game in admin
- [ ] Screenshot final scores
- [ ] Collect all devices
- [ ] Retrieve beacons
- [ ] Share highlights

---

## 🚀 You're Ready!

With everything set up, you're ready to host an amazing IRL Hunts game. Remember:
- Safety first
- Have fun
- Iterate on settings based on feedback
- Take photos of the fun!

Happy Hunting! 🎯🦊🐰
