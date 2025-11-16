# 🤔 IRL Hunts - Frequently Asked Questions

## General Questions

### What is IRL Hunts?
IRL Hunts is a real-world game where players use wireless trackers to play predator vs. prey. Think of it like a high-tech game of tag or hide-and-seek, but with proximity-based capture detection using LoRa radio technology.

### How many players do I need?
- **Minimum:** 2 players (1 predator, 1 prey)
- **Recommended:** 5-10 players for a balanced game
- **Maximum:** Limited by hardware (typically 20-30 with standard server)

### Do I need internet/cellular service?
- **Trackers:** Need WiFi connection to the game server
- **Server:** Needs to be on same WiFi network as trackers
- **Player phones:** Need to reach server (same WiFi or local network)
- **LoRa:** Works without internet - direct radio communication

### How far apart can players be?
LoRa radio range varies by environment:
- **Open field:** 1-2 km
- **Urban area:** 200-500m
- **Indoor:** 50-100m
- **Dense buildings:** 20-50m

---

## Hardware Questions

### What hardware do I need?
- **Per player:** Heltec WiFi LoRa 32 V3 (~$20-25)
- **Per safe zone:** Same Heltec device as beacon
- **Server:** Any laptop/computer with Python 3.8+

### Where can I buy the trackers?
- Official Heltec store: https://heltec.org/
- Amazon, AliExpress, Banggood
- Search for "Heltec WiFi LoRa 32 V3"

### How long does the battery last?
With 1000mAh battery:
- **Tracker:** 4-6 hours active gameplay
- **Beacon:** 8-10 hours (less power usage)

With 2000mAh battery:
- **Tracker:** 8-12 hours
- **Beacon:** 16-20 hours

### Why isn't my tracker connecting to WiFi?
1. Check SSID/password in config_local.h (case-sensitive!)
2. Ensure it's a 2.4GHz network (not 5GHz)
3. Router might have too many devices connected
4. Try moving closer to router during initial connection

### Why can't trackers see each other?
1. **Frequency mismatch:** All devices must use same LoRa frequency
2. **Antenna:** Make sure antenna is connected securely
3. **Distance:** Try testing closer together first
4. **Settings:** LoRa parameters (SF, BW, sync word) must match exactly

Check Serial Monitor (115200 baud) for debug output.

---

## Gameplay Questions

### How does capture work?
1. Predator gets close to prey (within RSSI threshold)
2. Predator's tracker detects strong prey signal
3. Predator presses button to attempt capture
4. Server validates: proximity, game running, prey not in safe zone
5. If valid, prey is captured!

### What's RSSI and what do the numbers mean?
RSSI = Received Signal Strength Indicator (measured in dBm)

| RSSI | Approximate Distance | Can Capture? |
|------|---------------------|--------------|
| -30 to -50 | 1-3 meters | ✅ Yes |
| -50 to -65 | 3-10 meters | ✅ Likely |
| -65 to -75 | 10-20 meters | ⚠️ Maybe |
| -75 to -90 | 20+ meters | ❌ No |

**Note:** Higher RSSI (less negative) = closer proximity

### How do I escape when captured?
1. You must physically walk to a safe zone beacon
2. When close enough (RSSI above threshold), your tracker detects it
3. You're automatically freed and get escape points!
4. In Infection mode: NO ESCAPING - you become a predator!

### What's the difference between game modes?

| Mode | Special Rules | Best For |
|------|--------------|----------|
| Classic | Standard rules, balanced scoring | Most games |
| Quick Match | Shorter time, fast-paced | Quick rounds |
| Endurance | Long game, high survival bonus | Experienced players |
| Team Competition | Predators on teams, team scoring | Large groups |
| Infection | Captured prey become predators | Exciting finale |
| Photo Safari | Must photograph before capture | Strategy/proof |

### Can I change roles during the game?
- **In lobby:** Yes, freely
- **During game:** Only if "Role Change in Safe Zone" is enabled AND you're in a safe zone
- **Infection mode:** No - once infected, you're permanently a predator

### What are consent settings?
Players can control:
- **Physical Contact:** Allow shoulder tap for capture verification
- **Photography:** Can be photographed for sightings
- **Location Sharing:** Share location hints with others

These are displayed on tracker OLEDs and respected by other players.

---

## Technical Questions

### How do I find my server IP address?
**Windows:** Run `ipconfig` in Command Prompt, look for "IPv4 Address"
**Mac/Linux:** Run `ifconfig` or `ip addr`, look for local IP (192.168.x.x)

### Why do I get "Session Conflict" in the dashboard?
This happens when multiple players log in from the same browser context. Solutions:
- Use different browsers (Chrome for Player 1, Firefox for Player 2)
- Use separate private windows (not tabs in same window!)
- Use different devices (phones/tablets)

### How do I add a safe zone beacon?
1. Flash beacon firmware to Heltec device
2. Power it on and note the ID shown on OLED (e.g., SZ1A2B)
3. In Admin Panel, go to "Safe Zone Beacons"
4. Enter the beacon ID and choose name/RSSI threshold
5. Click "Add Safe Zone"

### What RSSI threshold should I set for beacons?
- **-60:** Must be very close (< 5 meters)
- **-75:** Standard range (~10-15 meters) - RECOMMENDED
- **-85:** Extended range (~20-30 meters)

### How do I update/flash the tracker firmware?
1. Connect tracker via USB-C
2. Open tracker.ino in Arduino IDE
3. Select board: "Heltec WiFi LoRa 32(V3)"
4. Select correct COM port
5. Click Upload
6. Monitor Serial output (115200 baud)

### Where are photos stored?
Photos are stored in `server/uploads/` folder:
- Profile pics: `profile_DEVICEID_timestamp.jpg`
- Sightings: `sighting_SPOTTER_TARGET_uuid.jpg`

Use the cleanup script or Admin Panel to clear old photos.

---

## Troubleshooting

### "No players nearby" on tracker
1. Check LoRa frequency matches on all devices
2. Verify antennas are connected
3. Check Serial Monitor for TX/RX counts
4. Ensure devices are within range
5. Confirm LoRa parameters match (SF, BW, sync word)

### Server won't start
1. Check Python 3.8+ is installed: `python --version`
2. Install dependencies: `pip install -r requirements.txt`
3. Check for port conflicts: `lsof -i :5000`
4. Verify config files exist (config_local.py)

### Tracker shows "Server Offline"
1. Verify server is running
2. Check SERVER_URL in config_local.h is correct IP
3. Ensure both on same WiFi network
4. Check firewall isn't blocking port 5000
5. Try pinging server from another device

### Capture attempts fail
1. Game must be in "running" phase
2. Prey must not be in safe zone
3. RSSI must exceed capture threshold
4. Predator can't capture while in safe zone
5. Check cooldown timer (10 seconds between attempts)

### Emergency system not working
1. Ensure game server is running
2. Check WebSocket connection (should show "Connected")
3. Verify admin can clear emergencies in Admin Panel
4. Check event log for emergency events

---

## Safety & Best Practices

### Physical Safety
- ✅ Set clear play area boundaries
- ✅ Brief all players on rules before game
- ✅ Have emergency contacts ready
- ✅ Stay hydrated and take breaks
- ✅ Watch for obstacles while moving
- ❌ Don't run into roads or dangerous areas
- ❌ Don't ignore weather conditions

### Digital Safety
- ✅ Change default admin password
- ✅ Use config_local.py for credentials
- ✅ Keep credentials out of git
- ✅ Monitor server logs for issues
- ❌ Don't share WiFi credentials publicly

### Social Safety
- ✅ Respect all consent settings
- ✅ Report inappropriate behavior
- ✅ Use emergency system for real emergencies
- ✅ Include players of all skill levels
- ❌ Don't violate physical contact preferences

---

## Getting Help

### Logs and Debugging
- **Server:** Check terminal output or game.log
- **Tracker:** Open Serial Monitor (115200 baud)
- **Web:** Open browser console (F12)
- **Events:** Check Admin Panel event log

### Where to Report Issues
1. Check this FAQ first
2. Review error messages in logs
3. Open GitHub issue with:
   - What you expected
   - What actually happened
   - Relevant log output
   - Hardware/software versions

---

**Still have questions?** Check the full documentation in the `docs/` folder or open an issue on GitHub!
