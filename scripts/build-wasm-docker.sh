#!/bin/bash
set -euo pipefail

# Check for Docker
if ! command -v docker &> /dev/null; then
    echo "Error: docker command not found."
    exit 1
fi

echo "Building HamClock-Next WASM using Docker (emscripten/emsdk:latest)..."

# Get absolute path to repo root (parent of scripts dir)
REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD_DIR="build-wasm"

# Clean previous build to avoid CMake cache conflicts
rm -rf "$REPO_ROOT/$BUILD_DIR"
mkdir -p "$REPO_ROOT/$BUILD_DIR"

# Run build inside container
# We map the current user to avoid root-owned files in build-wasm/
docker run --rm \
    -v "$REPO_ROOT":/src \
    -u "$(id -u):$(id -g)" \
    emscripten/emsdk:latest \
    /bin/bash -c "
        emcmake cmake -S . -B $BUILD_DIR \
            -DCMAKE_BUILD_TYPE=Release \
            -DENABLE_DEBUG_API=OFF && \
        emmake make -C $BUILD_DIR hamclock-wasm -j$(nproc)
    "

echo ""
echo "=== WASM build complete (via Docker) ==="
echo "Output: $REPO_ROOT/$BUILD_DIR/hamclock-wasm.html"
echo ""
echo "Serve with COOP/COEP headers:"
echo "  python3 packaging/web/serve.py $BUILD_DIR"
