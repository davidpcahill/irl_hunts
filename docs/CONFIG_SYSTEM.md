# IRL Hunts Configuration System

## Overview

IRL Hunts uses a **layered configuration system** that separates default settings from your local customizations. This keeps your credentials safe and makes updates easier.

## Key Principle: Override, Don't Replace

The system works like this:

```
[Default Settings] + [Your Overrides] = [Final Configuration]
```

You only need to specify what's different from the defaults.

---

## Server Configuration (Python)

### Files

| File | Purpose | Git Tracked? |
|------|---------|--------------|
| `server/config.py` | All defaults with documentation | ✅ Yes |
| `server/config_local.py` | Your overrides (credentials) | ❌ No |
| `server/config_local.py.example` | Template for your overrides | ✅ Yes |

### How It Works

```python
# In app.py:
try:
    from config_local import *  # Your overrides take precedence
except ImportError:
    from config import *        # Fall back to defaults
```

### Quick Setup

```bash
cd server
cp config_local.py.example config_local.py
nano config_local.py  # Edit with your settings
```

### Minimum Required Settings

```python
ADMIN_PASSWORD = "your_secure_password"
SECRET_KEY = "your_random_secret_key"
```

Everything else has sensible defaults!

---

## Device Configuration (Arduino/C++)

### Files

| File | Purpose | Git Tracked? |
|------|---------|--------------|
| `config.h` | All defaults with documentation | ✅ Yes |
| `config_local.h` | Your overrides (WiFi password) | ❌ No |
| `config_local.h.example` | Template for your overrides | ✅ Yes |

### How It Works

```cpp
// In .ino files:
#if __has_include("config_local.h")
  #include "config_local.h"    // Use your overrides
#else
  #include "config.h"          // Use defaults
#endif

// Fallback defaults in code:
#ifndef PING_INTERVAL
  #define PING_INTERVAL 5000   // Default if not defined
#endif
```

**Important:** The Arduino include is either/or, but the .ino file has `#ifndef` fallbacks for most settings.

### Quick Setup (Tracker)

```bash
cd devices/tracker
cp config_local.h.example config_local.h
nano config_local.h  # Edit with your WiFi credentials
```

### Minimum Required Settings (Tracker)

```cpp
#define WIFI_SSID "YourWiFi"
#define WIFI_PASS "YourPassword"  
#define SERVER_URL "http://192.168.1.100:5000"
```

Everything else has defaults!

### Quick Setup (Beacon)

```bash
cd devices/beacon
cp config_local.h.example config_local.h
nano config_local.h  # Usually just verify LORA_FREQUENCY
```

### Minimum Required Settings (Beacon)

```cpp
#define LORA_FREQUENCY 915.0  // Match your region
```

Beacons don't need WiFi - they just broadcast!

---

## Why This System?

### 1. Security
- Your passwords never go in git
- Default files are safe to commit
- Local overrides stay on your machine

### 2. Simplicity
- Only configure what you need
- Sensible defaults for everything else
- Clear separation of concerns

### 3. Maintainability
- Updates don't overwrite your settings
- Easy to see what you've customized
- Documentation lives with defaults

### 4. Team Collaboration
- Everyone uses same defaults
- Individual customizations don't conflict
- Template files show what's configurable

---

## Common Questions

### Q: Do I need both config.h and config_local.h?

**A:** No! You only need config_local.h with your specific settings. The .ino files have built-in defaults for everything except WiFi credentials and server URL.

### Q: What if I want to use all defaults except WiFi?

**A:** Create config_local.h with just:
```cpp
#define WIFI_SSID "MyWiFi"
#define WIFI_PASS "MyPassword"
#define SERVER_URL "http://192.168.1.100:5000"
```
That's it! Everything else uses defaults.

### Q: Should I edit config.h directly?

**A:** No! Always use config_local.h for your changes. This way:
- Your credentials stay private
- Updates don't break your setup
- You know exactly what you've customized

### Q: How do I know what I can override?

**A:** Check config.h (or config.py for server). Every setting is documented there with explanations and valid values.

---

## Security Checklist

- [ ] config_local.py has strong ADMIN_PASSWORD
- [ ] config_local.py has unique SECRET_KEY
- [ ] config_local.h has correct WiFi credentials
- [ ] All *_local.* files are in .gitignore
- [ ] DEBUG = False for production (server)
- [ ] Never share config_local files

---

## Troubleshooting

### "WIFI_SSID not defined"
Your config_local.h is missing or doesn't have WIFI_SSID. Create it from the example.

### "Invalid password" on admin login
Check your ADMIN_PASSWORD in config_local.py matches what you're entering.

### Tracker can't reach server
Verify SERVER_URL in config_local.h:
- Correct IP address?
- Port 5000 included?
- http:// prefix (not https)?
- Server actually running?

### Settings not taking effect
1. Did you save the file?
2. Did you restart the server/reflash the device?
3. Is the _local file in the right directory?
4. Check for typos in #define statements

---

## Advanced: Environment Variables

For extra security, the server supports environment variables:

```bash
export IRLHUNTS_ADMIN_PASSWORD="super_secret"
python app.py
```

Environment variables take priority over config files.

---

**Remember:** The goal is to make configuration simple and secure. You only need to set a few values, and your credentials never leave your machine!
