#!/bin/bash
# Master Build Script - Executes all platform builds
# Requirements: Docker, macOS (for macOS build)

SCRIPT_DIR=$( cd -- "$( dirname -- "${BASH_SOURCE[0]}" )" &> /dev/null && pwd )
REPO_ROOT=$(dirname "$SCRIPT_DIR")
cd "$REPO_ROOT" || exit 1

echo "=================================================="
echo "   HamClock-Next Master Build System"
echo "=================================================="

# Function to run a build and log status
run_build() {
    local name=$1
    local script=$2
    echo ">>> Starting $name build..."
    if bash "$script"; then
        echo ">>> $name build SUCCESS"
        return 0
    else
        echo ">>> $name build FAILED"
        return 1
    fi
}

FAILED_BUILDS=()

# 1. Desktop Builds
if [[ "$(uname)" == "Darwin" ]]; then
    run_build "MacOS" "scripts/build-macos.sh" || FAILED_BUILDS+=("MacOS")
else
    echo "Skipping MacOS build (not on Darwin)"
fi

run_build "Windows x64" "scripts/build-win64.sh" || FAILED_BUILDS+=("Windows x64")
run_build "Linux x64 DEB" "scripts/build-linux-x64-deb.sh" || FAILED_BUILDS+=("Linux x64")
run_build "Fedora RPM" "scripts/build-rpm-fedora.sh" || FAILED_BUILDS+=("Fedora RPM")

# 2. ARM Universal Builds
run_build "Linux ARM64 Universal" "scripts/build-linux-arm64-universal.sh" || FAILED_BUILDS+=("ARM64")
run_build "Linux ARMhf Universal" "scripts/build-linux-armhf-universal.sh" || FAILED_BUILDS+=("ARMhf")

# 4. WASM Build
run_build "WASM (Docker)" "scripts/build-wasm-docker.sh" || FAILED_BUILDS+=("WASM")

echo "=================================================="
if [ ${#FAILED_BUILDS[@]} -eq 0 ]; then
    echo "   ALL BUILDS COMPLETED SUCCESSFULLY"
else
    echo "   SOME BUILDS FAILED:"
    for f in "${FAILED_BUILDS[@]}"; do
        echo "    - $f"
    done
    exit 1
fi
echo "=================================================="
