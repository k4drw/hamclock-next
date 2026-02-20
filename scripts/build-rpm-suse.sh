#!/bin/bash
set -euo pipefail

# OpenSuSE Tumbleweed RPM Build Script using Docker
# This script spins up a Tumbleweed container to build the RPM package.

SCRIPT_DIR=$( cd -- "$( dirname -- "${BASH_SOURCE[0]}" )" &> /dev/null && pwd )
REPO_ROOT=$(dirname "$SCRIPT_DIR")
cd "$REPO_ROOT" || exit 1

IMAGE="registry.opensuse.org/opensuse/tumbleweed:latest"
BUILD_DIR="build-rpm-suse"

# Clean previous build artifacts
echo "Cleaning old build artifacts..."
rm -rf "$BUILD_DIR"
mkdir -p "$BUILD_DIR"

echo "Starting RPM Build (OpenSuSE Tumbleweed)..."

# Aggressively exclude all build artifacts and sync junk to keep tarball small
echo "Creating source tarball..."
tar --exclude='./build*' \
    --exclude='./.git' \
    --exclude='./.mcp' \
    --exclude='./.cache' \
    --exclude='./.stversions' \
    --exclude="./$BUILD_DIR" \
    -czf "$BUILD_DIR/hamclock-next-0.8.0.tar.gz" .

docker run --rm \
    -v "$REPO_ROOT":/src:z \
    -w /src \
    $IMAGE \
    /bin/bash -c "
        # Install dependencies
        zypper --non-interactive install -y \
            rpm-build cmake gcc-c++ git \
            libX11-devel libXext-devel libXcursor-devel libXi-devel \
            libXrandr-devel libXss-devel libXxf86vm-devel libXinerama-devel \
            libcurl-devel libopenssl-devel libdrm-devel libgbm-devel \
            Mesa-libEGL-devel Mesa-libGLESv2-devel alsa-devel \
            desktop-file-utils update-desktop-files tar gzip && \
        
        # Setup RPM build tree
        mkdir -p /root/rpmbuild/{BUILD,RPMS,SOURCES,SPECS,SRPMS} && \
        cp $BUILD_DIR/hamclock-next-0.8.0.tar.gz /root/rpmbuild/SOURCES/ && \
        cp packaging/linux/rpm/hamclock.spec /root/rpmbuild/SPECS/ && \
        
        # Define _topdir via .rpmmacros for SuSE compatibility
        echo '%_topdir /root/rpmbuild' > /root/.rpmmacros && \
        
        # Build RPM
        rpmbuild -ba /root/rpmbuild/SPECS/hamclock.spec && \
        
        # Copy artifacts back
        cp -rv /root/rpmbuild/RPMS /src/$BUILD_DIR/ && \
        cp -rv /root/rpmbuild/SRPMS /src/$BUILD_DIR/ && \
        
        # Ensure ownership of output files
        chown -R $(id -u):$(id -g) /src/$BUILD_DIR
    "

if [ $? -eq 0 ]; then
    echo "--------------------------------------------------"
    echo "SUCCESS: SuSE RPM build finished!"
    echo "RPMs located in: $BUILD_DIR/RPMS"
    echo "SRPMs located in: $BUILD_DIR/SRPMS"
    echo "--------------------------------------------------"
else
    echo "ERROR: SuSE RPM build failed!"
    exit 1
fi
