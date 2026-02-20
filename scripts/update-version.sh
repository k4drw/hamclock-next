#!/bin/bash
# scripts/update-version.sh
# Automates updating version strings across the codebase based on VERSION and VERSION_SUFFIX files.

set -e

REPO_ROOT=$( cd -- "$( dirname -- "${BASH_SOURCE[0]}" )/.." &> /dev/null && pwd )
cd "$REPO_ROOT"

if [ ! -f VERSION ] || [ ! -f VERSION_SUFFIX ]; then
    echo "Error: VERSION or VERSION_SUFFIX file missing in root."
    exit 1
fi

VERSION=$(cat VERSION | tr -d '[:space:]')
SUFFIX=$(cat VERSION_SUFFIX | tr -d '[:space:]')
# Short version for HAMCLOCK_VERSION (e.g. 0.8B)
# We assume VERSION is x.y.z, we want x.y for the short version if z is 0
SHORT_VER=$(echo $VERSION | sed 's/\.0$//')
FULL_VERSION="${SHORT_VER}${SUFFIX}"

echo "Updating codebase to version: ${VERSION} (Display: ${FULL_VERSION})"

# 1. README.md
sed -i "1s/# HamClock-Next (v.*)/# HamClock-Next (v${FULL_VERSION})/" README.md
sed -i "s/BETA RELEASE NOTICE\*\*: This is a Beta release (\*\*v[0-9.A-Z]*\*\*)/BETA RELEASE NOTICE**: This is a Beta release (**v${FULL_VERSION}**)/" README.md

# 2. packaging/vcpkg.json - Preserve comma if present
sed -i "s/\"version-string\": \"[^\"]*\"/\"version-string\": \"${FULL_VERSION}\"/" packaging/vcpkg.json

# 3. packaging/linux/rpm/hamclock.spec
sed -i "s/^Version:        .*/Version:        ${VERSION}/" packaging/linux/rpm/hamclock.spec

echo "Done. Build scripts and CMakeLists.txt read VERSION and VERSION_SUFFIX directly."
