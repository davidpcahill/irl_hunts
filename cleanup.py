#!/usr/bin/env python3
"""
IRL Hunts - Cleanup Utility
Cleans up old sighting photos and temporary files.

Run: python cleanup.py [--all] [--sightings] [--profiles]
"""

import os
import glob
import argparse
from datetime import datetime, timedelta

def cleanup_sightings(upload_dir):
    """Remove all sighting photos"""
    files = glob.glob(os.path.join(upload_dir, "sighting_*"))
    count = 0
    for f in files:
        try:
            os.remove(f)
            count += 1
        except Exception as e:
            print(f"  ⚠️ Could not remove {f}: {e}")
    return count

def cleanup_old_profiles(upload_dir, days=7):
    """Remove profile pics older than X days"""
    cutoff = datetime.now() - timedelta(days=days)
    files = glob.glob(os.path.join(upload_dir, "profile_*"))
    count = 0
    for f in files:
        try:
            mtime = datetime.fromtimestamp(os.path.getmtime(f))
            if mtime < cutoff:
                os.remove(f)
                count += 1
        except Exception as e:
            print(f"  ⚠️ Could not process {f}: {e}")
    return count

def main():
    parser = argparse.ArgumentParser(description="Clean up IRL Hunts files")
    parser.add_argument("--all", action="store_true", help="Remove all uploaded files")
    parser.add_argument("--sightings", action="store_true", help="Remove sighting photos only")
    parser.add_argument("--profiles", type=int, metavar="DAYS", help="Remove profiles older than X days")
    
    args = parser.parse_args()
    
    upload_dir = os.path.join(os.path.dirname(__file__), "server", "uploads")
    if not os.path.exists(upload_dir):
        print(f"❌ Upload directory not found: {upload_dir}")
        return
    
    print(f"🧹 IRL Hunts Cleanup Utility")
    print(f"📁 Upload directory: {upload_dir}")
    
    if args.all:
        files = glob.glob(os.path.join(upload_dir, "*"))
        files = [f for f in files if not f.endswith(".gitkeep")]
        for f in files:
            os.remove(f)
        print(f"✅ Removed all {len(files)} files")
    elif args.sightings:
        count = cleanup_sightings(upload_dir)
        print(f"✅ Removed {count} sighting photos")
    elif args.profiles:
        count = cleanup_old_profiles(upload_dir, args.profiles)
        print(f"✅ Removed {count} profiles older than {args.profiles} days")
    else:
        print("No action specified. Use --help to see options.")
        
        # Show current usage
        files = glob.glob(os.path.join(upload_dir, "*"))
        sightings = len([f for f in files if "sighting_" in f])
        profiles = len([f for f in files if "profile_" in f])
        print(f"\nCurrent files: {len(files)} total")
        print(f"  - Sighting photos: {sightings}")
        print(f"  - Profile pics: {profiles}")

if __name__ == "__main__":
    main()
