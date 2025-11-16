#!/usr/bin/env python3
"""
Patch script for devices/tracker/tracker.ino
Fixes duplicate variable declarations that would cause compilation errors.

Apply with: python3 patch_tracker.py path/to/tracker.ino
"""

import sys
import re

def patch_tracker(filepath):
    with open(filepath, 'r') as f:
        content = f.read()
    
    # Fix 1: Remove duplicate global variable declarations
    # The problematic section declares consentBadge and myReady multiple times
    old_vars = '''bool wifiConnected = false, serverReachable = false;
String consentBadge = "";  // Visual badge for LoRa broadcasts
bool wifiConnected = false, serverReachable = false;
String consentBadge = "STD";  // Consent flags badge (STD=standard, T=touch OK, NP=no photo, NL=no location)
bool myReady = false;
String consentBadge = "STD";  // Consent flags badge (STD=standard, T=touch OK, NP=no photo, NL=no location)
bool myReady = false;
unsigned long lastWifiCheck = 0, lastSuccessfulPing = 0;'''
    
    new_vars = '''bool wifiConnected = false, serverReachable = false;
String consentBadge = "STD";  // Consent flags badge (STD=standard, T=touch OK, NP=no photo, NL=no location)
bool myReady = false;
unsigned long lastWifiCheck = 0, lastSuccessfulPing = 0;'''
    
    if old_vars in content:
        content = content.replace(old_vars, new_vars)
        print("✅ Fixed duplicate global variable declarations")
    else:
        print("⚠️  Duplicate global vars pattern not found (may already be fixed)")
    
    # Fix 2: Remove duplicate/incomplete consent flag update in pingServer()
    old_consent_update = '''      // Update consent flags
      if (consentPhysical != newConsentPhysical || consentPhoto != newConsentPhoto) {
        consentPhysical = newConsentPhysical;
        consentPhoto = newConsentPhoto;
        // Update badge string for display
        consentBadge = "";
        if (consentPhysical) consentBadge += "🤝";
        if (!consentPhoto) consentBadge += "📷❌";
      }
      String newConsentBadge = respDoc["consent_badge"].as<String>();
      bool newReady = respDoc["ready"];
      
      if (newName.length() > 0) myName = newName;
      if (newConsentBadge.length() > 0) consentBadge = newConsentBadge;
      myReady = newReady;'''
    
    new_consent_update = '''      String newConsentBadge = respDoc["consent_badge"].as<String>();
      bool newReady = respDoc["ready"];
      
      if (newName.length() > 0) myName = newName;
      if (newConsentBadge.length() > 0) consentBadge = newConsentBadge;
      myReady = newReady;'''
    
    if old_consent_update in content:
        content = content.replace(old_consent_update, new_consent_update)
        print("✅ Fixed duplicate consent update logic in pingServer()")
    else:
        print("⚠️  Consent update pattern not found (may already be fixed)")
    
    # Write the fixed file
    with open(filepath, 'w') as f:
        f.write(content)
    
    print(f"✅ Patched {filepath}")

if __name__ == "__main__":
    if len(sys.argv) != 2:
        print("Usage: python3 patch_tracker.py <path/to/tracker.ino>")
        sys.exit(1)
    
    patch_tracker(sys.argv[1])
