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
├── server/                      # Web server application
│   ├── app.py                   # Main Flask server
│   ├── requirements.txt         # Python dependencies
│   └── templates/               # Web interface
│       ├── login.html           # Player/admin login
│       ├── dashboard.html       # Player game interface
│       └── admin.html           # Admin control panel
├── devices/                     # Arduino device code
│   ├── tracker/                 # Player tracker
│   │   └── tracker.ino          # Main tracker firmware
│   └── beacon/                  # Safe zone beacon
│       └── beacon.ino           # Beacon firmware
├── docs/                        # Documentation
│   ├── SETUP.md                 # Detailed setup guide
│   ├── GAMEPLAY.md              # Game rules and strategies
│   └── HARDWARE.md              # Hardware requirements
└── LICENSE                      # MIT License
```

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
2. Place at designated safe zone location
3. Note beacon ID from OLED (e.g., `SZ1A2B`)
4. Register in admin panel

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

### Tracker Display
```
T9EF0 | pred
> active
Game: running
3 nearby | Best: TA2B3 -65dB
BTN=Capture | 2 attempts
```

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
- Share game plans with someone not playing
- Stay aware of surroundings
- Set boundaries for play area
- Have emergency contacts
- Use emergency button if needed
- Stay hydrated
- Watch for obstacles

---

## 📞 Support

- Check `docs/` folder for detailed guides
- Open GitHub issue for bugs
- Serial Monitor (115200 baud) for device debug
- Browser console (F12) for web issues

---

<p align="center">
  <b>Happy Hunting! 🎯🦊🐰</b>
  <br><br>
  <i>Built with ❤️ for real-world gaming</i>
</p>
