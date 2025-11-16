#!/usr/bin/env python3
"""
IRL Hunts - Master Patch Script
Applies all bug fixes and documentation updates.

Usage: python3 apply_all_patches.py <project_root_directory>

Example: python3 apply_all_patches.py /path/to/irlhunts/
"""

import sys
import os
import subprocess

def main():
    if len(sys.argv) != 2:
        print("Usage: python3 apply_all_patches.py <project_root_directory>")
        print("\nExample: python3 apply_all_patches.py /path/to/irlhunts/")
        sys.exit(1)
    
    project_root = sys.argv[1]
    
    if not os.path.isdir(project_root):
        print(f"Error: {project_root} is not a valid directory")
        sys.exit(1)
    
    # Get the directory where this script is located
    script_dir = os.path.dirname(os.path.abspath(__file__))
    
    print("=" * 60)
    print("  IRL Hunts - Applying Bug Fixes and Updates")
    print("=" * 60)
    print(f"Project root: {project_root}")
    print(f"Patch scripts: {script_dir}")
    print()
    
    patches = [
        ("patch_tracker.py", "devices/tracker/tracker.ino", "Fix duplicate variable declarations"),
        ("patch_dashboard.py", "server/templates/dashboard.html", "Fix duplicate JavaScript code"),
        ("patch_app.py", "server/app.py", "Fix duplicate consent handling"),
        ("patch_readme.py", "README.md", "Add consent system documentation"),
        ("patch_gameplay.py", "docs/GAMEPLAY.md", "Fix duplicate content"),
    ]
    
    success_count = 0
    fail_count = 0
    
    for patch_script, target_file, description in patches:
        print(f"🔧 {description}...")
        
        patch_path = os.path.join(script_dir, patch_script)
        target_path = os.path.join(project_root, target_file)
        
        if not os.path.exists(patch_path):
            print(f"   ❌ Patch script not found: {patch_path}")
            fail_count += 1
            continue
        
        if not os.path.exists(target_path):
            print(f"   ❌ Target file not found: {target_path}")
            fail_count += 1
            continue
        
        try:
            result = subprocess.run(
                ["python3", patch_path, target_path],
                capture_output=True,
                text=True,
                timeout=30
            )
            
            # Print indented output
            for line in result.stdout.strip().split('\n'):
                if line:
                    print(f"   {line}")
            
            if result.returncode == 0:
                success_count += 1
            else:
                print(f"   ❌ Patch failed with code {result.returncode}")
                if result.stderr:
                    print(f"   Error: {result.stderr}")
                fail_count += 1
        except Exception as e:
            print(f"   ❌ Exception: {str(e)}")
            fail_count += 1
        
        print()
    
    # Copy new config.py if missing
    config_source = os.path.join(script_dir, "..", "server", "config.py")
    config_dest = os.path.join(project_root, "server", "config.py")
    
    if not os.path.exists(config_dest):
        print("📄 Creating missing server/config.py...")
        if os.path.exists(config_source):
            try:
                import shutil
                shutil.copy(config_source, config_dest)
                print("   ✅ Created server/config.py")
                success_count += 1
            except Exception as e:
                print(f"   ❌ Failed to copy config.py: {e}")
                fail_count += 1
        else:
            print("   ⚠️  Source config.py not found. Please copy manually.")
    else:
        print("📄 server/config.py already exists")
    
    print()
    print("=" * 60)
    print(f"  Summary: {success_count} succeeded, {fail_count} failed")
    print("=" * 60)
    
    if fail_count > 0:
        print("\n⚠️  Some patches failed. Check output above for details.")
        sys.exit(1)
    else:
        print("\n✅ All patches applied successfully!")
        print("\nNext steps:")
        print("1. Review changes with: git diff")
        print("2. Test compilation of tracker.ino in Arduino IDE")
        print("3. Test server startup with: python server/app.py")
        print("4. Commit changes with: git add -A && git commit -m 'Fix bugs and update docs'")

if __name__ == "__main__":
    main()
