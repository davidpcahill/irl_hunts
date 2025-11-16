#!/usr/bin/env python3
"""
Patch script for server/app.py
Fixes duplicate consent handling code.

Apply with: python3 patch_app.py path/to/app.py
"""

import sys

def patch_app(filepath):
    with open(filepath, 'r') as f:
        content = f.read()
    
    changes_made = 0
    
    # Fix 1: Remove duplicate consent_flags handling in api_update_player
    old_consent = '''    # Update consent flags if provided
    if "consent_flags" in data and isinstance(data["consent_flags"], dict):
        for key in ["physical_tag", "photo_visible", "location_share"]:
            if key in data["consent_flags"]:
                player["consent_flags"][key] = bool(data["consent_flags"][key])
        log_event("consent_update", {"player": player["name"], "flags": player["consent_flags"]})
    
    # Update consent flags if provided
    if "consent_flags" in data and isinstance(data["consent_flags"], dict):
        for key in ["physical_tag", "photo_visible", "location_share"]:
            if key in data["consent_flags"]:
                player["consent_flags"][key] = bool(data["consent_flags"][key])
        log_event("consent_update", {"player": player["name"], "flags": player["consent_flags"]})'''
    
    new_consent = '''    # Update consent flags if provided
    if "consent_flags" in data and isinstance(data["consent_flags"], dict):
        for key in ["physical_tag", "photo_visible", "location_share"]:
            if key in data["consent_flags"]:
                player["consent_flags"][key] = bool(data["consent_flags"][key])
        log_event("consent_update", {"player": player["name"], "flags": player["consent_flags"]})'''
    
    if old_consent in content:
        content = content.replace(old_consent, new_consent)
        print("✅ Fixed duplicate consent_flags handling in api_update_player()")
        changes_made += 1
    
    # Fix 2: Remove duplicate consent check in api_upload_sighting
    old_sighting_consent = '''    # Check consent for photography
    if not target.get("consent_flags", {}).get("photo_visible", True):
        return jsonify({"error": f"{target['name']} has disabled photo visibility"}), 403
    
    # Check consent for photography
    if not target.get("consent_flags", {}).get("photo_visible", True):
        return jsonify({"error": f"{target['name']} has disabled photo visibility"}), 403'''
    
    new_sighting_consent = '''    # Check consent for photography
    if not target.get("consent_flags", {}).get("photo_visible", True):
        return jsonify({"error": f"{target['name']} has disabled photo visibility"}), 403'''
    
    if old_sighting_consent in content:
        content = content.replace(old_sighting_consent, new_sighting_consent)
        print("✅ Fixed duplicate consent check in api_upload_sighting()")
        changes_made += 1
    
    if changes_made == 0:
        print("⚠️  No duplicate patterns found (may already be fixed)")
    
    # Write the fixed file
    with open(filepath, 'w') as f:
        f.write(content)
    
    print(f"✅ Patched {filepath} ({changes_made} fixes applied)")

if __name__ == "__main__":
    if len(sys.argv) != 2:
        print("Usage: python3 patch_app.py <path/to/app.py>")
        sys.exit(1)
    
    patch_app(sys.argv[1])
