#!/usr/bin/env python3
"""
Patch script for server/templates/dashboard.html
Fixes duplicate JavaScript code blocks.

Apply with: python3 patch_dashboard.py path/to/dashboard.html
"""

import sys

def patch_dashboard(filepath):
    with open(filepath, 'r') as f:
        content = f.read()
    
    changes_made = 0
    
    # Fix 1: Remove duplicate initialization calls at the bottom of script
    old_bottom = '''        initConsent();  // Initialize consent settings
        isReady = playerData.status === 'ready';
        updateReadyButton();
        initConsent();  // Initialize consent settings
        isReady = playerData.status === 'ready';
        updateReadyButton();'''
    
    new_bottom = '''        initConsent();  // Initialize consent settings
        isReady = playerData.status === 'ready';
        updateReadyButton();'''
    
    if old_bottom in content:
        content = content.replace(old_bottom, new_bottom)
        print("✅ Fixed duplicate initialization calls")
        changes_made += 1
    
    # Fix 2: Remove duplicate function declarations (isReady, initConsent, etc.)
    # These appear twice in the script
    old_duplicate_funcs = '''        let isReady = false;
        
        // Initialize consent checkboxes from player data
        function initConsent() {
            const flags = playerData.consent_flags || {
                physical_tag: false,
                photo_visible: true,
                location_share: true
            };
            document.getElementById('consent-physical').checked = flags.physical_tag;
            document.getElementById('consent-photo').checked = flags.photo_visible;
            document.getElementById('consent-location').checked = flags.location_share;
            updateConsentBadge();
        }
        
        function updateConsentBadge() {
            const physical = document.getElementById('consent-physical').checked;
            const photo = document.getElementById('consent-photo').checked;
            const location = document.getElementById('consent-location').checked;
            
            let badge = [];
            if (physical) badge.push("T");  // Touch OK
            if (!photo) badge.push("NP");   // No Photo
            if (!location) badge.push("NL"); // No Location
            
            const badgeText = badge.length > 0 ? badge.join("") : "STD";
            document.getElementById('consent-badge-text').textContent = badgeText;
        }
        
        async function updateConsent() {
            const flags = {
                physical_tag: document.getElementById('consent-physical').checked,
                photo_visible: document.getElementById('consent-photo').checked,
                location_share: document.getElementById('consent-location').checked
            };
            
            updateConsentBadge();
            
            try {
                const resp = await fetch('/api/player/consent', {
                    method: 'PUT',
                    headers: {'Content-Type': 'application/json'},
                    body: JSON.stringify(flags)
                });
                
                if (resp.ok) {
                    notify('Consent settings updated', 'success');
                    playerData.consent_flags = flags;
                } else {
                    notify('Failed to update consent', 'error');
                }
            } catch (err) {
                notify('Connection error', 'error');
            }
        }
        
        async function toggleReady() {
            const newStatus = isReady ? 'lobby' : 'ready';
            
            try {
                const resp = await fetch('/api/player', {
                    method: 'PUT',
                    headers: {'Content-Type': 'application/json'},
                    body: JSON.stringify({status: newStatus})
                });
                
                if (resp.ok) {
                    isReady = !isReady;
                    updateReadyButton();
                    notify(isReady ? 'You are ready!' : 'You are no longer ready', isReady ? 'success' : 'info');
                } else {
                    const data = await resp.json();
                    notify(data.error || 'Failed to update status', 'error');
                }
            } catch (err) {
                notify('Failed to update ready status', 'error');
            }
        }
        
        function updateReadyButton() {
            const btn = document.getElementById('ready-btn');
            if (isReady) {
                btn.textContent = '❌ Click to Unready';
                btn.className = 'btn btn-warning';
            } else {
                btn.textContent = '✅ Ready to Play';
                btn.className = 'btn btn-success';
            }
        }
        let isReady = false;'''
    
    # Only keep the first occurrence, remove the duplicate
    count = content.count('let isReady = false;')
    if count > 1:
        # Find where the duplication starts (second let isReady = false;)
        first_pos = content.find('let isReady = false;')
        second_pos = content.find('let isReady = false;', first_pos + 1)
        
        if second_pos > 0:
            # Find the end of the duplicate block - it's followed by "let currentGameMode"
            end_marker = 'let currentGameMode = "{{ game_mode }}";'
            end_pos = content.find(end_marker, second_pos)
            
            if end_pos > second_pos:
                # Remove the duplicate block
                content = content[:second_pos] + content[end_pos:]
                print("✅ Removed duplicate function declarations")
                changes_made += 1
    
    if changes_made == 0:
        print("⚠️  No duplicate patterns found (may already be fixed)")
    
    # Write the fixed file
    with open(filepath, 'w') as f:
        f.write(content)
    
    print(f"✅ Patched {filepath} ({changes_made} fixes applied)")

if __name__ == "__main__":
    if len(sys.argv) != 2:
        print("Usage: python3 patch_dashboard.py <path/to/dashboard.html>")
        sys.exit(1)
    
    patch_dashboard(sys.argv[1])
