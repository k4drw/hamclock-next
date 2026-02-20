#!/bin/bash
set -euo pipefail

# Fedora RPM Build Script using Docker
# This script spins up a Fedora container to build the RPM package.

SCRIPT_DIR=$( cd -- "$( dirname -- "${BASH_SOURCE[0]}" )" &> /dev/null && pwd )
REPO_ROOT=$(dirname "$SCRIPT_DIR")
cd "$REPO_ROOT" || exit 1

IMAGE="fedora:latest"
BUILD_DIR="build-rpm"

# Clean previous build artifacts
echo "Cleaning old build artifacts..."
rm -rf "$BUILD_DIR"
mkdir -p "$BUILD_DIR"

echo "Starting RPM Build (Fedora)..."
# We run as root inside the container to install packages, but we'll try to build as user if possible?
# Actually rpmbuild is often easier as a user. We'll install deps then switch to user.

docker run --rm \
    -v "$REPO_ROOT":/src:z \
    -w /src \
    $IMAGE \
    /bin/bash -c "
        dnf install -y rpm-build cmake gcc-c++ git libstdc++-static libX11-devel libXext-devel libXcursor-devel libXi-devel libXrandr-devel libXScrnSaver-devel libXxf86vm-devel libXinerama-devel libcurl-devel openssl-devel libdrm-devel mesa-libgbm-devel mesa-libEGL-devel mesa-libGLES-devel alsa-lib-devel tar gzip desktop-file-utils && \
        
        # Create tarball for Source0
        # Exclude build directories to keep it small
        tar --exclude='./build*' --exclude='./.git' -czf /src/$BUILD_DIR/hamclock-next-0.8.0.tar.gz . && \
        
        # Setup RPM build tree
        mkdir -p /root/rpmbuild/{BUILD,RPMS,SOURCES,SPECS,SRPMS} && \
        cp /src/$BUILD_DIR/hamclock-next-0.8.0.tar.gz /root/rpmbuild/SOURCES/ && \
        cp /src/packaging/linux/rpm/hamclock.spec /root/rpmbuild/SPECS/ && \
        
        # Build RPM
        rpmbuild -ba /root/rpmbuild/SPECS/hamclock.spec && \
        
        # Copy artifacts back
        cp -r /root/rpmbuild/RPMS /src/$BUILD_DIR/ && \
        cp -r /root/rpmbuild/SRPMS /src/$BUILD_DIR/ && \
        
        # Ensure ownership of output files
        chown -R $(id -u):$(id -g) /src/$BUILD_DIR
    "

if [ $? -eq 0 ]; then
    echo "--------------------------------------------------"
    echo "SUCCESS: RPM build finished!"
    echo "RPMs located in: $BUILD_DIR/RPMS"
    echo "SRPMs located in: $BUILD_DIR/SRPMS"
    echo "--------------------------------------------------"
else
    echo "ERROR: RPM build failed!"
    exit 1
fi
