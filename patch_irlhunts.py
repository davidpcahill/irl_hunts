#!/usr/bin/env python3
"""
IRL Hunts Comprehensive Patch Script v1.0
Fixes LoRa packet parsing bug and enhances player detection

Run: python3 patch_irlhunts.py /path/to/irlhunts

This script will:
1. Fix the critical LoRa packet parsing bug in tracker.ino
2. Add new player detection alerts with prioritization
3. Update documentation (TEST.md, HARDWARE.md, GAMEPLAY.md, README.md)
4. Create backup files before patching
"""

import os
import sys
import shutil
from datetime import datetime

class Patcher:
    def __init__(self, base_path):
        self.base_path = base_path
        self.backup_dir = os.path.join(base_path, f"_backup_{datetime.now().strftime('%Y%m%d_%H%M%S')}")
        self.patched_files = []
        self.failed_files = []
        
    def backup_file(self, filepath):
        """Create backup of file before patching"""
        if not os.path.exists(self.backup_dir):
            os.makedirs(self.backup_dir)
        
        rel_path = os.path.relpath(filepath, self.base_path)
        backup_path = os.path.join(self.backup_dir, rel_path)
        backup_parent = os.path.dirname(backup_path)
        
        if not os.path.exists(backup_parent):
            os.makedirs(backup_parent)
        
        shutil.copy2(filepath, backup_path)
        return backup_path
    
    def patch_file(self, rel_path, patches):
        """Apply multiple patches to a file"""
        filepath = os.path.join(self.base_path, rel_path)
        
        if not os.path.exists(filepath):
            print(f"  ⚠️  File not found: {rel_path}")
            self.failed_files.append((rel_path, "File not found"))
            return False
        
        # Create backup
        self.backup_file(filepath)
        
        with open(filepath, 'r', encoding='utf-8', errors='ignore') as f:
            content = f.read()
        
        original = content
        patches_applied = 0
        
        for old_text, new_text in patches:
            if old_text in content:
                content = content.replace(old_text, new_text, 1)
                patches_applied += 1
            else:
                # Try to find partial match for debugging
                lines = old_text.strip().split('\n')
                if lines and lines[0].strip() in content:
                    print(f"    ⚠️  Partial match found but full pattern missing")
        
        if content != original:
            with open(filepath, 'w', encoding='utf-8') as f:
                f.write(content)
            print(f"  ✅ Patched {rel_path} ({patches_applied} changes)")
            self.patched_files.append(rel_path)
            return True
        else:
            print(f"  ⚠️  No changes made to {rel_path}")
            self.failed_files.append((rel_path, "Pattern not found"))
            return False
    
    def patch_tracker_ino(self):
        """Fix the critical LoRa packet parsing bug and add enhanced player detection"""
        print("\n📡 Patching devices/tracker/tracker.ino...")
        
        patches = [
            # FIX 1: Critical - Fix packet parsing in receivePackets()
            (
'''void receivePackets() {
  if (!rxFlag) return;
  rxFlag = false;
  String packet;
  int state = radio.readData(packet);
  if (state == RADIOLIB_ERR_NONE && packet.length() > 0) {
    float rssi = radio.getRSSI();
    int sep = packet.indexOf('|');
    if (sep > 0) {
      String id = packet.substring(0, sep);
      String type = packet.substring(sep + 1);
      if (id != NODE_ID) {
        if (type == "SAFEZONE" || type == "BEACON") {
          beaconRssi[id] = (int)rssi;
        } else if (type == "pred" || type == "prey" || type == "unknown") {
          int oldRssi = playerRssi.count(id) ? playerRssi[id] : -999;
          playerRssi[id] = (int)rssi;
          rxCount++;
          
          // Debug: Log received player beacon
          Serial.print("RX from ");
          Serial.print(id);
          Serial.print(" (");
          Serial.print(type);
          Serial.print(") RSSI: ");
          Serial.print((int)rssi);
          Serial.print("dB | Total nearby: ");
          Serial.println(playerRssi.size());
          
          // Parse consent badge if present (format: ID|role|consent)
          int sep2 = packet.indexOf('|', sep + 1);
          String otherConsent = "STD";
          if (sep2 > 0) {
            otherConsent = packet.substring(sep2 + 1);
          }
          
          // Proximity warnings for prey
          if (!emergencyActive && myRole == "prey" && type == "pred" && !inSafeZone) {
            bool approaching = (oldRssi != -999 && (int)rssi > oldRssi + 3);
            
            if (rssi > -50) {
              showNotification("CRITICAL! Predator RIGHT HERE!", "danger");
            } else if (rssi > -55) {
              showNotification("DANGER! Predator very close!", "danger");
            } else if (rssi > -65) {
              if (approaching) {
                showNotification("WARNING! Predator approaching fast!", "danger");
              } else {
                showNotification("Predator nearby", "warning");
              }
            } else if (rssi > -75 && approaching) {
              showNotification("Predator detected, moving closer", "warning");
            }
          }
        }
      }
    }
  }
  radio.startReceive();
}''',
             
'''void receivePackets() {
  if (!rxFlag) return;
  rxFlag = false;
  String packet;
  int state = radio.readData(packet);
  if (state == RADIOLIB_ERR_NONE && packet.length() > 0) {
    float rssi = radio.getRSSI();
    int sep = packet.indexOf('|');
    if (sep > 0) {
      String id = packet.substring(0, sep);
      String typeAndConsent = packet.substring(sep + 1);
      
      // CRITICAL FIX: Parse consent badge BEFORE checking type!
      // Packet format: ID|role|consent (e.g., "T1234|pred|STD")
      String type = typeAndConsent;
      String otherConsent = "STD";
      int sep2 = typeAndConsent.indexOf('|');
      if (sep2 > 0) {
        type = typeAndConsent.substring(0, sep2);  // Extract just the role
        otherConsent = typeAndConsent.substring(sep2 + 1);  // Extract consent badge
      }
      
      if (id != NODE_ID) {
        if (type == "SAFEZONE" || type == "BEACON") {
          beaconRssi[id] = (int)rssi;
        } else if (type == "pred" || type == "prey" || type == "unknown") {
          bool isNewPlayer = (playerRssi.count(id) == 0);
          int oldRssi = isNewPlayer ? -999 : playerRssi[id];
          playerRssi[id] = (int)rssi;
          rxCount++;
          
          // Debug: Log received player beacon
          Serial.print("RX from ");
          Serial.print(id);
          Serial.print(" (");
          Serial.print(type);
          Serial.print(") RSSI: ");
          Serial.print((int)rssi);
          Serial.print("dB | Total nearby: ");
          Serial.println(playerRssi.size());
          
          // NEW PLAYER DETECTION - Priority alerts!
          if (isNewPlayer && !emergencyActive && gamePhase == "running") {
            if (myRole == "prey" && type == "pred") {
              showNotification("⚠️ NEW PREDATOR DETECTED! " + id, "danger");
            } else if (myRole == "pred" && type == "prey") {
              showNotification("🎯 NEW PREY SPOTTED! " + id, "success");
            }
          }
          
          // Proximity warnings for prey (enhanced with player count)
          if (!emergencyActive && myRole == "prey" && type == "pred" && !inSafeZone) {
            bool approaching = (oldRssi != -999 && (int)rssi > oldRssi + 3);
            int predCount = 0;
            for (auto& p : playerRssi) {
              // Count predators (we can't know role from RSSI map alone, but we can warn about multiple signals)
              if (p.second > -80) predCount++;
            }
            
            if (rssi > -50) {
              showNotification("🚨 CRITICAL! Predator RIGHT HERE!", "danger");
            } else if (rssi > -55) {
              if (predCount > 1) {
                showNotification("🚨 DANGER! Multiple threats very close!", "danger");
              } else {
                showNotification("🚨 DANGER! Predator very close!", "danger");
              }
            } else if (rssi > -65) {
              if (approaching) {
                showNotification("⚠️ Predator approaching fast!", "danger");
              } else {
                showNotification("⚠️ Predator nearby (" + String(playerRssi.size()) + " total)", "warning");
              }
            } else if (rssi > -75 && approaching) {
              showNotification("📡 Predator detected, moving closer", "warning");
            }
          }
          
          // Alert predators about multiple prey
          if (!emergencyActive && myRole == "pred" && type == "prey" && !isNewPlayer) {
            if (rssi > -60 && oldRssi < -70) {
              showNotification("🎯 Prey " + id + " now in range!", "success");
            }
          }
        }
      }
    }
  }
  radio.startReceive();
}'''
            ),
            
            # FIX 2: Enhance display to show multiple players better
            (
'''  String line5 = "";
  if (playerRssi.size() == 0) line5 = "No players nearby";
  else {
    int strongest = -999; String strongestId = "";
    for (auto& p : playerRssi) {
      if (p.second > strongest) { strongest = p.second; strongestId = p.first; }
    }
    line5 = (myRole == "pred" ? "Target: " : "Threat: ") + strongestId + " (" + String(strongest) + "dB)";
  }
  drawScrollingText(48, line5);''',
             
'''  String line5 = "";
  if (playerRssi.size() == 0) line5 = "No players nearby";
  else if (playerRssi.size() == 1) {
    int strongest = -999; String strongestId = "";
    for (auto& p : playerRssi) {
      strongest = p.second; strongestId = p.first;
    }
    line5 = (myRole == "pred" ? "Target: " : "Threat: ") + strongestId + " (" + String(strongest) + "dB)";
  } else {
    // Multiple players - show count and closest
    int strongest = -999; String strongestId = "";
    for (auto& p : playerRssi) {
      if (p.second > strongest) { strongest = p.second; strongestId = p.first; }
    }
    String prefix = (myRole == "pred" ? "🎯 " : "⚠️ ");
    line5 = prefix + String(playerRssi.size()) + " nearby | Best: " + strongestId + " " + String(strongest) + "dB";
  }
  drawScrollingText(48, line5);'''
            ),
        ]
        
        return self.patch_file("devices/tracker/tracker.ino", patches)
    
    def patch_test_md(self):
        """Update TEST.md with LoRa troubleshooting section"""
        print("\n📄 Patching docs/TEST.md...")
        
        patches = [
            (
'''**Debug Steps:**
```
1. Open Serial Monitor (115200 baud)
2. Look for: "TX #1: TXXXX|prey" (sending beacons)
3. Look for: "RX from TXXXX" (receiving packets)
4. If TX but no RX: other device not transmitting or out of range
5. If no TX: LoRa init failed, check error code
```''',
             
'''**Debug Steps:**
```
1. Open Serial Monitor (115200 baud)
2. Look for: "TX #1: TXXXX|prey|STD" (sending beacons with consent badge)
3. Look for: "RX from TXXXX (prey) RSSI: -XXdB | Total nearby: 1" (receiving packets)
4. If TX but no RX: other device not transmitting or out of range
5. If no TX: LoRa init failed, check error code
6. Verify packet format is ID|role|consent (3 parts separated by |)
7. Check "Total nearby" count increments when receiving valid packets
```

**Common LoRa Issues:**
- "No players nearby" despite devices being close: Check packet parsing (consent badge must be parsed before role check)
- Weak RSSI (<-90 dB) at short range: Verify antennas are connected
- Intermittent detection: Check for WiFi interference or low battery'''
            ),
        ]
        
        return self.patch_file("docs/TEST.md", patches)
    
    def patch_hardware_md(self):
        """Update HARDWARE.md with packet format documentation"""
        print("\n📄 Patching docs/HARDWARE.md...")
        
        patches = [
            (
'''### LoRa Communication
- **Frequency:** 915 MHz (US) or 868 MHz (EU)
- **Range:** Up to 1km line-of-sight
- **Data:** Player ID + role broadcast
- **RSSI:** Signal strength for proximity''',
             
'''### LoRa Communication
- **Frequency:** 915 MHz (US) or 868 MHz (EU)
- **Range:** Up to 1km line-of-sight
- **Data:** Player ID + role + consent broadcast
- **RSSI:** Signal strength for proximity detection

### Packet Format
```
ID|role|consent
```
**Examples:**
- `T9EF0|pred|STD` - Predator, standard consent settings
- `TA2B3|prey|T` - Prey, physical contact OK
- `T1234|unknown|NP` - Unassigned, no photos preference

**Parts:**
- **ID**: 5-character device identifier (e.g., T9EF0)
- **Role**: `pred`, `prey`, or `unknown`
- **Consent**: `STD` (standard), `T` (touch OK), `NP` (no photos), `NL` (no location), or combinations'''
            ),
        ]
        
        return self.patch_file("docs/HARDWARE.md", patches)
    
    def patch_gameplay_md(self):
        """Update GAMEPLAY.md with consent badge broadcast info"""
        print("\n📄 Patching docs/GAMEPLAY.md...")
        
        patches = [
            (
'''Consent badges appear on tracker displays (in square brackets):
- `[STD]` = Standard (default settings - photo and location ON, physical OFF)
- `[T]` = Touch/Physical tag OK (has enabled physical contact)
- `[NP]` = No Photos please (has disabled photo visibility)
- `[NL]` = No Location sharing (has disabled location hints)
- Badges combine: `[TNP]` = Touch OK but No Photos''',
             
'''Consent badges appear on tracker displays (in square brackets) and are broadcast via LoRa:
- `[STD]` = Standard (default settings - photo and location ON, physical OFF)
- `[T]` = Touch/Physical tag OK (has enabled physical contact)
- `[NP]` = No Photos please (has disabled photo visibility)
- `[NL]` = No Location sharing (has disabled location hints)
- Badges combine: `[TNP]` = Touch OK but No Photos

**LoRa Broadcast Format:** `DeviceID|Role|ConsentBadge`
Examples:
- `T9EF0|pred|T` = Tracker T9EF0, predator role, physical contact OK
- `TA2B3|prey|STD` = Tracker TA2B3, prey role, standard settings

This allows nearby players to see consent preferences before interaction.'''
            ),
        ]
        
        return self.patch_file("docs/GAMEPLAY.md", patches)
    
    def patch_readme_md(self):
        """Update README.md with LoRa troubleshooting"""
        print("\n📄 Patching README.md...")
        
        patches = [
            (
'''- Serial Monitor (115200 baud) for device debug
- Browser console (F12) for web issues''',
             
'''- Serial Monitor (115200 baud) for device debug
- Browser console (F12) for web issues
- LoRa packet format: `ID|role|consent` (e.g., `T9EF0|pred|STD`)

### Common LoRa Issues
If trackers show "No players nearby" when devices are close:
1. Open Serial Monitor (115200 baud) on both devices
2. Verify TX messages: `TX #1: T1234|pred|STD` (3 parts with `|`)
3. Verify RX messages: `RX from T5678 (prey) RSSI: -45dB | Total nearby: 1`
4. Check frequencies match (915.0 MHz for Americas)
5. Ensure antennas are connected securely'''
            ),
        ]
        
        return self.patch_file("README.md", patches)
    
    def run_all_patches(self):
        """Run all patches"""
        print("=" * 60)
        print("🔧 IRL Hunts Comprehensive Patch Script v1.0")
        print("=" * 60)
        print(f"Base path: {self.base_path}")
        print(f"Backup dir: {self.backup_dir}")
        
        # Verify base path
        tracker_path = os.path.join(self.base_path, "devices/tracker/tracker.ino")
        if not os.path.exists(tracker_path):
            print(f"\n❌ ERROR: Cannot find tracker.ino at expected location:")
            print(f"   {tracker_path}")
            print("\nPlease provide the correct path to your irlhunts directory.")
            return False
        
        # Run patches
        self.patch_tracker_ino()
        self.patch_test_md()
        self.patch_hardware_md()
        self.patch_gameplay_md()
        self.patch_readme_md()
        
        # Summary
        print("\n" + "=" * 60)
        print("📊 PATCH SUMMARY")
        print("=" * 60)
        
        if self.patched_files:
            print(f"\n✅ Successfully patched ({len(self.patched_files)} files):")
            for f in self.patched_files:
                print(f"   - {f}")
        
        if self.failed_files:
            print(f"\n⚠️  Failed to patch ({len(self.failed_files)} files):")
            for f, reason in self.failed_files:
                print(f"   - {f}: {reason}")
        
        print(f"\n📁 Backup created at: {self.backup_dir}")
        
        if "devices/tracker/tracker.ino" in self.patched_files:
            print("\n" + "=" * 60)
            print("⚠️  IMPORTANT: REFLASH YOUR TRACKERS!")
            print("=" * 60)
            print("The tracker firmware has been updated. You MUST:")
            print("1. Open devices/tracker/tracker.ino in Arduino IDE")
            print("2. Flash to ALL tracker devices")
            print("3. Verify fix by checking Serial Monitor for:")
            print('   "RX from TXXXX (prey) RSSI: -XXdB | Total nearby: 1"')
            print("\nNew features after flashing:")
            print("- 🎯 NEW PREY SPOTTED! alerts for predators")
            print("- ⚠️ NEW PREDATOR DETECTED! alerts for prey")
            print("- Multiple player count display (e.g., '3 nearby | Best: T1234 -65dB')")
            print("- Enhanced proximity warnings with emoji indicators")
        
        print("\n" + "=" * 60)
        print("✅ PATCH COMPLETE!")
        print("=" * 60)
        
        return len(self.failed_files) == 0 or "devices/tracker/tracker.ino" in self.patched_files


def main():
    if len(sys.argv) < 2:
        # Try to find irlhunts directory
        possible_paths = [
            ".",
            "./irlhunts",
            "../irlhunts",
            os.path.expanduser("~/irlhunts"),
            "/home/claude/irlhunts",
        ]
        
        base_path = None
        for path in possible_paths:
            if os.path.exists(os.path.join(path, "devices/tracker/tracker.ino")):
                base_path = path
                break
        
        if not base_path:
            print("Usage: python3 patch_irlhunts.py /path/to/irlhunts")
            print("\nCannot find irlhunts directory automatically.")
            print("Please provide the path to your irlhunts project root.")
            sys.exit(1)
    else:
        base_path = sys.argv[1]
    
    base_path = os.path.abspath(base_path)
    patcher = Patcher(base_path)
    success = patcher.run_all_patches()
    
    sys.exit(0 if success else 1)


if __name__ == "__main__":
    main()
