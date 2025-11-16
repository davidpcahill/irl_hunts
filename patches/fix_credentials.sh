#!/bin/bash
# =============================================================================
# IRL Hunts - Remove Committed Credentials from Git
# =============================================================================
# 
# This script removes local config files from git tracking.
# These files contain sensitive credentials and should not be in version control.
#
# Run from your project root: bash fix_credentials.sh
# =============================================================================

echo "🔒 IRL Hunts - Credential File Cleanup"
echo "========================================"
echo ""

# Check we're in the right directory
if [ ! -f "README.md" ] || [ ! -d "server" ]; then
    echo "❌ Error: Run this script from the project root directory"
    echo "   (The directory containing README.md and server/)"
    exit 1
fi

echo "This script will:"
echo "1. Remove credential files from git tracking (keeps local copies)"
echo "2. Ensure .gitignore is properly configured"
echo "3. Show you what to commit"
echo ""
echo "⚠️  Your local config files will NOT be deleted, just untracked from git."
echo ""
read -p "Continue? (y/N) " -n 1 -r
echo ""

if [[ ! $REPLY =~ ^[Yy]$ ]]; then
    echo "Cancelled."
    exit 0
fi

echo ""
echo "📁 Step 1: Checking for committed credential files..."

FILES_TO_UNTRACK=""

if git ls-files --error-unmatch server/config_local.py &>/dev/null; then
    echo "   Found: server/config_local.py (tracked in git)"
    FILES_TO_UNTRACK="$FILES_TO_UNTRACK server/config_local.py"
else
    echo "   ✓ server/config_local.py is not tracked"
fi

if git ls-files --error-unmatch devices/tracker/config_local.h &>/dev/null; then
    echo "   Found: devices/tracker/config_local.h (tracked in git)"
    FILES_TO_UNTRACK="$FILES_TO_UNTRACK devices/tracker/config_local.h"
else
    echo "   ✓ devices/tracker/config_local.h is not tracked"
fi

if git ls-files --error-unmatch devices/beacon/config_local.h &>/dev/null; then
    echo "   Found: devices/beacon/config_local.h (tracked in git)"
    FILES_TO_UNTRACK="$FILES_TO_UNTRACK devices/beacon/config_local.h"
else
    echo "   ✓ devices/beacon/config_local.h is not tracked"
fi

if [ -z "$FILES_TO_UNTRACK" ]; then
    echo ""
    echo "✅ No credential files are tracked in git. You're all set!"
    exit 0
fi

echo ""
echo "📝 Step 2: Removing files from git tracking..."

for file in $FILES_TO_UNTRACK; do
    echo "   Untracking: $file"
    git rm --cached "$file" 2>/dev/null
done

echo ""
echo "🔍 Step 3: Verifying .gitignore..."

# Check if .gitignore has the right entries
GITIGNORE_OK=true

if ! grep -q "server/config_local.py" .gitignore 2>/dev/null; then
    echo "   ⚠️  Missing from .gitignore: server/config_local.py"
    GITIGNORE_OK=false
fi

if ! grep -q "devices/\*/config_local.h" .gitignore 2>/dev/null; then
    echo "   ⚠️  Missing from .gitignore: devices/*/config_local.h"
    GITIGNORE_OK=false
fi

if [ "$GITIGNORE_OK" = true ]; then
    echo "   ✓ .gitignore is properly configured"
else
    echo ""
    echo "   Adding missing entries to .gitignore..."
    echo "" >> .gitignore
    echo "# Local configs with credentials - NEVER commit these!" >> .gitignore
    echo "server/config_local.py" >> .gitignore
    echo "devices/*/config_local.h" >> .gitignore
    echo "   ✓ Updated .gitignore"
fi

echo ""
echo "========================================"
echo "✅ Done! Files are now untracked from git."
echo ""
echo "Your credential files are still on disk but won't be committed."
echo ""
echo "Next steps:"
echo "1. Review changes: git status"
echo "2. Commit the removal: git commit -m 'Remove credential files from tracking'"
echo "3. If already pushed, consider changing your credentials"
echo ""
echo "⚠️  WARNING: If you've already pushed these files to a remote repository,"
echo "   your credentials may be exposed in git history. Consider:"
echo "   - Changing your WiFi password"
echo "   - Changing your admin password"
echo "   - Using git filter-branch to remove from history (advanced)"
echo ""
