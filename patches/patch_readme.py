#!/usr/bin/env python3
"""
Patch script for README.md
Adds consent system to gameplay features and updates safety notice.

Apply with: python3 patch_readme.py path/to/README.md
"""

import sys

def patch_readme(filepath):
    with open(filepath, 'r') as f:
        content = f.read()
    
    changes_made = 0
    
    # Fix 1: Add consent system to gameplay features
    old_gameplay = '''### 🎮 Gameplay
- **Real proximity-based capture** - No GPS required, works indoors and outdoors
- **Automatic safe zone detection** - Physical beacon devices create sanctuary areas
- **Photo sightings** - Earn points by photographing the opposing team
- **Live leaderboards** - Rankings update in real-time
- **Team chat** - Coordinate strategies with your team
- **Custom profiles** - Nicknames and profile pictures'''
    
    new_gameplay = '''### 🎮 Gameplay
- **Real proximity-based capture** - No GPS required, works indoors and outdoors
- **Automatic safe zone detection** - Physical beacon devices create sanctuary areas
- **Photo sightings** - Earn points by photographing the opposing team
- **Live leaderboards** - Rankings update in real-time
- **Team chat** - Coordinate strategies with your team
- **Custom profiles** - Nicknames and profile pictures
- **Consent system** - Players control physical contact, photography, and location sharing preferences'''
    
    if old_gameplay in content:
        content = content.replace(old_gameplay, new_gameplay)
        print("✅ Added consent system to gameplay features")
        changes_made += 1
    
    # Fix 2: Update safety notice to emphasize consent
    old_safety = '''## ⚠️ Safety Notice

**Always prioritize safety:**
- **Respect consent settings** - Check for 🤝 badge before physical contact
- Share game plans with someone not playing
- Stay aware of surroundings
- Set boundaries for play area
- Have emergency contacts
- Use emergency button if needed
- Stay hydrated
- Watch for obstacles
- **No non-consensual contact** - Only touch players who have opted in'''
    
    new_safety = '''## ⚠️ Safety Notice

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
- **Consent is mandatory** - Violating consent settings may result in removal'''
    
    if old_safety in content:
        content = content.replace(old_safety, new_safety)
        print("✅ Updated safety notice with consent details")
        changes_made += 1
    
    if changes_made == 0:
        print("⚠️  No patterns found to update (may already be current)")
    
    # Write the fixed file
    with open(filepath, 'w') as f:
        f.write(content)
    
    print(f"✅ Patched {filepath} ({changes_made} fixes applied)")

if __name__ == "__main__":
    if len(sys.argv) != 2:
        print("Usage: python3 patch_readme.py <path/to/README.md>")
        sys.exit(1)
    
    patch_readme(sys.argv[1])
