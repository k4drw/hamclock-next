#!/bin/bash
# Script to create macOS .icns icon from PNG source
# Usage: ./scripts/create-macos-icon.sh

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(dirname "$SCRIPT_DIR")"
SOURCE_ICON="$PROJECT_ROOT/packaging/icon.png"
ICONSET_DIR="$PROJECT_ROOT/packaging/macos/HamClockNext.iconset"
OUTPUT_ICNS="$PROJECT_ROOT/packaging/macos/HamClockNext.icns"

# Check if running on macOS
if [[ "$OSTYPE" != "darwin"* ]]; then
    echo "ERROR: This script must be run on macOS (requires sips and iconutil)"
    exit 1
fi

# Check if source icon exists
if [ ! -f "$SOURCE_ICON" ]; then
    echo "ERROR: Source icon not found at $SOURCE_ICON"
    exit 1
fi

echo "Creating macOS icon from $SOURCE_ICON..."

# Create iconset directory
rm -rf "$ICONSET_DIR"
mkdir -p "$ICONSET_DIR"

# Generate all required icon sizes
# macOS requires 1x and 2x (Retina) versions of each size
sips -z 16 16     "$SOURCE_ICON" --out "$ICONSET_DIR/icon_16x16.png" > /dev/null
sips -z 32 32     "$SOURCE_ICON" --out "$ICONSET_DIR/icon_16x16@2x.png" > /dev/null
sips -z 32 32     "$SOURCE_ICON" --out "$ICONSET_DIR/icon_32x32.png" > /dev/null
sips -z 64 64     "$SOURCE_ICON" --out "$ICONSET_DIR/icon_32x32@2x.png" > /dev/null
sips -z 128 128   "$SOURCE_ICON" --out "$ICONSET_DIR/icon_128x128.png" > /dev/null
sips -z 256 256   "$SOURCE_ICON" --out "$ICONSET_DIR/icon_128x128@2x.png" > /dev/null
sips -z 256 256   "$SOURCE_ICON" --out "$ICONSET_DIR/icon_256x256.png" > /dev/null
sips -z 512 512   "$SOURCE_ICON" --out "$ICONSET_DIR/icon_256x256@2x.png" > /dev/null
sips -z 512 512   "$SOURCE_ICON" --out "$ICONSET_DIR/icon_512x512.png" > /dev/null
sips -z 1024 1024 "$SOURCE_ICON" --out "$ICONSET_DIR/icon_512x512@2x.png" > /dev/null

echo "Generated icon resolutions:"
ls -lh "$ICONSET_DIR"

# Convert iconset to .icns
echo ""
echo "Converting iconset to .icns..."
iconutil -c icns "$ICONSET_DIR" -o "$OUTPUT_ICNS"

# Clean up iconset directory
rm -rf "$ICONSET_DIR"

echo ""
echo "SUCCESS: Icon created at $OUTPUT_ICNS"
ls -lh "$OUTPUT_ICNS"
