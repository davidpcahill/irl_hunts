# IRL Hunts Configuration Guide

This document explains the configuration system for IRL Hunts.

---

## 📁 Configuration File Structure

```
irlhunts/
├── server/
│   ├── config.py                    # Default config (committed)
│   ├── config_local.py.example      # Template (committed)
│   └── config_local.py              # YOUR config (gitignored)
├── devices/
│   ├── tracker/
│   │   ├── config.h                 # Default config (committed)
│   │   ├── config_local.h.example   # Template (committed)
│   │   └── config_local.h           # YOUR config (gitignored)
│   └── beacon/
│       ├── config.h                 # Default config (committed)
│       ├── config_local.h.example   # Template (committed)
│       └── config_local.h           # YOUR config (gitignored)
```

---

## 🔧 Server Configuration

### Quick Setup

```bash
cd server
cp config_local.py.example config_local.py
nano config_local.py  # or your favorite editor
```

### Required Settings

```python
# MUST CHANGE THESE!
ADMIN_PASSWORD = "your_secure_password_here"
SECRET_KEY = "random_32_character_string_here"
```

### Optional Settings

```python
# Server binding
HOST = "0.0.0.0"  # Listen on all interfaces
PORT = 5000

# Production mode
DEBUG = False
ALLOW_UNSAFE_WERKZEUG = False

# Game limits
MAX_PLAYERS = 100
DEFAULT_CAPTURE_RSSI = -70
DEFAULT_SAFEZONE_RSSI = -75

# Points
SIGHTING_POINTS = 25
SIGHTING_COOLDOWN = 30

# Logging
LOG_FILE = "game.log"
LOG_LEVEL = "INFO"
```

### Environment Variable Override

For maximum security, set admin password via environment:

```bash
export IRLHUNTS_ADMIN_PASSWORD="super_secure_password"
python app.py
```

Environment variable takes precedence over config file.

---

## 📟 Device Configuration (Arduino)

### Quick Setup

```bash
cd devices/tracker  # or beacon
cp config_local.h.example config_local.h
nano config_local.h  # or edit in Arduino IDE
```

### Required Settings

```cpp
// WiFi credentials
#define WIFI_SSID "YourWiFiName"
#define WIFI_PASS "YourWiFiPassword"

// Server URL (get IP with ipconfig/ifconfig)
#define SERVER_URL "http://192.168.1.100:5000"
```

### Regional Settings

```cpp
// LoRa frequency for your region
// Americas: 915.0
// Europe: 868.0
// Asia: 433.0
#define LORA_FREQUENCY 915.0
```

### Optional Customization

```cpp
// Custom device ID (instead of MAC-based)
#define USE_CUSTOM_ID 1
#define CUSTOM_DEVICE_ID "TRACKER1"

// Timing adjustments
#define PING_INTERVAL 5000           // Server ping (ms)
#define BEACON_INTERVAL 3000         // LoRa broadcast (ms)
#define WIFI_RECONNECT_INTERVAL 15000

// Emergency settings
#define EMERGENCY_HOLD_TIME 2000     // Hold button (ms)
#define EMERGENCY_TAP_COUNT 3        // Taps to confirm
```

---

## 🔒 Security Best Practices

### DO:
- ✅ Use strong, unique admin password
- ✅ Generate random SECRET_KEY
- ✅ Keep config_local files out of git
- ✅ Use environment variables for production
- ✅ Disable DEBUG in production
- ✅ Use different passwords for different events

### DON'T:
- ❌ Commit config_local.py to git
- ❌ Share your WiFi password publicly
- ❌ Use default admin password
- ❌ Enable DEBUG in production
- ❌ Hardcode credentials in source files

---

## 🔄 Loading Priority

### Server (Python)

1. Try `from config_local import *`
2. If not found, try `from config import *`
3. If not found, use hardcoded defaults
4. Environment variables override config files

### Devices (Arduino)

1. Check `#if __has_include("config_local.h")`
2. If found, include it
3. Else, include `config.h`
4. All settings have fallback defaults

---

## 📝 Configuration Options Reference

### Server Options (config.py)

| Option | Type | Default | Description |
|--------|------|---------|-------------|
| `ADMIN_PASSWORD` | string | "hunt2024!" | Admin panel password |
| `SECRET_KEY` | string | auto | Flask session secret |
| `HOST` | string | "0.0.0.0" | Server bind address |
| `PORT` | int | 5000 | Server port |
| `DEBUG` | bool | True | Enable debug mode |
| `SESSION_LIFETIME_HOURS` | int | 12 | Session duration |
| `MAX_UPLOAD_SIZE` | int | 16MB | Max file upload size |
| `DEFAULT_CAPTURE_RSSI` | int | -70 | Capture proximity threshold |
| `DEFAULT_SAFEZONE_RSSI` | int | -75 | Safe zone detection range |
| `SIGHTING_POINTS` | int | 25 | Points per photo |
| `SIGHTING_COOLDOWN` | int | 30 | Seconds between sightings |
| `CAPTURE_COOLDOWN` | int | 10 | Seconds between captures |
| `PLAYER_TIMEOUT` | int | 30 | Offline after X seconds |
| `MAX_PLAYERS` | int | 100 | Maximum concurrent players |
| `MAX_EVENTS` | int | 1000 | Events to keep in memory |
| `MAX_MESSAGES` | int | 500 | Messages to keep in memory |
| `LOG_FILE` | string | None | Log file path (None=console) |
| `LOG_LEVEL` | string | "INFO" | Logging verbosity |
| `SOCKETIO_ASYNC_MODE` | string | "threading" | WebSocket mode |

### Device Options (config.h)

| Option | Type | Default | Description |
|--------|------|---------|-------------|
| `WIFI_SSID` | string | - | WiFi network name |
| `WIFI_PASS` | string | - | WiFi password |
| `SERVER_URL` | string | - | Server HTTP URL |
| `LORA_FREQUENCY` | float | 915.0 | LoRa frequency (MHz) |
| `LORA_SF` | int | 7 | Spreading factor |
| `LORA_BW` | float | 125.0 | Bandwidth (kHz) |
| `LORA_CR` | int | 5 | Coding rate |
| `LORA_TX_POWER` | int | 14 | Transmit power (dBm) |
| `PING_INTERVAL` | int | 5000 | Server ping interval (ms) |
| `BEACON_INTERVAL` | int | 3000 | LoRa broadcast interval (ms) |
| `EMERGENCY_HOLD_TIME` | int | 2000 | Emergency button hold (ms) |
| `BATTERY_DIVIDER_RATIO` | float | 4.9 | Battery voltage divider |

---

## 🚀 Quick Reference

### First Time Setup

```bash
# 1. Server config
cd server
cp config_local.py.example config_local.py
# Edit: ADMIN_PASSWORD, SECRET_KEY

# 2. Tracker config
cd ../devices/tracker
cp config_local.h.example config_local.h
# Edit: WIFI_SSID, WIFI_PASS, SERVER_URL

# 3. Beacon config (if using)
cd ../beacon
cp config_local.h.example config_local.h
# Edit: LORA_FREQUENCY
```

### Verify Config Loaded

**Server:**
```bash
python app.py
# Look for: "✅ Loaded local configuration (config_local.py)"
```

**Tracker:**
```
Open Serial Monitor (115200 baud)
# Look for: "Config: config_local.h"
```

---

## 🐛 Troubleshooting

### "No config file found"

Server can't find config.py:
```bash
cd server
ls -la config*.py  # Check files exist
python -c "from config import *"  # Test import
```

### "WIFI_SSID not defined"

Arduino config missing:
```bash
cd devices/tracker
ls -la config*.h  # Check files exist
# Make sure config_local.h has all required #defines
```

### "Invalid password"

Check your config_local.py:
```python
ADMIN_PASSWORD = "check_this_matches"
```

Also check environment variable:
```bash
echo $IRLHUNTS_ADMIN_PASSWORD
```

---

## 📦 Deployment Checklist

- [ ] Created `server/config_local.py`
- [ ] Set strong `ADMIN_PASSWORD`
- [ ] Generated random `SECRET_KEY`
- [ ] Set `DEBUG = False`
- [ ] Created `devices/*/config_local.h`
- [ ] Set correct `WIFI_SSID` and `WIFI_PASS`
- [ ] Set correct `SERVER_URL` (server's IP)
- [ ] Set correct `LORA_FREQUENCY` for region
- [ ] Verified config files NOT in git
- [ ] Tested server starts with configs
- [ ] Tested devices connect with configs

---

**Happy configuring! 🔧**
