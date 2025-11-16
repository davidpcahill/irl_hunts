#!/usr/bin/env python3
"""
Patch script for README.md
Updates the "What's Included" section to show accurate file structure.

Apply with: python3 patch_readme_structure.py path/to/README.md
"""

import sys

def patch_readme_structure(filepath):
    with open(filepath, 'r') as f:
        content = f.read()
    
    # Find and replace the What's Included section
    old_structure = '''## 📦 What's Included

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
```'''
    
    new_structure = '''## 📦 What's Included

```
irlhunts/
├── README.md                    # This file
├── LICENSE                      # MIT License
├── .gitignore                   # Git ignore rules
├── TEST.md                      # Testing procedures
├── server/                      # Web server application
│   ├── app.py                   # Main Flask server
│   ├── config.py                # Default configuration
│   ├── config_local.py.example  # Local config template
│   ├── requirements.txt         # Python dependencies
│   ├── uploads/                 # User-uploaded photos (gitignored contents)
│   └── templates/               # Web interface
│       ├── login.html           # Player/admin login
│       ├── dashboard.html       # Player game interface
│       └── admin.html           # Admin control panel
├── devices/                     # Arduino device code
│   ├── tracker/                 # Player tracker
│   │   ├── tracker.ino          # Main tracker firmware
│   │   ├── config.h             # Default device config
│   │   └── config_local.h.example  # Local config template
│   └── beacon/                  # Safe zone beacon
│       ├── beacon.ino           # Beacon firmware
│       ├── config.h             # Default device config
│       └── config_local.h.example  # Local config template
└── docs/                        # Documentation
    ├── SETUP.md                 # Detailed setup guide
    ├── GAMEPLAY.md              # Game rules and strategies
    ├── HARDWARE.md              # Hardware requirements
    ├── CONFIG.md                # Configuration guide
    └── TEST.md                  # Testing procedures (duplicate)
```

**Note:** Files ending in `.example` are templates. Copy them to create your local config files (without `.example`). Local config files contain credentials and are gitignored.'''
    
    if old_structure in content:
        content = content.replace(old_structure, new_structure)
        print("✅ Updated What's Included section with complete file structure")
    else:
        print("⚠️  Could not find exact What's Included section to replace")
        print("   You may need to update it manually")
    
    with open(filepath, 'w') as f:
        f.write(content)
    
    print(f"✅ Patched {filepath}")

if __name__ == "__main__":
    if len(sys.argv) != 2:
        print("Usage: python3 patch_readme_structure.py <path/to/README.md>")
        sys.exit(1)
    
    patch_readme_structure(sys.argv[1])
