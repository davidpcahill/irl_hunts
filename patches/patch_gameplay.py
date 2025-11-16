#!/usr/bin/env python3
"""
Patch script for docs/GAMEPLAY.md
Fixes duplicate content in consent and safety sections.

Apply with: python3 patch_gameplay.py path/to/GAMEPLAY.md
"""

import sys

def patch_gameplay(filepath):
    with open(filepath, 'r') as f:
        content = f.read()
    
    changes_made = 0
    
    # Fix 1: Remove duplicate consent reminders in Social Safety section
    old_duplicate = '''### Social Safety
- Share game plans with non-players
- Have emergency contacts
- Buddy system recommended
- Report inappropriate behavior
- **Respect ALL consent settings**
- Review and set your consent preferences before playing
- **Respect ALL consent settings**
- Review and set your consent preferences before playing'''
    
    new_social = '''### Social Safety
- Share game plans with non-players
- Have emergency contacts
- Buddy system recommended
- Report inappropriate behavior
- **Respect ALL consent settings**
- Review and set your consent preferences before playing'''
    
    if old_duplicate in content:
        content = content.replace(old_duplicate, new_social)
        print("✅ Fixed duplicate consent reminders in Social Safety")
        changes_made += 1
    
    if changes_made == 0:
        print("⚠️  No duplicate patterns found (may already be fixed)")
    
    # Write the fixed file
    with open(filepath, 'w') as f:
        f.write(content)
    
    print(f"✅ Patched {filepath} ({changes_made} fixes applied)")

if __name__ == "__main__":
    if len(sys.argv) != 2:
        print("Usage: python3 patch_gameplay.py <path/to/GAMEPLAY.md>")
        sys.exit(1)
    
    patch_gameplay(sys.argv[1])
