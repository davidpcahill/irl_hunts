#!/usr/bin/env python3
"""
Patch script for .gitignore
Ensures all local config files with credentials are properly ignored.

Apply with: python3 patch_gitignore.py path/to/.gitignore
"""

import sys
import os

def patch_gitignore(filepath):
    # Read existing .gitignore or create empty if doesn't exist
    if os.path.exists(filepath):
        with open(filepath, 'r') as f:
            content = f.read()
        lines = content.strip().split('\n') if content.strip() else []
    else:
        content = ""
        lines = []
    
    changes_made = 0
    
    # Essential patterns that MUST be in .gitignore
    required_patterns = {
        # Local config files with credentials
        "server/config_local.py": "Local server config with credentials",
        "devices/*/config_local.h": "Device configs with WiFi passwords",
        
        # Additional safety patterns
        "*.local.py": "Any .local.py files",
        "*.local.h": "Any .local.h files",
        "*_local.py": "Any _local.py files", 
        "*_local.h": "Any _local.h files",
        
        # Environment files
        ".env": "Environment variables",
        "*.env": "Any .env files",
        ".env.*": "Environment file variants",
        "!.env.example": "But keep examples",
        
        # Python
        "__pycache__/": "Python bytecode cache",
        "*.pyc": "Compiled Python",
        "*.pyo": "Optimized Python",
        "venv/": "Virtual environment",
        ".venv/": "Virtual environment (dot prefix)",
        "env/": "Environment folder",
        "ENV/": "Environment folder (caps)",
        
        # IDE
        ".vscode/": "VS Code settings",
        ".idea/": "JetBrains IDE",
        "*.swp": "Vim swap files",
        "*.swo": "Vim swap files",
        "*~": "Backup files",
        ".DS_Store": "macOS folder attributes",
        "Thumbs.db": "Windows thumbnails",
        
        # Uploads (user content)
        "server/uploads/*": "Uploaded photos",
        "!server/uploads/.gitkeep": "But keep the folder",
        "uploads/": "Any uploads folder",
        
        # Logs
        "*.log": "Log files",
        "logs/": "Logs folder",
        
        # Database
        "*.db": "Database files",
        "*.sqlite": "SQLite databases",
        "*.sqlite3": "SQLite3 databases",
    }
    
    # Check which patterns are missing
    missing_patterns = []
    for pattern, description in required_patterns.items():
        # Check if pattern exists in any form (with or without comments)
        pattern_found = False
        for line in lines:
            # Strip comments and whitespace for comparison
            clean_line = line.split('#')[0].strip()
            if clean_line == pattern:
                pattern_found = True
                break
        
        if not pattern_found:
            missing_patterns.append((pattern, description))
    
    if not missing_patterns:
        print("✅ .gitignore already has all required patterns")
        return
    
    print(f"📝 Adding {len(missing_patterns)} missing patterns to .gitignore...")
    
    # Group missing patterns by category
    categories = {
        "Credentials & Local Configs": [],
        "Python": [],
        "IDE & Editor": [],
        "User Content": [],
        "Logs & Database": [],
        "Other": []
    }
    
    for pattern, desc in missing_patterns:
        if "local" in pattern.lower() or "env" in pattern.lower() or "credential" in desc.lower():
            categories["Credentials & Local Configs"].append((pattern, desc))
        elif "python" in desc.lower() or pattern.endswith('.py') or pattern.endswith('.pyc') or "venv" in pattern:
            categories["Python"].append((pattern, desc))
        elif any(x in desc.lower() for x in ["ide", "vim", "code", "macos", "windows"]):
            categories["IDE & Editor"].append((pattern, desc))
        elif "upload" in pattern.lower() or "user" in desc.lower():
            categories["User Content"].append((pattern, desc))
        elif "log" in pattern.lower() or "database" in desc.lower() or pattern.endswith('.db'):
            categories["Logs & Database"].append((pattern, desc))
        else:
            categories["Other"].append((pattern, desc))
    
    # Build the additions
    additions = []
    
    # Add a separator if file already has content
    if content.strip():
        additions.append("")
        additions.append("# " + "=" * 50)
        additions.append("# Additional gitignore patterns (auto-added)")
        additions.append("# " + "=" * 50)
    
    for category, patterns in categories.items():
        if patterns:
            additions.append("")
            additions.append(f"# {category}")
            for pattern, desc in patterns:
                additions.append(f"{pattern}")
            changes_made += len(patterns)
    
    # Append to content
    new_content = content.rstrip() + '\n' + '\n'.join(additions) + '\n'
    
    # Write the updated file
    with open(filepath, 'w') as f:
        f.write(new_content)
    
    print(f"✅ Added {changes_made} patterns to .gitignore")
    
    # List what was added
    print("\nPatterns added:")
    for pattern, desc in missing_patterns:
        print(f"   {pattern}")

def create_gitkeep(project_root):
    """Create .gitkeep in uploads folder so folder is tracked but not contents"""
    uploads_dir = os.path.join(project_root, "server", "uploads")
    gitkeep_path = os.path.join(uploads_dir, ".gitkeep")
    
    if os.path.isdir(uploads_dir) and not os.path.exists(gitkeep_path):
        with open(gitkeep_path, 'w') as f:
            f.write("# This file ensures the uploads directory is tracked by git\n")
            f.write("# The actual uploaded files are gitignored\n")
        print(f"✅ Created {gitkeep_path}")

if __name__ == "__main__":
    if len(sys.argv) < 2:
        print("Usage: python3 patch_gitignore.py <path/to/.gitignore> [project_root]")
        print("\nExample: python3 patch_gitignore.py .gitignore .")
        sys.exit(1)
    
    gitignore_path = sys.argv[1]
    patch_gitignore(gitignore_path)
    
    # Optionally create .gitkeep if project root provided
    if len(sys.argv) >= 3:
        project_root = sys.argv[2]
        create_gitkeep(project_root)
