#!/bin/bash
set -euo pipefail

EMSDK_DIR="${EMSDK:-$HOME/emsdk}"
if [ ! -f "$EMSDK_DIR/emsdk_env.sh" ]; then
    echo "Error: emsdk not found at $EMSDK_DIR"
    echo "Set EMSDK env var or install at ~/emsdk"
    exit 1
fi

source "$EMSDK_DIR/emsdk_env.sh"

BUILD_DIR="build-wasm"
mkdir -p "$BUILD_DIR"

DEBUG_API_FLAG="-DENABLE_DEBUG_API=OFF"
for arg in "$@"; do
    if [ "$arg" == "--enable-debug-api" ]; then
        DEBUG_API_FLAG="-DENABLE_DEBUG_API=ON"
    fi
done

emcmake cmake -S . -B "$BUILD_DIR" \
    -DCMAKE_BUILD_TYPE=Release \
    ${DEBUG_API_FLAG}

emmake make -C "$BUILD_DIR" hamclock-wasm -j"$(nproc)"

echo ""
echo "=== WASM build complete ==="
echo "Output: $BUILD_DIR/hamclock-wasm.html"
echo ""
echo "Serve with COOP/COEP headers (required for SharedArrayBuffer/pthreads):"
echo "  python3 packaging/web/serve.py $BUILD_DIR"
find "$BUILD_DIR" -maxdepth 1 ! -name "hamclock-next*" -delete
