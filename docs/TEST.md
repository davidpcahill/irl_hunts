# IRL Hunts - Testing & Validation Guide

Complete testing procedures for development, QA, and production deployment.


## ⚠️ Important: Testing Multiple Players

### Session Isolation Warning

When testing multiple players on the same computer:

**DO NOT** use multiple private browsing tabs in the same window!
- Private tabs in the same window share cookies/sessions
- This causes names, photos, and roles to "switch" between players
- Very confusing bugs result!

**CORRECT ways to test multiple players:**

1. **Different Browsers** (Recommended)
   - Player 1: Chrome
   - Player 2: Firefox
   - Player 3: Safari/Edge
   - Each browser has its own session

2. **Separate Private Windows** (Not tabs!)
   - Player 1: Chrome Private Window 1
   - Player 2: Chrome Private Window 2 (NEW WINDOW, not new tab)
   - Each window is isolated

3. **Different Browser Profiles**
   - Create Chrome profiles for each test player
   - Each profile is fully isolated

4. **Different Devices** (Best for realistic testing)
   - Use actual phones/tablets
   - Real network conditions
   - Realistic user experience

### Session Validation

The system now includes automatic session validation:
- Each login generates a unique session token
- Dashboard checks session validity every 30 seconds
- Warning banner appears if session conflict detected
- Recommends reload or re-login

### What "Session Conflict" Means

If you see "SESSION CONFLICT" warning:
1. Another login happened in the same browser context
2. Your session cookie was overwritten
3. You may see someone else's data
4. **Solution**: Reload or use different browser

---

## 📋 Table of Contents

1. [Quick Smoke Tests](#quick-smoke-tests)
2. [Unit Tests](#unit-tests)
3. [Integration Tests](#integration-tests)
4. [End-to-End Tests](#end-to-end-tests)
5. [Hardware Tests](#hardware-tests)
6. [Performance Tests](#performance-tests)
7. [Security Tests](#security-tests)
8. [Game Mode Tests](#game-mode-tests)
9. [Emergency System Tests](#emergency-system-tests)
10. [Pre-Production Checklist](#pre-production-checklist)
11. [Automated Test Scripts](#automated-test-scripts)

---

## 🔥 Quick Smoke Tests

### Server Startup (30 seconds)
```bash
cd server
python -m py_compile app.py  # Syntax check
python app.py                 # Should see banner

# Expected output:
# ============================================================
#   🎯 IRL Hunts Game Server v4
# ============================================================
```

### Basic API Health (1 minute)
```bash
# Server running in another terminal
curl http://localhost:5000/api/game
# Should return JSON with game state

curl http://localhost:5000/api/players
# Should return empty {} or player list

curl http://localhost:5000/api/beacons
# Should return [] or beacon list
```

### Web Interface Load (2 minutes)
1. Open `http://localhost:5000`
2. ✅ Login page loads
3. Enter fake device ID: `TTEST`
4. ✅ Dashboard loads
5. Check browser console (F12) - no red errors
6. Click Admin tab
7. Enter password
8. ✅ Admin panel loads

---

## 🧪 Unit Tests

### Scoring Calculations

**Test Classic Hunt Scoring:**
```python
# In Python shell or test file
from app import calculate_points, players, game, GAME_MODES

# Setup
test_player = {
    "role": "pred",
    "captures": 3,
    "escapes": 0,
    "times_captured": 0,
    "sightings": 2,
    "points": 0,
    "infections": 0,
    "team": None
}
game["mode"] = "classic"
game["phase"] = "ended"

calculate_points(test_player)
assert test_player["points"] == 350  # 300 (3 caps) + 50 (3+ bonus) + 0 (no 5+) + 50 (2 sightings)
print("✅ Classic scoring correct")
```

**Test Infection Mode Scoring:**
```python
game["mode"] = "infection"
test_player["infections"] = 3
calculate_points(test_player)
# Should be: 150 (3*50) + 25 (3+ bonus) + 75 (3*25 infections) + 50 (sightings) = 300
assert test_player["points"] == 300
print("✅ Infection scoring correct")
```

**Test Prey Survival Bonus:**
```python
prey_player = {
    "role": "prey",
    "captures": 0,
    "escapes": 2,
    "times_captured": 0,  # Never caught!
    "sightings": 1,
    "points": 0,
    "infections": 0,
    "team": None
}
game["mode"] = "classic"
game["phase"] = "ended"
calculate_points(prey_player)
assert prey_player["points"] == 375  # 150 (escapes) + 25 (sighting) + 200 (survival)
print("✅ Survival bonus correct")
```

### RSSI Validation

```python
# Test beacon RSSI validation
from app import api_add_beacon

# Test invalid RSSI values
# Should reject -150 (too low)
# Should reject -10 (too high)
# Should accept -75 (valid)
```

### Input Sanitization

```python
# Test nickname sanitization
test_cases = [
    ("Normal Name", "Normal Name"),
    ("  spaces  ", "spaces"),
    ("Hack<script>", "Hackscript"),  # No < or >
    ("🔥Fire🔥", "Fire"),  # Emoji stripped
    ("O'Connor", "O'Connor"),  # Apostrophe OK
    ("A", ""),  # Too short, becomes empty
    ("x" * 50, "x" * 30),  # Truncated to 30
]
```

---

## 🔗 Integration Tests

### Player Flow Test

```bash
# Terminal 1: Server
python app.py

# Terminal 2: Simulate player
curl -X POST http://localhost:5000/api/login \
  -H "Content-Type: application/json" \
  -d '{"device_id": "TTEST"}'
# ✅ Should return player object with session cookie

# Use cookie for subsequent requests
curl -X PUT http://localhost:5000/api/player \
  -H "Content-Type: application/json" \
  -b "session=..." \
  -d '{"nickname": "TestPlayer", "role": "prey"}'
# ✅ Should update player
```

### Capture Flow Test

```python
# Simulate capture
from app import process_capture, players, game

# Setup predator
players["PRED1"] = {
    "role": "pred",
    "captures": 0,
    "status": "active",
    "in_safe_zone": False,
    "has_photo_of": [],
    # ... other fields
}

# Setup prey
players["PREY1"] = {
    "role": "prey",
    "status": "active",
    "in_safe_zone": False,
    "times_captured": 0,
    # ... other fields
}

game["phase"] = "running"
game["emergency"] = False

# Test capture
success, msg = process_capture("PRED1", "PREY1", -65)
assert success == True
assert players["PREY1"]["status"] == "captured"
assert players["PRED1"]["captures"] == 1
print("✅ Capture flow works")
```

### Safe Zone Detection Test

```python
from app import update_safe_zone, players, beacons

# Setup
players["TEST1"] = {
    "in_safe_zone": False,
    "safe_zone_beacon": None,
    "status": "captured",
    "escapes": 0,
    "name": "TestPlayer",
    "notifications": []
}

beacons["SZ1234"] = {
    "name": "Oak Tree",
    "rssi": -75,
    "active": True
}

# Test entering safe zone
beacon_rssi = {"SZ1234": -70}  # Strong signal
update_safe_zone("TEST1", beacon_rssi)

assert players["TEST1"]["in_safe_zone"] == True
assert players["TEST1"]["status"] == "active"  # Escaped!
assert players["TEST1"]["escapes"] == 1
print("✅ Safe zone detection works")
```

### WebSocket Connection Test

```javascript
// Browser console test
const socket = io('http://localhost:5000');

socket.on('connect', () => {
    console.log('✅ WebSocket connected');
});

socket.on('event', (data) => {
    console.log('✅ Received event:', data);
});

socket.emit('heartbeat');
socket.on('heartbeat_ack', (data) => {
    console.log('✅ Heartbeat acknowledged:', data);
});
```

---

## 🎮 End-to-End Tests

### Complete Game Flow

**Setup:**
1. Start server
2. Open 3 browser tabs (Admin, Player1, Player2)

**Test Steps:**

1. **Player Registration**
   - [ ] Player1 logs in as `TPREY` with nickname "Rabbit"
   - [ ] Player2 logs in as `TPRED` with nickname "Wolf"
   - [ ] Both appear in admin player list

2. **Role Selection**
   - [ ] Player1 selects Prey role
   - [ ] Player2 selects Predator role
   - [ ] Confirmation dialog shows role description
   - [ ] Roles appear correctly in leaderboard

3. **Ready Up**
   - [ ] Both players click Ready
   - [ ] Admin sees "2/2 ready"
   - [ ] Ready count updates in real-time

4. **Game Start**
   - [ ] Admin sets 5-minute game
   - [ ] Admin clicks Start
   - [ ] Countdown appears for all
   - [ ] Game phase changes to "RUNNING"
   - [ ] Timer starts counting down

5. **Photo Sighting**
   - [ ] Player2 (pred) opens photo modal
   - [ ] Selects Player1 as target
   - [ ] Uploads photo
   - [ ] Sighting recorded (+25 points)
   - [ ] Photo appears in gallery

6. **Simulated Capture**
   - [ ] Admin manually triggers capture event via API
   - [ ] Player1 sees "CAPTURED" overlay
   - [ ] Player2 gets points
   - [ ] Leaderboard updates

7. **Safe Zone Escape**
   - [ ] Player1 (captured) enters safe zone
   - [ ] Status changes to "active"
   - [ ] Escape points awarded
   - [ ] Notification sent

8. **Timer Warning**
   - [ ] Timer turns orange at <5:00
   - [ ] Timer turns red and flashes at <1:00
   - [ ] Audio cue (if enabled)

9. **Game End**
   - [ ] Timer reaches 0:00
   - [ ] Game phase changes to "ENDED"
   - [ ] Final scores calculated
   - [ ] Leaderboard shows winners

10. **Reset**
    - [ ] Admin clicks Reset
    - [ ] All stats cleared
    - [ ] Phase returns to "LOBBY"
    - [ ] Players remain registered

---

## 📟 Hardware Tests

### Tracker Connectivity Test

```cpp
// In Serial Monitor (115200 baud)

// 1. Power on tracker
// Expected: Shows device ID (T1234)

// 2. WiFi connection
// Expected: "WiFi OK: 192.168.x.x"

// 3. Server ping
// Expected: Phase, role updates from server

// 4. LoRa transmission
// Expected: "TX #1: T1234|unknown"

// 5. Battery reading
// Expected: "Free heap: XXXXX bytes"
```

### Beacon Broadcasting Test

```cpp
// Serial Monitor for beacon

// 1. Power on
// Expected: Shows beacon ID (SZ1234)

// 2. Transmission
// Expected: "TX #1: SZ1234|SAFEZONE" every 2 seconds

// 3. LED toggle
// Expected: LED blinks on each transmission

// 4. Health check (every 5 min)
// Expected: TX count, free heap, uptime
```

### Range Test Protocol

1. Place two trackers at known distance
2. Record RSSI at each distance:

| Distance | Expected RSSI | Actual RSSI | Pass? |
|----------|--------------|-------------|-------|
| 1m | -30 to -50 | | |
| 5m | -50 to -65 | | |
| 10m | -60 to -75 | | |
| 20m | -70 to -85 | | |
| 50m | -80 to -95 | | |

### Battery Life Test

1. Fully charge tracker
2. Start continuous operation
3. Log voltage every 30 minutes:

| Time | Voltage | Percentage | Notes |
|------|---------|-----------|-------|
| 0:00 | 4.2V | 100% | Start |
| 0:30 | | | |
| 1:00 | | | |
| 2:00 | | | |
| 4:00 | | | |
| 6:00 | | | Low warning? |
| 8:00 | | | Shutdown? |

---

## 🚀 Performance Tests

### Concurrent User Load Test

```python
import asyncio
import aiohttp
import time

async def simulate_player(session, player_id):
    # Login
    async with session.post('/api/login', json={'device_id': player_id}) as resp:
        if resp.status != 200:
            return False
    
    # Ping 10 times
    for _ in range(10):
        async with session.get('/api/game') as resp:
            if resp.status != 200:
                return False
        await asyncio.sleep(1)
    
    return True

async def load_test(num_players):
    async with aiohttp.ClientSession('http://localhost:5000') as session:
        tasks = [simulate_player(session, f'T{i:04d}') for i in range(num_players)]
        start = time.time()
        results = await asyncio.gather(*tasks)
        elapsed = time.time() - start
        
        success = sum(results)
        print(f"✅ {success}/{num_players} players successful")
        print(f"⏱️ Total time: {elapsed:.2f}s")
        print(f"📊 Avg response: {elapsed/num_players:.3f}s")

# Test with increasing load
asyncio.run(load_test(10))   # Light load
asyncio.run(load_test(50))   # Medium load
asyncio.run(load_test(100))  # Heavy load
```

### Memory Usage Test

```bash
# Monitor server memory
while true; do
    ps aux | grep "python app.py" | awk '{print $6}'
    sleep 60
done

# Should stay stable, not continuously increase
```

### WebSocket Stress Test

```javascript
// Connect many WebSocket clients
const connections = [];
for (let i = 0; i < 100; i++) {
    const sock = io('http://localhost:5000');
    sock.on('connect', () => console.log(`Client ${i} connected`));
    connections.push(sock);
}

// After 60 seconds, check all still connected
setTimeout(() => {
    const active = connections.filter(s => s.connected).length;
    console.log(`${active}/100 connections active`);
}, 60000);
```

---

## 🔒 Security Tests

### Authentication Tests

```bash
# Test accessing protected routes without auth
curl http://localhost:5000/api/player
# Expected: 401 Unauthorized

curl http://localhost:5000/api/game/start -X POST
# Expected: 401 or 403

# Test admin with wrong password
curl -X POST http://localhost:5000/api/admin_login \
  -H "Content-Type: application/json" \
  -d '{"password": "wrong"}'
# Expected: 401 Invalid password
```

### Input Validation Tests

```bash
# Test SQL injection (shouldn't affect anything but test anyway)
curl -X POST http://localhost:5000/api/login \
  -H "Content-Type: application/json" \
  -d '{"device_id": "T1234; DROP TABLE players;"}'
# Expected: Normal response (no SQL database to inject)

# Test XSS in nickname
curl -X PUT http://localhost:5000/api/player \
  -H "Content-Type: application/json" \
  -d '{"nickname": "<script>alert(1)</script>"}'
# Expected: Script tags removed

# Test extremely long input
curl -X POST http://localhost:5000/api/messages \
  -H "Content-Type: application/json" \
  -d '{"message": "'$(python -c "print('A'*10000)")'"}'
# Expected: Truncated to 500 chars
```

### File Upload Tests

```bash
# Test uploading non-image file
curl -X POST http://localhost:5000/api/player/photo \
  -F "file=@malicious.php"
# Expected: 400 Invalid file type

# Test file size limit
dd if=/dev/zero of=large.jpg bs=1M count=20
curl -X POST http://localhost:5000/api/player/photo \
  -F "file=@large.jpg"
# Expected: 413 File too large
```

---

## 🎯 Game Mode Tests

### Classic Hunt Mode
- [ ] Duration defaults to 30 min
- [ ] Capture = 100 pts
- [ ] Escape = 75 pts
- [ ] Sighting = 25 pts
- [ ] Survival bonus = 200 pts
- [ ] 3+ captures bonus = 50 pts
- [ ] 5+ captures bonus = 100 pts

### Quick Match Mode
- [ ] Duration defaults to 15 min
- [ ] Same scoring as Classic
- [ ] Survival bonus = 150 pts

### Endurance Mode
- [ ] Duration defaults to 60 min
- [ ] Same base scoring
- [ ] Survival bonus = 500 pts

### Team Competition Mode
- [ ] Predators auto-assigned to teams
- [ ] Team chat routes correctly
- [ ] Team scores track captures
- [ ] Winning team gets +300 bonus per member
- [ ] Team badges display correctly

### Infection Mode
- [ ] Captured prey become predators
- [ ] Role changes immediately
- [ ] No escape mechanic
- [ ] Game ends when all prey infected
- [ ] Last survivor gets 1000 pts
- [ ] Cannot manually change role during game
- [ ] Capture points = 50 (not 100)

### Photo Safari Mode
- [ ] Must photograph prey before capture
- [ ] Photo list tracks targets per predator
- [ ] Capture rejected without prior photo
- [ ] Photo points doubled (50 not 25)
- [ ] "Can Capture" list updates after photo

---

## 🚨 Emergency System Tests

### Trigger from Dashboard
1. Click emergency button
2. Confirmation dialog appears
3. Enter reason (optional)
4. Emergency overlay appears for ALL players
5. Game pauses
6. Admin sees emergency banner

### Trigger from Tracker
1. Hold PRG button 2 seconds
2. Screen shows "EMERGENCY?"
3. Tap 3 more times quickly
4. Server receives alert
5. All clients notified
6. Game pauses

### Clear Emergency
1. Admin clicks "Clear Emergency"
2. Emergency overlay disappears
3. Game can resume
4. Event logged

### Emergency Information Display
- [ ] Player name shown
- [ ] Reason shown
- [ ] Time shown
- [ ] Location hint shown
- [ ] Nearest players listed with RSSI

---

## ✅ Pre-Production Checklist

### Code Quality
- [ ] All Python files pass syntax check
- [ ] No console errors in web interface
- [ ] All imports resolved
- [ ] No hardcoded test values in production

### Configuration
- [ ] Admin password changed from default
- [ ] WiFi credentials updated
- [ ] Server URL correct
- [ ] Config files in .gitignore

### Security
- [ ] HTTPS configured (or local network only)
- [ ] Session secrets randomized
- [ ] File upload restrictions working
- [ ] No sensitive data in logs

### Hardware
- [ ] All trackers flashed with same firmware
- [ ] All trackers connect to WiFi
- [ ] All beacons registered in admin
- [ ] Batteries fully charged
- [ ] Antennas attached

### Documentation
- [ ] README up to date
- [ ] Setup guide accurate
- [ ] Config files documented
- [ ] Emergency procedures printed

### Backup
- [ ] Code committed to git
- [ ] Config files backed up (not in repo)
- [ ] Device IDs documented
- [ ] Recovery plan in place

---

## 🤖 Automated Test Scripts

### Python API Tests

```python
#!/usr/bin/env python3
"""
Automated API test suite for IRL Hunts
Run with: python test_api.py
"""

import requests
import json
import time

BASE_URL = "http://localhost:5000"

def test_game_endpoint():
    resp = requests.get(f"{BASE_URL}/api/game")
    assert resp.status_code == 200
    data = resp.json()
    assert "phase" in data
    assert "mode" in data
    print("✅ Game endpoint OK")

def test_player_login():
    resp = requests.post(f"{BASE_URL}/api/login", 
                        json={"device_id": "TTEST"})
    assert resp.status_code == 200
    data = resp.json()
    assert data["success"] == True
    print("✅ Player login OK")

def test_admin_login_fail():
    resp = requests.post(f"{BASE_URL}/api/admin_login",
                        json={"password": "wrongpassword"})
    assert resp.status_code == 401
    print("✅ Admin auth rejection OK")

def test_beacon_rssi_validation():
    # This requires admin session
    # Test that invalid RSSI is rejected
    pass

def test_leaderboard():
    resp = requests.get(f"{BASE_URL}/api/leaderboard")
    assert resp.status_code == 200
    data = resp.json()
    assert "preds" in data
    assert "prey" in data
    print("✅ Leaderboard endpoint OK")

if __name__ == "__main__":
    print("🧪 Running API tests...")
    test_game_endpoint()
    test_player_login()
    test_admin_login_fail()
    test_leaderboard()
    print("\n✅ All tests passed!")
```

### Arduino Compilation Test

```bash
#!/bin/bash
# Test Arduino compilation
# Requires arduino-cli installed

BOARD="esp32:esp32:heltec_wifi_lora_32_V3"

echo "🔧 Testing tracker compilation..."
arduino-cli compile --fqbn $BOARD devices/tracker/tracker.ino
if [ $? -eq 0 ]; then
    echo "✅ Tracker compiles OK"
else
    echo "❌ Tracker compilation failed"
    exit 1
fi

echo "🔧 Testing beacon compilation..."
arduino-cli compile --fqbn $BOARD devices/beacon/beacon.ino
if [ $? -eq 0 ]; then
    echo "✅ Beacon compiles OK"
else
    echo "❌ Beacon compilation failed"
    exit 1
fi

echo "✅ All firmware compiles successfully"
```

---

## 📊 Test Results Template

```markdown
# IRL Hunts Test Report

**Date:** YYYY-MM-DD
**Version:** v4.x
**Tester:** Name

## Summary
- Total Tests: X
- Passed: Y
- Failed: Z
- Skipped: W

## Environment
- Server: Python X.Y
- Browser: Chrome/Firefox/Safari
- Trackers: Heltec V3
- Network: Local WiFi

## Results

### Critical Tests
| Test | Status | Notes |
|------|--------|-------|
| Server starts | ✅/❌ | |
| Admin login | ✅/❌ | |
| Player flow | ✅/❌ | |
| Capture works | ✅/❌ | |
| Emergency system | ✅/❌ | |

### Game Modes
| Mode | Status | Notes |
|------|--------|-------|
| Classic | ✅/❌ | |
| Quick Match | ✅/❌ | |
| Endurance | ✅/❌ | |
| Team Competition | ✅/❌ | |
| Infection | ✅/❌ | |
| Photo Safari | ✅/❌ | |

### Hardware
| Device | Status | Notes |
|--------|--------|-------|
| Tracker 1 | ✅/❌ | |
| Tracker 2 | ✅/❌ | |
| Beacon 1 | ✅/❌ | |

## Issues Found
1. Issue description
2. Issue description

## Recommendations
1. Fix X before production
2. Monitor Y during game

## Sign-off
- [ ] Ready for production
- [ ] Needs fixes first
```

---

## 🎯 Quick Test Commands

```bash
# One-liner health check
curl -s http://localhost:5000/api/game | python -m json.tool

# Check all Python syntax
python -m py_compile server/*.py

# Monitor server logs
tail -f server.log

# Watch WebSocket traffic (browser)
# F12 → Network → WS tab

# Memory usage
ps aux | grep python | awk '{print $6 " KB"}'

# Port in use?
lsof -i :5000
```

---

**Remember: Test early, test often, test everything! 🧪**
