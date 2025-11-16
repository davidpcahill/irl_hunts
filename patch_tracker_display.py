#!/usr/bin/env python3
"""
IRL Hunts - Tracker Display Fix
Run this script from the root of your irlhunts project directory.

Fixes:
1. Server not returning consent_badge in ping response
2. ArduinoJson converting null to "null" string
3. Consent badge not always displaying
"""

import os
import sys

def check_project_structure():
    """Verify we're in the right directory"""
    required = ['server/app.py', 'devices/tracker/tracker.ino', 'docs/GAMEPLAY.md']
    missing = [f for f in required if not os.path.exists(f)]
    if missing:
        print("ERROR: Missing required files:")
        for f in missing:
            print(f"  - {f}")
        print("\nMake sure you run this script from the irlhunts root directory.")
        sys.exit(1)
    print("✅ Project structure verified")

def patch_server():
    """Add consent_badge calculation to tracker ping response"""
    print("\nPatching server/app.py...")
    
    with open('server/app.py', 'r') as f:
        content = f.read()
    
    # Find and replace the ping return section
    old = '''    mode_config = get_mode_config()
    
    return jsonify({
        "phase": game["phase"],
        "status": player["status"],
        "role": player["role"],
        "name": player["name"],
        "in_safe_zone": player["in_safe_zone"],
        "team": player.get("team"),
        "notifications": notifs,
        "settings": game["settings"],
        "beacons": list(beacons.keys()),
        "game_mode": game["mode"],
        "game_mode_name": mode_config["name"],
        "emergency": game["emergency"],
        "emergency_by": game["emergency_by"],
        "infection_mode": mode_config["infection"],
        "photo_required": mode_config["photo_required"],
        "has_photo_of": player.get("has_photo_of", []),
        "consent_flags": player.get("consent_flags", {}),
        "ready": player.get("ready", False)
    })'''
    
    new = '''    mode_config = get_mode_config()
    
    # Calculate consent badge for tracker display
    consent_flags = player.get("consent_flags", {})
    badge_parts = []
    if consent_flags.get("physical_tag", False):
        badge_parts.append("T")
    if not consent_flags.get("photo_visible", True):
        badge_parts.append("NP")
    if not consent_flags.get("location_share", True):
        badge_parts.append("NL")
    consent_badge = "".join(badge_parts) if badge_parts else "STD"
    
    # Return None for empty team (avoids "null" string in ArduinoJson)
    team_value = player.get("team") if player.get("team") else None
    
    return jsonify({
        "phase": game["phase"],
        "status": player["status"],
        "role": player["role"],
        "name": player["name"],
        "in_safe_zone": player["in_safe_zone"],
        "team": team_value,
        "notifications": notifs,
        "settings": game["settings"],
        "beacons": list(beacons.keys()),
        "game_mode": game["mode"],
        "game_mode_name": mode_config["name"],
        "emergency": game["emergency"],
        "emergency_by": game["emergency_by"],
        "infection_mode": mode_config["infection"],
        "photo_required": mode_config["photo_required"],
        "has_photo_of": player.get("has_photo_of", []),
        "consent_flags": consent_flags,
        "consent_badge": consent_badge,
        "consent_physical": consent_flags.get("physical_tag", False),
        "consent_photo": consent_flags.get("photo_visible", True),
        "ready": player.get("ready", False)
    })'''
    
    if old in content:
        content = content.replace(old, new)
        with open('server/app.py', 'w') as f:
            f.write(content)
        print("  ✅ Added consent_badge calculation to /api/tracker/ping")
        return True
    else:
        print("  ⚠️  Could not find exact match - may already be patched")
        return False

def patch_tracker():
    """Fix null JSON handling in tracker firmware"""
    print("\nPatching devices/tracker/tracker.ino...")
    
    with open('devices/tracker/tracker.ino', 'r') as f:
        content = f.read()
    
    changes = 0
    
    # Patch 1: Team null handling
    old_team = '      String newTeam = respDoc["team"].as<String>();'
    new_team = '''      // Handle team - check for null before converting to string
      String newTeam = "";
      if (!respDoc["team"].isNull()) {
        newTeam = respDoc["team"].as<String>();
      }'''
    
    if old_team in content:
        content = content.replace(old_team, new_team)
        print("  ✅ Fixed team null handling")
        changes += 1
    
    # Patch 2: Consent badge null handling
    old_badge = '      String newConsentBadge = respDoc["consent_badge"].as<String>();'
    new_badge = '''      // Handle consent_badge - check for null and provide default
      String newConsentBadge = "STD";
      if (!respDoc["consent_badge"].isNull()) {
        newConsentBadge = respDoc["consent_badge"].as<String>();
      }'''
    
    if old_badge in content:
        content = content.replace(old_badge, new_badge)
        print("  ✅ Fixed consent_badge null handling")
        changes += 1
    
    # Patch 3: Boolean defaults
    old_bool = '''      bool newConsentPhysical = respDoc["consent_physical"];
      bool newConsentPhoto = respDoc["consent_photo"];'''
    new_bool = '''      bool newConsentPhysical = respDoc["consent_physical"] | false;
      bool newConsentPhoto = respDoc["consent_photo"] | true;'''
    
    if old_bool in content:
        content = content.replace(old_bool, new_bool)
        print("  ✅ Added default values for consent booleans")
        changes += 1
    
    # Patch 4: Ready boolean default
    old_ready = '      bool newReady = respDoc["ready"];'
    new_ready = '      bool newReady = respDoc["ready"] | false;'
    
    if old_ready in content:
        content = content.replace(old_ready, new_ready)
        print("  ✅ Added default value for ready boolean")
        changes += 1
    
    # Patch 5: Team display filtering
    old_team_display = '  if (myTeam.length() > 0) statusLine += " " + myTeam.substring(0, 1);'
    new_team_display = '  if (myTeam.length() > 0 && myTeam != "null") statusLine += " " + myTeam.substring(0, 1);'
    
    if old_team_display in content:
        content = content.replace(old_team_display, new_team_display)
        print("  ✅ Fixed team display to ignore 'null' string")
        changes += 1
    
    # Patch 6: Always show consent badge
    old_badge_display = '''  // Show consent badge indicator
  if (consentBadge != "STD" && consentBadge.length() > 0) {
    statusLine += " [" + consentBadge + "]";
  }'''
    new_badge_display = '''  // Show consent badge indicator (always show for clarity)
  if (consentBadge.length() > 0) {
    statusLine += " [" + consentBadge + "]";
  }'''
    
    if old_badge_display in content:
        content = content.replace(old_badge_display, new_badge_display)
        print("  ✅ Changed consent badge to always display")
        changes += 1
    
    if changes > 0:
        with open('devices/tracker/tracker.ino', 'w') as f:
            f.write(content)
        print(f"  Applied {changes} patches to tracker firmware")
        return True
    else:
        print("  ⚠️  No patches applied - may already be fixed")
        return False

def update_docs():
    """Update documentation with clearer consent badge info"""
    print("\nUpdating docs/GAMEPLAY.md...")
    
    with open('docs/GAMEPLAY.md', 'r') as f:
        content = f.read()
    
    old_badges = '''Consent badges appear on tracker displays:
- `STD` = Standard (default settings)
- `T` = Touch/Physical tag OK
- `NP` = No Photos please
- `NL` = No Location sharing'''
    
    new_badges = '''Consent badges appear on tracker displays (in square brackets):
- `[STD]` = Standard (default settings - photo and location ON, physical OFF)
- `[T]` = Touch/Physical tag OK (has enabled physical contact)
- `[NP]` = No Photos please (has disabled photo visibility)
- `[NL]` = No Location sharing (has disabled location hints)
- Badges combine: `[TNP]` = Touch OK but No Photos'''
    
    if old_badges in content:
        content = content.replace(old_badges, new_badges)
        with open('docs/GAMEPLAY.md', 'w') as f:
            f.write(content)
        print("  ✅ Updated consent badge documentation")
        return True
    else:
        print("  ⚠️  Documentation may already be updated")
        return False

def main():
    print("=" * 60)
    print("  IRL Hunts - Tracker Display Fix")
    print("=" * 60)
    
    check_project_structure()
    
    server_patched = patch_server()
    tracker_patched = patch_tracker()
    docs_updated = update_docs()
    
    print("\n" + "=" * 60)
    print("  PATCH SUMMARY")
    print("=" * 60)
    
    if server_patched or tracker_patched or docs_updated:
        print("✅ Patches applied successfully!")
        print("\nNext steps:")
        print("1. Restart the server:")
        print("   cd server && python app.py")
        print("\n2. Re-flash tracker devices with updated firmware:")
        print("   - Open devices/tracker/tracker.ino in Arduino IDE")
        print("   - Upload to all tracker devices")
        print("\n3. Test the display shows:")
        print("   Name [W][S] [STD] R")
        print("   or [T] when physical consent is enabled")
    else:
        print("⚠️  No changes made - files may already be patched")
    
    print("\n" + "=" * 60)

if __name__ == "__main__":
    main()
