#!/bin/bash
# scripts/build-android-docker.sh
# Experimental script to build an Android APK for hamclock-next using Docker
# This will completely scaffold an SDL2 gradle project and compile it.

set -euo pipefail

REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD_DIR="$REPO_ROOT/build-android"

DEBUG_API_BOOL="OFF"
for arg in "$@"; do
    if [ "$arg" == "--enable-debug-api" ]; then
        DEBUG_API_BOOL="ON"
    fi
done

echo "=================================================="
echo "    HamClock-Next Android APK Build Script       "
echo "=================================================="

if ! command -v docker &> /dev/null; then
    echo "Error: docker command not found. Please install Docker first."
    exit 1
fi

echo "Cleaning previous build dir..."
rm -rf "$BUILD_DIR"
mkdir -p "$BUILD_DIR"
cd "$BUILD_DIR"

echo "[1/3] Scaffolding SDL2 Android project template..."
# We use SDL 2.30.2 to match the version in CMakeLists.txt
curl -sL https://github.com/libsdl-org/SDL/archive/refs/tags/release-2.30.2.tar.gz | tar xz
mv SDL-release-2.30.2/android-project/* .
mv SDL-release-2.30.2/android-project/.* . 2>/dev/null || true
rm -rf SDL-release-2.30.2

echo "[2/3] Wiring CMake to the Android template..."
# Change ndkBuild to cmake in gradle
sed -i 's/ndkBuild {/cmake {/g' app/build.gradle
sed -i "s/path 'jni\/Android.mk'/path 'jni\/CMakeLists.txt'/g" app/build.gradle
sed -i 's/path "jni\/Android.mk"/path "jni\/CMakeLists.txt"/g' app/build.gradle

# Remove old ndkBuild arguments (like APP_PLATFORM=android-19) that confuse CMake
sed -i '/arguments/d' app/build.gradle

# Update app ID to hamclock's package
sed -i 's/org.libsdl.app/com.k4drw.hcn/g' app/build.gradle
sed -i 's/org.libsdl.app/com.k4drw.hcn/g' app/src/main/AndroidManifest.xml

# Bump minSdkVersion to 24 (Android 7.0) to get native 'getifaddrs' support in Bionic libc
sed -i 's/minSdkVersion 16/minSdkVersion 24/g' app/build.gradle
sed -i 's/minSdkVersion 19/minSdkVersion 24/g' app/build.gradle

# Update app version from the repository VERSION file
PROJECT_VERSION=$(cat "$REPO_ROOT/VERSION" | tr -d '[:space:]' | sed 's/^v//')
V1=$(echo "$PROJECT_VERSION" | cut -d. -f1)
V2=$(echo "$PROJECT_VERSION" | cut -d. -f2)
V3=$(echo "$PROJECT_VERSION" | cut -d. -f3)
# Fallback to zero if fields are empty
V1=${V1:-1}
V2=${V2:-0}
V3=${V3:-0}
VERSION_CODE=$((V1 * 10000 + V2 * 100 + V3))

sed -i "s/versionName \"1.0\"/versionName \"$PROJECT_VERSION\"/g" app/build.gradle
sed -i "s/versionCode 1/versionCode $VERSION_CODE/g" app/build.gradle

# Fix the app name from default SDL "Game"
sed -i 's/<string name="app_name">Game<\/string>/<string name="app_name">HamClock-Next<\/string>/g' app/src/main/res/values/strings.xml

# Replace default SDL icons with HamClock icon
for d in app/src/main/res/mipmap-*/; do
    cp ../packaging/icon.png "${d}ic_launcher.png"
done

# AndroidManifest expects com.k4drw.hcn.SDLActivity, creating wrapper for libmain.so
mkdir -p app/src/main/java/com/hamclock/next
cat << 'JAVA_EOF' > app/src/main/java/com/hamclock/next/SDLActivity.java
package com.k4drw.hcn;

import android.Manifest;
import android.content.pm.PackageManager;
import android.os.Bundle;

public class SDLActivity extends org.libsdl.app.SDLActivity {
    private static final int PERM_REQUEST_CODE = 1;

    @Override
    protected String[] getLibraries() {
        return new String[] { "main" };
    }

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        String[] perms = {
            Manifest.permission.ACCESS_FINE_LOCATION,
            Manifest.permission.ACCESS_COARSE_LOCATION
        };
        java.util.ArrayList<String> needed = new java.util.ArrayList<>();
        for (String p : perms) {
            if (checkSelfPermission(p) != PackageManager.PERMISSION_GRANTED) {
                needed.add(p);
            }
        }
        if (!needed.isEmpty()) {
            requestPermissions(needed.toArray(new String[0]), PERM_REQUEST_CODE);
        }
    }
}
JAVA_EOF

# Network Security Config — Android 9+ (API 28) blocks cleartext HTTP by default.
# HamClock connects to the local data hub over http://IP:PORT, so we must explicitly
# permit cleartext traffic. Without this every curl fetch silently returns nothing.
mkdir -p app/src/main/res/xml
cat << 'NSC_EOF' > app/src/main/res/xml/network_security_config.xml
<?xml version="1.0" encoding="utf-8"?>
<network-security-config>
    <!-- Allow cleartext HTTP for local data hub (LAN) and any legacy HTTP endpoints -->
    <base-config cleartextTrafficPermitted="true">
        <trust-anchors>
            <certificates src="system" />
        </trust-anchors>
    </base-config>
</network-security-config>
NSC_EOF

# Wire the network security config into the <application> element
sed -i 's|android:label="@string/app_name"|android:networkSecurityConfig="@xml/network_security_config" android:label="@string/app_name"|' app/src/main/AndroidManifest.xml

# Force landscape orientation on the SDL activity
sed -i 's/android:exported="true"/android:exported="true"\n        android:screenOrientation="sensorLandscape"/' app/src/main/AndroidManifest.xml

# Add permissions before </manifest> — this anchor is guaranteed to exist regardless of template formatting.
# INTERNET/ACCESS_NETWORK_STATE/ACCESS_WIFI_STATE are normal permissions (granted silently at install).
# ACCESS_FINE_LOCATION/ACCESS_COARSE_LOCATION are dangerous permissions that trigger a runtime dialog.
sed -i 's|</manifest>|    <uses-permission android:name="android.permission.INTERNET" />\n    <uses-permission android:name="android.permission.ACCESS_NETWORK_STATE" />\n    <uses-permission android:name="android.permission.ACCESS_WIFI_STATE" />\n    <uses-permission android:name="android.permission.ACCESS_FINE_LOCATION" />\n    <uses-permission android:name="android.permission.ACCESS_COARSE_LOCATION" />\n</manifest>|' app/src/main/AndroidManifest.xml


# We need the output library to be named libmain.so so SDLActivity finds it.
cat << 'EOF' > app/jni/CMakeLists.txt
cmake_minimum_required(VERSION 3.18)
project(hamclock_android)

# Disable unsupported features for this build
set(ENABLE_DEBUG_API OFF CACHE BOOL "Disable debug API")

# Default to info-level logging on Android so PSK/WSPR fetches appear in logcat
# without requiring a rooted device. Override by passing HC_LOG_LEVEL=debug to this script.
set(HC_LOG_LEVEL "$ENV{HC_LOG_LEVEL}" CACHE STRING "Log level override")
if(NOT HC_LOG_LEVEL)
    set(HC_LOG_LEVEL "info" CACHE STRING "Log level override" FORCE)
endif()

# Fix Ninja / Android NDK 'multiple rules generate resolve' collisions
# by aggressively disabling all tests and tools in third-party libraries (Curl, mbedtls, etc.)
set(BUILD_TESTING OFF CACHE BOOL "Disable tests" FORCE)
set(ENABLE_TESTING OFF CACHE BOOL "Disable testing" FORCE)
set(BUILD_CURL_EXE OFF CACHE BOOL "Disable curl executable" FORCE)

# Intercept PkgConfig to prevent host (Docker) libdrm leakage into the cross-compile environment
# (The Android image has libdrm natively, which causes CMake to mistakenly try mappingkmsdrm)
macro(pkg_check_modules)
    message(STATUS "Android build: PkgConfig overridden/disabled")
endmacro()

# Import the root CMakeLists.txt
add_subdirectory(../../../ hamclock-next)

# Android's SDLActivity natively loads "libmain.so", so we rename our target output.
# add_executable implicitly acts as a shared library under the NDK wrapper wrapper.
set_target_properties(hamclock-next PROPERTIES OUTPUT_NAME "main")
EOF

sed -i "s/set(ENABLE_DEBUG_API OFF/set(ENABLE_DEBUG_API ${DEBUG_API_BOOL}/g" app/jni/CMakeLists.txt

KEYSTORE_PATH="$REPO_ROOT/packaging/android-release.keystore"
KEYSTORE_ALIAS="hamclock-next"
KEYSTORE_PASS="hamclock-next-dev"

# Add release signing config so the same key signs every build.
# This lets adb install -r work without uninstalling between builds.
# The keystore lives in packaging/ and is generated once (or reused if it exists).
cat << 'GRADLE_SIGN_EOF' >> app/build.gradle

android {
    signingConfigs {
        release {
            storeFile file('/repo/packaging/android-release.keystore')
            storePassword 'hamclock-next-dev'
            keyAlias 'hamclock-next'
            keyPassword 'hamclock-next-dev'
        }
    }
    buildTypes {
        release {
            minifyEnabled false
            signingConfig signingConfigs.release
        }
    }
}
GRADLE_SIGN_EOF

echo "[3/3] Building APK with mingc/android-build-box:latest Docker container..."
# The container will use Gradle to parse CMake and compile our repo natively for arm64/arm32.
docker run --rm \
    -v "$REPO_ROOT":/repo \
    -w /repo/build-android \
    -e LOCAL_USER_ID="$(id -u)" \
    mingc/android-build-box:latest \
    bash -c "
        chmod +x gradlew
        # Avoid local.properties errors in Docker
        echo 'sdk.dir=/opt/android-sdk' > local.properties

        # Upgrade Gradle to 8.7 to fix Java 21 (class version 65) compatibility in the build-box
        sed -i 's/gradle-.*-bin.zip/gradle-8.7-bin.zip/g' gradle/wrapper/gradle-wrapper.properties

        # Generate a persistent dev keystore in packaging/ if it does not exist.
        # The same keystore is reused on every subsequent build so adb install -r works.
        if [ ! -f /repo/packaging/android-release.keystore ]; then
            echo 'Generating development signing keystore (one-time)...'
            keytool -genkeypair -v \
                -keystore /repo/packaging/android-release.keystore \
                -alias hamclock-next \
                -keyalg RSA -keysize 2048 -validity 10000 \
                -dname 'CN=HamClock-Next Dev, OU=Dev, O=Dev, L=Local, S=Dev, C=US' \
                -storepass hamclock-next-dev \
                -keypass hamclock-next-dev 2>/dev/null
        fi

        ./gradlew assembleRelease

        # Return ownership of all build output to the calling user so the next
        # run can rm -rf build-android/ without hitting permission-denied errors.
        # LOCAL_USER_ID is passed in from the host via -e above.
        chown -R \"\${LOCAL_USER_ID}\" /repo/build-android
    "

echo ""
echo "=== Android Build Complete ==="
echo "Your APK is located at:"
echo "$BUILD_DIR/app/build/outputs/apk/release/app-release.apk"
echo ""
echo "To sideload it onto your device, enable Developer Options and run:"
echo "  adb install -r build-android/app/build/outputs/apk/release/app-release.apk"
