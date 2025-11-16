# 🎯 IRL Hunts

**A real-world augmented reality game of predator vs prey using LoRa mesh networking**

[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](https://opensource.org/licenses/MIT)
[![Python 3.8+](https://img.shields.io/badge/python-3.8+-blue.svg)](https://www.python.org/downloads/)
[![Platform](https://img.shields.io/badge/platform-Heltec%20V3-orange.svg)](https://heltec.org/)

---

## 🌟 What is IRL Hunts?

IRL Hunts is an outdoor game system that brings the thrill of digital gaming into the real world. Players carry small wireless trackers that communicate via LoRa radio, enabling a game of cat-and-mouse across large outdoor spaces like parks, convention centers, or campuses.

**Predators** hunt down **Prey** using proximity detection, while Prey use strategy, teamwork, and safe zones to survive. A central web server manages everything: leaderboards, team chat, photo sightings, and game rules.

---

## ✨ Key Features

### 🎮 Gameplay
- **Real proximity-based capture** - No GPS required, works indoors and outdoors
- **Automatic safe zone detection** - Physical beacon devices create sanctuary areas
- **Photo sightings** - Earn points by photographing the opposing team
- **Live leaderboards** - Rankings update in real-time
- **Team chat** - Coordinate strategies with your team
- **Custom profiles** - Nicknames and profile pictures
- **Consent system** - Players control physical contact, photography, and location sharing preferences

### 🔧 Technology
- **LoRa mesh networking** - 1km+ range, no cellular needed
- **Heltec V3 trackers** - OLED display, WiFi + LoRa
- **Flask web server** - Real-time WebSocket updates
- **Responsive web UI** - Works on any phone or tablet
- **RSSI-based proximity** - Configurable capture ranges

### 👥 Administration
- **Complete game control** - Start, pause, end, reset
- **Safe zone management** - Add/configure beacon devices
- **Player management** - Kick, release, promote to moderator
- **Event logging** - Track every capture, escape, and zone entry
- **Bounty system** - Place rewards on specific players
- **Adjustable settings** - RSSI thresholds, honor system, rules

---

## 📦 What's Included

```
irlhunts/
├── README.md                    # This file
├── LICENSE                      # MIT License
├── .gitignore                   # Git ignore rules (keeps credentials safe)
├── server/                      # Web server application
│   ├── app.py                   # Main Flask server
│   ├── config.py                # Default configuration (documented)
│   ├── config_local.py.example  # Template for your local overrides
│   ├── config_local.py          # YOUR settings (gitignored, you create this)
│   ├── requirements.txt         # Python dependencies
│   ├── uploads/                 # User-uploaded photos (contents gitignored)
│   │   └── .gitkeep             # Keeps folder in git
│   └── templates/               # Web interface
│       ├── login.html           # Player/admin login
│       ├── dashboard.html       # Player game interface
│       └── admin.html           # Admin control panel
├── devices/                     # Arduino device code
│   ├── tracker/                 # Player tracker
│   │   ├── tracker.ino          # Main tracker firmware
│   │   ├── config.h             # Default device settings (documented)
│   │   ├── config_local.h.example  # Template for your WiFi/server settings
│   │   └── config_local.h       # YOUR settings (gitignored, you create this)
│   └── beacon/                  # Safe zone beacon
│       ├── beacon.ino           # Beacon firmware
│       ├── config.h             # Default device settings (documented)
│       ├── config_local.h.example  # Template for your settings
│       └── config_local.h       # YOUR settings (gitignored, you create this)
├── docs/                        # Documentation
│   ├── SETUP.md                 # Detailed setup guide
│   ├── GAMEPLAY.md              # Game rules and strategies
│   ├── HARDWARE.md              # Hardware requirements
│   ├── CONFIG.md                # Configuration options reference
│   ├── CONFIG_SYSTEM.md         # How the config system works
│   └── TEST.md                  # Testing procedures
└── patches/                     # Maintenance scripts
    ├── patch_gitignore.py       # Ensures credentials are gitignored
    └── irlhunts_patch.py        # Fixes config issues and bugs
```


**Configuration Philosophy:** You only need to set what's different from defaults! 
- `config.h` / `config.py` = All defaults with documentation (don't edit)
- `config_local.h` / `config_local.py` = Your overrides only (credentials, WiFi, etc.)
- See `docs/CONFIG_SYSTEM.md` for detailed explanation

**Note:** Files ending in `.example` are templates. Copy them to create your local config files (without `.example`). Local config files contain credentials and are gitignored.

---

## 🚀 Quick Start

### 1. Server Setup (5 minutes)

```bash
cd server
pip install -r requirements.txt

# Create local config
cp config_local.py.example config_local.py
# Edit config_local.py with your admin password

python app.py
```

Server runs at `http://YOUR_IP:5000`

### 2. Flash Trackers (10 minutes each)

1. Install Arduino IDE libraries:
   - **ESP8266 and ESP32 OLED driver for SSD1306 displays** by ThingPulse
   - **RadioLib** by Jan Gromes
   - **ArduinoJson** by Benoit Blanchon (version 6.x or 7.x)

2. Create config file `devices/tracker/config_local.h`:
   ```cpp
   #define WIFI_SSID "your_wifi"
   #define WIFI_PASS "your_password"
   #define SERVER_URL "http://YOUR_SERVER_IP:5000"
   ```

3. Flash to Heltec V3 devices

### 3. Set Up Safe Zones (5 minutes each)

1. Flash `devices/beacon/beacon.ino` to spare Heltec V3
2. Beacon auto-generates unique ID (e.g., `SZ1A2B`)
3. Place at designated safe zone location with USB power bank
4. Register in admin panel with appropriate RSSI threshold
5. Test by walking near beacon - tracker should show "Entered safe zone"

**Beacon Features (v3):**
- Battery monitoring with low-power warnings
- Health reporting every 5 minutes (Serial Monitor)
- LED patterns indicate status
- Auto-recovery from LoRa errors

### 4. Play!

1. Players open `http://SERVER_IP:5000` on phone
2. Enter tracker ID and nickname
3. Choose role: 🐰 Prey or 🦊 Predator
4. Admin starts game
5. The hunt begins!

---

## 🎮 How to Play

### Predators 🦊
Your mission: **Capture prey by getting close enough**

- Your tracker detects nearby prey signals
- Get within capture range (RSSI threshold)
- Press button to capture
- Take photos for bonus points
- Coordinate with other predators

**Scoring:**
- +100 points per capture
- +50 bonus for 3+ captures
- +100 bonus for 5+ captures
- +25 per photo sighting

### Prey 🐰
Your mission: **Survive and escape when caught**

- Avoid predators using proximity warnings
- Use safe zones strategically
- Escape by entering safe zone while captured
- Photo predators for points
- Work with other prey

**Scoring:**
- +75 points per escape
- +25 per photo sighting
- +200 survival bonus (never caught)

### Safe Zones 🏠
Physical locations with beacon devices:
- Auto-detected by your tracker
- Captured prey instantly escape upon entry
- Can't be captured while inside
- Strategic locations around play area

---

## 📱 Screenshots

### Player Dashboard
- Profile with custom picture
- Live game timer with low-time warnings
- Leaderboard rankings
- Team chat
- Photo gallery
- Status indicators
- Keyboard shortcuts (P=photo, R=refresh, ?=help)

### Admin Panel
- Game control buttons
- Safe zone beacon manager
- Player list with actions
- Event log stream
- Settings configuration
- Bounty system

### Tracker Display (Interactive Edition v5.0)

The tracker now features **6 cycling information screens** accessible by holding the button:

**Screen 1: MAIN STATUS**
```
ShadowWolf [WS] [T]  1/6
PREDATOR [Hunt]
Classic Hunt LIVE
Pts: 350 | active
🎯 3 prey | Best: -65dB
```

**Screen 2: NEARBY PLAYERS**
```
NEARBY PLAYERS (5)
TA2B3 prey -52dB [STD]
T1F4E pred -68dB [T]
T9C01 prey -71dB
+2 more...
```

**Screen 3: PERSONAL STATS**
```
MY STATISTICS
Points: 450
Captures: 4
Sightings: 6
Holding: 2 prey
```

**Screen 4: RECENT EVENTS**
```
RECENT EVENTS (8)
success: Captured Rabb
warning: TA2B3 ESCAPED
info: Game started!
danger: NEW PREDATOR...
```

**Screen 5: CHAT MESSAGES**
```
CHAT MESSAGES
ShadowWolf:
 Let's hunt north!
Admin:
 5 minutes left!
```

**Screen 6: HELP**
```
CONTROLS & HELP
Short press: Action
Hold 1s: Next screen
Hold 2s+3tap: EMERGENCY
ID: T9EF0
```

**Button Controls:**
- **Short press** (<500ms): Context action (capture/threat check)
- **Medium press** (500ms-1.5s): Cycle to next screen
- **Long hold** (2s) + 3 taps: Emergency trigger

**LED Patterns:**
- Predator: Double blink
- Prey: Slow single blink
- Safe Zone: Breathing pulse effect
- Captured: Fast blink
- Emergency: Rapid strobe
- No WiFi: Very fast blink

---

## 🔧 Hardware Requirements

### Per Player
- **1x Heltec WiFi LoRa 32 V3** (~$20-25)
- **1x LoRa antenna** (usually included)
- **1x USB-C cable** (for charging/programming)
- **Battery pack** (optional, for longer games)

### Per Safe Zone
- **1x Heltec WiFi LoRa 32 V3**
- **1x LoRa antenna**
- **Power source** (USB battery pack)

### Server
- **Laptop/Computer** on same WiFi network
- **Python 3.8+**

### Recommended Setup
- 5-10 player trackers
- 2-4 safe zone beacons
- 1 laptop as server
- Large outdoor area (park, campus, convention)

---

## 📡 Technical Details

### LoRa Communication
- **Frequency:** 915 MHz (US) or 868 MHz (EU)
- **Range:** Up to 1km line-of-sight
- **Data:** Player ID + role broadcast
- **RSSI:** Signal strength for proximity

### RSSI Reference
| RSSI Range | Approximate Distance |
|------------|---------------------|
| -30 to -50 | 1-2 meters (very close) |
| -50 to -60 | 2-5 meters |
| -60 to -70 | 5-10 meters |
| -70 to -80 | 10-20 meters |
| -80 to -90 | 20+ meters |

### Server Architecture
- **Flask** - Web framework
- **Flask-SocketIO** - Real-time communication
- **REST API** - Tracker communication
- **WebSocket** - Live updates to browsers

---

## 🎯 Use Cases

### Convention Events
- Furry conventions
- Gaming conventions
- Comic-cons
- Tech meetups

### Outdoor Activities
- Campus events
- Park gatherings
- Team building
- Birthday parties

### Educational
- STEM demonstrations
- IoT workshops
- Game design classes
- Networking concepts

---

## 🛠️ Configuration

### Server Settings (`server/config_local.py`)
```python
ADMIN_PASSWORD = "your_password"      # Change this!
SECRET_KEY = "random_secret"          # For sessions
DEBUG = False                         # Production mode
```

### Device Settings (`devices/*/config_local.h`)
```cpp
#define WIFI_SSID "your_wifi"
#define WIFI_PASS "your_password"
#define SERVER_URL "http://192.168.1.100:5000"
#define LORA_FREQUENCY 915.0           # Your region
```

**Note:** Local config files are gitignored to protect your credentials!

### In-Game Settings (Admin Panel)
- **Honor System** - Allow manual safe zone toggle
- **Role Change** - Switch roles in safe zones
- **Capture Threshold** - RSSI requirement
- **Game Duration** - Match length

---

## 🤝 Contributing

We'd love your help making IRL Hunts better!

### Ideas for Improvement
- [ ] GPS overlay map
- [ ] Mobile native app
- [ ] Heart rate integration
- [ ] Achievement system
- [ ] Tournament brackets
- [ ] Spectator mode
- [ ] Sound effects
- [ ] NFC tag checkpoints
- [ ] Team vs team modes
- [ ] Power-ups

### How to Contribute
1. Fork the repository
2. Create feature branch
3. Make your changes
4. Test thoroughly
5. Submit pull request

---

## 📄 License

This project is licensed under the MIT License - see the [LICENSE](LICENSE) file for details.

---

## 🙏 Acknowledgments

- **Heltec** for excellent LoRa hardware
- **RadioLib** for LoRa library
- **Flask** team for the framework
- **Furry community** for inspiration
- Everyone who playtests!

---

## ⚠️ Safety Notice

**Always prioritize safety:**
- **Respect ALL consent settings** - Check badges before any interaction
  - 🤝 = Physical contact OK (gentle shoulder tap)
  - No badge = NO touching allowed
  - Players can disable photos and location sharing
- Share game plans with someone not playing
- Stay aware of surroundings
- Set boundaries for play area
- Have emergency contacts
- Use emergency button if needed
- Stay hydrated
- Watch for obstacles
- **Consent is mandatory** - Violating consent settings may result in removal

---

## 📞 Support

- Check `docs/` folder for detailed guides
- Open GitHub issue for bugs
- Serial Monitor (115200 baud) for device debug
- Browser console (F12) for web issues
- LoRa packet format: `ID|role|consent` (e.g., `T9EF0|pred|STD`)

### Common LoRa Issues
If trackers show "No players nearby" when devices are close:
1. Open Serial Monitor (115200 baud) on both devices
2. Verify TX messages: `TX #1: T1234|pred|STD` (3 parts with `|`)
3. Verify RX messages: `RX from T5678 (prey) RSSI: -45dB | Total nearby: 1`
4. Check frequencies match (915.0 MHz for Americas)
5. Ensure antennas are connected securely

---

<p align="center">
  <b>Happy Hunting! 🎯🦊🐰</b>
  <br><br>
  <i>Built with ❤️ for real-world gaming</i>
</p>
