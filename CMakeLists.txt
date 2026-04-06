cmake_minimum_required(VERSION 3.18)

# Read version from file
file(READ "${CMAKE_CURRENT_SOURCE_DIR}/VERSION" VER_NUM)
string(STRIP "${VER_NUM}" VER_NUM)
file(READ "${CMAKE_CURRENT_SOURCE_DIR}/VERSION_SUFFIX" VER_SUFFIX)
string(STRIP "${VER_SUFFIX}" VER_SUFFIX)

# Calculate short display version (e.g. 0.8B if version is 0.8.0)
string(REGEX REPLACE "\\.0$" "" SHORT_VER "${VER_NUM}")
set(HAMCLOCK_DISPLAY_VERSION "${SHORT_VER}${VER_SUFFIX}")

# libpredict v2.0.0 uses cmake_minimum_required(VERSION 2.8) which modern CMake rejects
set(CMAKE_POLICY_VERSION_MINIMUM 3.5 CACHE STRING "Compat for old FetchContent deps")

project(HamClock-Next
    VERSION ${VER_NUM}
    LANGUAGES C CXX
)

if(EMSCRIPTEN OR IOS)
    # Use C++17 for WASM/iOS to avoid fmt/spdlog consteval issues
    set(CMAKE_CXX_STANDARD 17)
else()
    set(CMAKE_CXX_STANDARD 20)
endif()
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# ── iOS cross-compile config ──────────────────────────────────────────────
if(IOS)
    set(CMAKE_OSX_DEPLOYMENT_TARGET "16.0" CACHE STRING "" FORCE)
    # Use Apple SecureTransport (built into iOS) instead of mbedtls for CURL TLS
    set(CURL_USE_SECTRANSP ON CACHE BOOL "" FORCE)
    set(ENABLE_MBEDTLS OFF CACHE BOOL "" FORCE)
    # Suppress tests in third-party deps (same pattern as Android)
    set(BUILD_TESTING OFF CACHE BOOL "" FORCE)
    set(ENABLE_TESTING OFF CACHE BOOL "" FORCE)
    set(BUILD_CURL_EXE OFF CACHE BOOL "" FORCE)
    # Prevent host pkg-config from leaking macOS libraries into the iOS sysroot
    macro(pkg_check_modules)
        message(STATUS "iOS build: pkg_check_modules disabled")
    endmacro()
    # Pre-cache ApplicationServices to empty so find_library() in FreeType and
    # HarfBuzz (pulled in by SDL2_ttf) skips the macOS host filesystem search.
    # Both variable names are used across different library versions.
    set(APPLICATIONSERVICES_LIBRARY "" CACHE FILEPATH "Disabled on iOS" FORCE)
    set(APPLICATION_SERVICES_FRAMEWORK "" CACHE FILEPATH "Disabled on iOS" FORCE)
    set(CORESERVICES_LIBRARY "" CACHE FILEPATH "Disabled on iOS" FORCE)
    set(CORE_SERVICES_FRAMEWORK "" CACHE FILEPATH "Disabled on iOS" FORCE)

    # Disable IMAGEIO backend for SDL2_image on iOS to avoid hardcoded ApplicationServices framework.
    # iOS has ImageIO, but the SDL2_image CMakeLists.txt hardcodes the macOS-only ApplicationServices
    # umbrella framework instead of linking ImageIO directly. Fall back to stb_image.
    set(SDL2IMAGE_BACKEND_IMAGEIO OFF CACHE BOOL "Disabled on iOS" FORCE)
endif()

# Target architecture detection
if(NOT HAMCLOCK_ARCH)
    set(HAMCLOCK_ARCH "${CMAKE_SYSTEM_PROCESSOR}")
endif()

# Installation type awareness (default to BIN if not specified)
if(NOT HAMCLOCK_INSTALL_TYPE)
    if(EMSCRIPTEN)
        set(HAMCLOCK_INSTALL_TYPE "WASM")
    else()
        set(HAMCLOCK_INSTALL_TYPE "BIN")
    endif()
endif()

add_compile_definitions(HAMCLOCK_VERSION="v${HAMCLOCK_DISPLAY_VERSION}")
add_compile_definitions(HAMCLOCK_INSTALL_TYPE="${HAMCLOCK_INSTALL_TYPE}")
add_compile_definitions(HAMCLOCK_ARCH="${HAMCLOCK_ARCH}")

# Pthreads are not enabled for WASM: the app is single-threaded in that
# context and enabling them would require SharedArrayBuffer + COOP/COEP HTTP
# headers.  See the hamclock-wasm target below for details.

# Force static builds globally for all FetchContent dependencies
set(BUILD_SHARED_LIBS OFF CACHE BOOL "Build static libs" FORCE)
set(SDL_SHARED OFF CACHE BOOL "Build shared SDL library" FORCE)
set(SDL_STATIC ON CACHE BOOL "Build static SDL library" FORCE)
set(SDL2_SHARED OFF CACHE BOOL "Build shared SDL library" FORCE)
set(SDL2_STATIC ON CACHE BOOL "Build static SDL library" FORCE)
set(SDL2TTF_BUILD_SHARED_LIBS OFF CACHE BOOL "Build shared SDL2_ttf library" FORCE)
set(SDL2IMAGE_BUILD_SHARED_LIBS OFF CACHE BOOL "Build shared SDL2_image library" FORCE)


# --- Build Options ---
option(ENABLE_DEBUG_API "Enable debug API endpoints and live view (increases CPU usage)" OFF)
option(ENABLE_ASAN "Enable AddressSanitizer + UBSan (Debug only)" OFF)
set(HC_LOG_LEVEL "" CACHE STRING "Override compiled-in default log level (trace/debug/info/warn/error). Empty = warn.")

if(HC_LOG_LEVEL)
    add_compile_definitions(HC_LOG_LEVEL="${HC_LOG_LEVEL}")
    message(STATUS "Log level override: ${HC_LOG_LEVEL}")
endif()

if(ENABLE_DEBUG_API)
    add_compile_definitions(ENABLE_DEBUG_API)
    message(STATUS "Debug API enabled (live view, debug endpoints)")
else()
    message(STATUS "Debug API disabled (production build)")
endif()

# --- Dependencies (FetchContent - The "Universal Build" Strategy) ---
include(FetchContent)

if(NOT EMSCRIPTEN)
    # 1. SDL2 (Base) - Always build from source for consistency and static linking
    if(TRUE) # Force FetchContent
        set(SDL_STATIC ON CACHE BOOL "Build static SDL library")
        set(SDL_SHARED OFF CACHE BOOL "Build shared SDL library")
        set(SDL2_SHARED OFF CACHE BOOL "Build shared SDL library" FORCE)
        set(SDL2_STATIC ON CACHE BOOL "Build static SDL library" FORCE)
        # ...
        set(SDL_TEST OFF CACHE BOOL "Disable SDL tests")
        if(NOT ANDROID)
            set(SDL_JOYSTICK OFF CACHE BOOL "Disable SDL joystick")
            set(SDL_HAPTIC OFF CACHE BOOL "Disable SDL haptic")
            set(SDL_SENSOR OFF CACHE BOOL "Disable SDL sensor")
        endif()

        set(SDL_PIPEWIRE OFF CACHE BOOL "Disable SDL PipeWire audio")

        FetchContent_Declare(
            SDL2
            GIT_REPOSITORY https://github.com/libsdl-org/SDL.git
            GIT_TAG        release-2.30.2
            GIT_SHALLOW    TRUE
        )
    endif()
endif()

if(NOT EMSCRIPTEN)
    # 2. SDL2_ttf - Always build from source
    if(TRUE)
        set(SDL2TTF_VENDORED ON CACHE BOOL "Use vendored dependencies for SDL2_ttf")
        set(SDL2TTF_BUILD_SHARED_LIBS OFF CACHE BOOL "Build shared SDL2_ttf library")
        FetchContent_Declare(
            SDL2_ttf
            GIT_REPOSITORY https://github.com/libsdl-org/SDL_ttf.git
            GIT_TAG        release-2.22.0
            GIT_SHALLOW    TRUE
        )
    endif()
endif()

if(NOT EMSCRIPTEN)
    # 3. SDL2_image - Always build from source
    if(TRUE)
        set(SDL2IMAGE_VENDORED ON CACHE BOOL "Use vendored dependencies for SDL2_image")
        set(SDL2IMAGE_SAMPLES OFF CACHE BOOL "Disable SDL2_image samples")
        set(SDL2IMAGE_BUILD_SHARED_LIBS OFF CACHE BOOL "Build shared SDL2_image library")
        set(SDL2IMAGE_WEBP OFF CACHE BOOL "Disable WebP to avoid MinGW link errors")

        set(SDL2IMAGE_TIF OFF CACHE BOOL "Disable TIFF - not used")
        set(SDL2IMAGE_JXL OFF CACHE BOOL "Disable JXL - not used")
        set(SDL2IMAGE_AVIF OFF CACHE BOOL "Disable AVIF - not used")
        FetchContent_Declare(
            SDL2_image
            GIT_REPOSITORY https://github.com/libsdl-org/SDL_image.git
            GIT_TAG        release-2.8.2
            GIT_SHALLOW    TRUE
            GIT_SUBMODULES ""
        )
    endif()
endif()

if(NOT EMSCRIPTEN)
    # 4. SDL2_mixer - Always build from source
    if(TRUE)
        set(SDL2MIXER_VENDORED ON CACHE BOOL "Use vendored dependencies for SDL2_mixer")
        set(SDL2MIXER_SAMPLES OFF CACHE BOOL "Disable SDL2_mixer samples")
        set(SDL2MIXER_BUILD_SHARED_LIBS OFF CACHE BOOL "Build shared SDL2_mixer library")
        set(SDL2MIXER_OPUS OFF CACHE BOOL "Disable OPUS")
        set(SDL2MIXER_FLAC OFF CACHE BOOL "Disable FLAC")
        set(SDL2MIXER_MOD OFF CACHE BOOL "Disable MOD")
        set(SDL2MIXER_MIDI OFF CACHE BOOL "Disable MIDI")
        set(SDL2MIXER_MP3 OFF CACHE BOOL "Disable MP3")
        set(SDL2MIXER_WAVPACK OFF CACHE BOOL "Disable Wavpack")
        FetchContent_Declare(
            SDL2_mixer
            GIT_REPOSITORY https://github.com/libsdl-org/SDL_mixer.git
            GIT_TAG        release-2.8.0
            GIT_SHALLOW    TRUE
        )
    endif()
endif()

# 4. mbedtls (for static SSL on Linux/RPi/Android — cross-compiles cleanly with NDK)
# iOS uses SecureTransport (built into the OS) — mbedtls is not needed.
if(NOT WIN32 AND NOT EMSCRIPTEN AND NOT IOS)
    set(ENABLE_TESTING OFF CACHE BOOL "Disable mbedtls tests")
    set(ENABLE_PROGRAMS OFF CACHE BOOL "Disable mbedtls programs")
    set(MBEDTLS_FATAL_WARNINGS OFF CACHE BOOL "No fatal warnings")
    FetchContent_Declare(
        mbedtls
        GIT_REPOSITORY https://github.com/ARMmbed/mbedtls.git
        GIT_TAG        v3.6.0
        GIT_SHALLOW    TRUE
    )
    FetchContent_MakeAvailable(mbedtls)
    # Explicitly suppress warnings for mbedtls targets
    foreach(target mbedtls mbedx509 mbedcrypto)
        if(TARGET ${target})
            if(MSVC)
                target_compile_options(${target} PRIVATE /w)
            else()
                target_compile_options(${target} PRIVATE -w)
            endif()
        endif()
    endforeach()
    # Export paths for CURL's FindMbedTLS using absolute paths to bypass export set checks
    set(MBEDTLS_INCLUDE_DIR "${mbedtls_SOURCE_DIR}/include" CACHE PATH "" FORCE)
    set(MBEDTLS_LIBRARY "${mbedtls_BINARY_DIR}/library/libmbedtls.a" CACHE FILEPATH "" FORCE)
    set(MBEDX509_LIBRARY "${mbedtls_BINARY_DIR}/library/libmbedx509.a" CACHE FILEPATH "" FORCE)
    set(MBEDCRYPTO_LIBRARY "${mbedtls_BINARY_DIR}/library/libmbedcrypto.a" CACHE FILEPATH "" FORCE)
endif()

# 5. CURL
if(NOT EMSCRIPTEN)
    if(WIN32)
        # Windows always uses vendored static build
        set(USE_SYSTEM_CURL OFF)
    elseif(IOS)
        # iOS cross-compile: system CURL is macOS-targeted; always build from source
        set(USE_SYSTEM_CURL OFF)
    else()
        # On Linux/macOS, check for system CURL first
        find_package(CURL)
        if(CURL_FOUND)
            message(STATUS "Using System Curl: ${CURL_VERSION_STRING}")
            set(USE_SYSTEM_CURL ON)
        else()
            message(STATUS "System Curl not found, building from source")
            set(USE_SYSTEM_CURL OFF)
        endif()
    endif()

    if(NOT USE_SYSTEM_CURL)
        set(CURL_DISABLE_INSTALL ON CACHE BOOL "Disable curl installation")
        set(BUILD_CURL_EXE OFF CACHE BOOL "Disable curl executable")
        set(BUILD_SHARED_LIBS OFF CACHE BOOL "Build static libs" FORCE)
        set(HTTP_ONLY ON CACHE BOOL "Build HTTP-only curl")
        if(WIN32)
            set(CURL_USE_OPENSSL OFF CACHE BOOL "Disable OpenSSL on Windows (use Schannel)")
            set(CURL_USE_SCHANNEL ON CACHE BOOL "Use Windows native SSL")
        elseif(IOS)
            set(CURL_USE_OPENSSL OFF CACHE BOOL "Disable OpenSSL")
            set(CURL_USE_MBEDTLS OFF CACHE BOOL "Disable mbedtls on iOS" FORCE)
            set(CURL_USE_SECTRANSP ON CACHE BOOL "Use iOS/macOS SecureTransport" FORCE)
            set(CURL_USE_SCHANNEL OFF CACHE BOOL "Disable Schannel on non-Windows")
            # Prevent CURL from finding host (macOS) libraries that have no iOS equivalent
            set(USE_LIBIDN2 OFF CACHE BOOL "Disable libidn2 on iOS" FORCE)
            set(CURL_USE_LIBPSL OFF CACHE BOOL "Disable libpsl on iOS" FORCE)
            set(CURL_USE_LIBSSH2 OFF CACHE BOOL "Disable libssh2 on iOS" FORCE)
        else()
            set(CURL_USE_OPENSSL OFF CACHE BOOL "Disable OpenSSL")
            set(CURL_USE_MBEDTLS ON CACHE BOOL "Use mbedtls for static build")
            set(CURL_USE_SCHANNEL OFF CACHE BOOL "Disable Schannel on non-Windows")
        endif()
        set(CURL_CA_FALLBACK ON CACHE BOOL "Use system CA certificates")
        FetchContent_Declare(
            CURL
            GIT_REPOSITORY https://github.com/curl/curl.git
            GIT_TAG        curl-8_11_1
            GIT_SHALLOW    TRUE
        )
    endif()
endif()

# 5. SQLite3 (Using the official amalgamation)
FetchContent_Declare(
    sqlite3_amalgamation
    URL https://www.sqlite.org/2024/sqlite-amalgamation-3470000.zip
)

# 6. JSON
find_package(nlohmann_json QUIET)
if(NOT nlohmann_json_FOUND)
    FetchContent_Declare(
        nlohmann_json
        URL https://github.com/nlohmann/json/archive/refs/tags/v3.11.3.zip
    )
endif()

# 7. Predict
find_package(libpredict QUIET)
if(NOT libpredict_FOUND)
    FetchContent_Declare(
        libpredict
        GIT_REPOSITORY https://github.com/la1k/libpredict.git
        GIT_TAG        v2.0.0
    )
endif()

# 8. HTTP / Log
find_package(httplib QUIET)
if(NOT httplib_FOUND)
    FetchContent_Declare(
        httplib
        GIT_REPOSITORY https://github.com/yhirose/cpp-httplib.git
        GIT_TAG        v0.14.3
    )
endif()

# 9. Fmt & Spdlog
# Force FetchContent for these to avoid version mismatches with system libraries (like Linuxbrew)
# which can cause "basic_format_string not found" errors in spdlog/common.h.
FetchContent_Declare(
    fmt
    GIT_REPOSITORY https://github.com/fmtlib/fmt.git
    GIT_TAG        10.2.1
)
FetchContent_MakeAvailable(fmt)

set(SPDLOG_FMT_EXTERNAL ON CACHE BOOL "Use external fmt library" FORCE)
FetchContent_Declare(
    spdlog
    GIT_REPOSITORY https://github.com/gabime/spdlog.git
    GIT_TAG        v1.15.0
)
FetchContent_MakeAvailable(spdlog)


# --- Make Available (Order carefully) ---
# 1. Base dependencies
if(NOT EMSCRIPTEN)
    FetchContent_MakeAvailable(SDL2)
endif()
FetchContent_MakeAvailable(sqlite3_amalgamation)

if(NOT nlohmann_json_FOUND)
    FetchContent_MakeAvailable(nlohmann_json)
endif()

if(NOT httplib_FOUND)
    FetchContent_MakeAvailable(httplib)
endif()




# 2. SDL Extensions (Must come AFTER SDL2 is made available)
if(NOT EMSCRIPTEN)
    FetchContent_MakeAvailable(SDL2_ttf)
    FetchContent_MakeAvailable(SDL2_image)
    FetchContent_MakeAvailable(SDL2_mixer)
endif()


# Define SDL2 Aliases if missing
if(NOT TARGET SDL2::SDL2)
    if(TARGET SDL2-static)
        add_library(SDL2::SDL2 ALIAS SDL2-static)
    endif()
endif()
if(NOT TARGET SDL2::SDL2-static)
    if(TARGET SDL2-static)
        add_library(SDL2::SDL2-static ALIAS SDL2-static)
    endif()
endif()
if(NOT TARGET SDL2::SDL2main)
    if(TARGET SDL2main)
        add_library(SDL2::SDL2main ALIAS SDL2main)
    endif()
endif()

# Propagate include dirs
if(NOT EMSCRIPTEN)
    get_target_property(SDL2_INCLUDE_DIRS SDL2-static INTERFACE_INCLUDE_DIRECTORIES)
    set(SDL2_INCLUDE_DIR ${SDL2_INCLUDE_DIRS} CACHE PATH "SDL2 include dir" FORCE)
endif()

if(NOT httplib_FOUND)
    FetchContent_MakeAvailable(httplib)
    if(NOT TARGET httplib)
        add_library(httplib ALIAS cpp-httplib::cpp-httplib)
    endif()
else()
    if(NOT TARGET httplib)
        add_library(httplib ALIAS cpp-httplib::cpp-httplib)
    endif()
endif()



if(NOT USE_SYSTEM_CURL AND NOT EMSCRIPTEN)
    if(ENABLE_DEBUG_API)
        message(STATUS "Integrating Curl for debug API")
    endif()
    FetchContent_MakeAvailable(CURL)
    if(NOT TARGET libcurl)
        add_library(libcurl ALIAS CURL::libcurl)
    endif()
elseif(NOT EMSCRIPTEN)
    if(NOT TARGET libcurl)
        add_library(libcurl ALIAS CURL::libcurl)
    endif()
endif()

# Since the amalgamation ZIP doesn't have a CMakeLists.txt, we define the target here
if(NOT TARGET sqlite3)
    FetchContent_GetProperties(sqlite3_amalgamation)
    add_library(sqlite3 STATIC 
        ${sqlite3_amalgamation_SOURCE_DIR}/sqlite3.c
    )
    target_include_directories(sqlite3 SYSTEM PUBLIC ${sqlite3_amalgamation_SOURCE_DIR})
    if(MSVC)
        target_compile_options(sqlite3 PRIVATE /w)
    else()
        target_compile_options(sqlite3 PRIVATE -w)
    endif()
    if(UNIX)
        target_link_libraries(sqlite3 PRIVATE m dl)
    endif()
endif()


# SDL Extensions (Now safe to load, as SDL2::SDL2 is defined)
if(NOT EMSCRIPTEN)
    if(NOT SDL2_ttf_FOUND)
        FetchContent_MakeAvailable(SDL2_ttf)
    endif()
    if(NOT SDL2_image_FOUND)
        FetchContent_MakeAvailable(SDL2_image)
    endif()
    if(NOT SDL2_mixer_FOUND)
        FetchContent_MakeAvailable(SDL2_mixer)
    endif()
endif()

# Create ALIAS targets for Extensions (for consistency)
if(NOT TARGET SDL2_ttf::SDL2_ttf)
    if(TARGET SDL2_ttf)
        add_library(SDL2_ttf::SDL2_ttf ALIAS SDL2_ttf)
    endif()
endif()
if(NOT TARGET SDL2_ttf::SDL2_ttf-static)
    if(TARGET SDL2_ttf)
        add_library(SDL2_ttf::SDL2_ttf-static ALIAS SDL2_ttf)
    endif()
endif()

if(NOT TARGET SDL2_image::SDL2_image)
    if(TARGET SDL2_image)
        add_library(SDL2_image::SDL2_image ALIAS SDL2_image)
    endif()
endif()
if(NOT TARGET SDL2_image::SDL2_image-static)
    if(TARGET SDL2_image)
        add_library(SDL2_image::SDL2_image-static ALIAS SDL2_image)
    endif()
endif()

if(NOT TARGET SDL2_mixer::SDL2_mixer)
    if(TARGET SDL2_mixer)
        add_library(SDL2_mixer::SDL2_mixer ALIAS SDL2_mixer)
    endif()
endif()
if(NOT TARGET SDL2_mixer::SDL2_mixer-static)
    if(TARGET SDL2_mixer)
        add_library(SDL2_mixer::SDL2_mixer-static ALIAS SDL2_mixer)
    endif()
endif()

# Explicitly suppress warnings for SDL2 targets
foreach(target SDL2-static SDL2_ttf SDL2_image SDL2_mixer)
    if(TARGET ${target})
        if(MSVC)
            target_compile_options(${target} PRIVATE /w)
        else()
            target_compile_options(${target} PRIVATE -w)
        endif()
    endif()
endforeach()

# Special handling for libpredict (skipping tests)
FetchContent_GetProperties(libpredict)
if(NOT libpredict_POPULATED)
    FetchContent_Populate(libpredict)
    include(GNUInstallDirs)
    set(LIBPREDICT_INCLUDE_DIR ${libpredict_BINARY_DIR}/include)
    include_directories(AFTER ${LIBPREDICT_INCLUDE_DIR})
    add_subdirectory(${libpredict_SOURCE_DIR}/include ${libpredict_BINARY_DIR}/include)
    add_subdirectory(${libpredict_SOURCE_DIR}/src ${libpredict_BINARY_DIR}/src)
    if(APPLE)
        # Apple ld doesn't support --version-script; strip it from the predict shared target
        set_target_properties(predict PROPERTIES LINK_FLAGS "")
    endif()
endif()


# --- System Libraries (non-FetchContent) ---
set(THREADS_PREFER_PTHREAD_FLAG ON)
find_package(Threads REQUIRED)

# On some platforms (like RPi 32-bit), we need to explicitly link atomic for C++20
if(CMAKE_SYSTEM_NAME STREQUAL "Linux" AND (CMAKE_SYSTEM_PROCESSOR MATCHES "arm" OR CMAKE_SYSTEM_PROCESSOR MATCHES "aarch64"))
    link_libraries(atomic)
endif()

# Find LibDRM (for DisplayPower KMS/DRM support)
if(NOT EMSCRIPTEN AND NOT WIN32 AND NOT APPLE)
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(LIBDRM QUIET libdrm)
    if(LIBDRM_FOUND)
        message(STATUS "Found libdrm: ${LIBDRM_VERSION}")
        add_compile_definitions(HAS_LIBDRM)
    else()
        message(WARNING "libdrm not found. DRM DPMS support will be disabled.")
    endif()
endif()


# --- Application Target ---

set(HAMCLOCK_SOURCES
    src/main.cpp
    src/core/DashboardContext.cpp
    src/core/Theme.cpp
    src/core/Logger.cpp
    src/core/ConfigManager.cpp
    src/core/DatabaseManager.cpp
    src/core/OrbitPredictor.cpp
    src/core/DXClusterData.cpp
    src/core/DisplayPower.cpp
    src/core/GreylineCalculator.cpp
    src/core/BrightnessManager.cpp
    src/core/CPUMonitor.cpp
    src/core/SatelliteManager.cpp
    src/core/PrefixManager.cpp
    src/core/CitiesManager.cpp
    src/core/StringUtils.cpp
    src/core/SoundManager.cpp
    src/core/WorkerService.cpp
    src/core/PropEngine.cpp
    src/core/ActivityLocationManager.cpp
    src/network/FrameCapture.cpp
    src/network/NetworkManager.cpp
    src/network/WebServer.cpp
    src/network/WebServer_Routes.cpp
    src/services/UpdateChecker.cpp
    src/services/NOAAProvider.cpp
    src/services/RSSProvider.cpp
    src/services/BME280Provider.cpp
    src/services/LTR329Provider.cpp
    src/services/ADIFProvider.cpp
    src/services/ActivityProvider.cpp
    src/services/DRAPProvider.cpp
    src/services/LiveSpotProvider.cpp
    src/services/ReachProvider.cpp
    src/services/DXClusterProvider.cpp
    src/services/AuroraProvider.cpp
    src/services/MufRtProvider.cpp
    src/services/WxMbProvider.cpp
    src/services/GribCloudProvider.cpp
    src/services/IonosondeProvider.cpp
    src/services/HistoryProvider.cpp
    src/services/BandConditionsProvider.cpp
    src/services/BeaconProvider.cpp
    src/services/ContestProvider.cpp
    src/services/MoonProvider.cpp
    src/services/SDOProvider.cpp
    src/services/WeatherProvider.cpp
    src/services/CallbookProvider.cpp
    src/services/FccProvider.cpp
    src/services/DstProvider.cpp
    src/services/SantaProvider.cpp
    src/services/TropoProvider.cpp
    src/services/LightningProvider.cpp
    src/services/MeteorProvider.cpp
    src/services/IonosondeProvider.cpp
    src/services/SolarStormProvider.cpp
    src/services/RotatorService.cpp
    src/services/RigService.cpp
    src/services/GPSProvider.cpp
    src/services/AsteroidProvider.cpp
    src/core/AsteroidPropagator.cpp
    src/services/RBNProvider.cpp
    src/services/QRZProvider.cpp
    src/services/AlertsProvider.cpp
    src/services/ForecastProvider.cpp
    src/services/RepeaterProvider.cpp
    src/services/HurricaneProvider.cpp
    src/services/MarineProvider.cpp
    src/services/WinlinkProvider.cpp
    src/services/GreylineDXProvider.cpp
    src/services/SpaceWeatherAlertProvider.cpp
    src/ui/ActivityPanels.cpp
    src/ui/ADIFPanel.cpp
    src/ui/AlertsPanel.cpp
    src/ui/BigClockPanel.cpp
    src/ui/CallsignClock.cpp
    src/ui/SolarCyclePanel.cpp
    src/ui/GreylineWindowsPanel.cpp
    src/ui/DXCCProgressPanel.cpp
    src/ui/SpaceWeatherAlertsPanel.cpp
    src/ui/NOAASpaceWxPanel.cpp
    src/ui/VoacapDeDxPanel.cpp
    src/ui/AsteroidPanel.cpp
    src/ui/ForecastPanel.cpp
    src/ui/HurricanePanel.cpp
    src/ui/MarinePanel.cpp
    src/ui/RepeaterPanel.cpp
    src/ui/WinlinkPanel.cpp
    src/ui/GreylineDXPanel.cpp
    src/ui/AuroraPanel.cpp
    src/ui/AuroraGraphPanel.cpp
    src/ui/KIndexAlertPanel.cpp
    src/ui/SFITrendPanel.cpp
    src/ui/DXClusterPanel.cpp
    src/ui/DXClusterSetup.cpp
    src/ui/BandConditionsPanel.cpp
    src/ui/BeaconPanel.cpp
    src/ui/ContestPanel.cpp
    src/ui/CountdownPanel.cpp
    src/ui/SysInfoPanel.cpp
    src/ui/DRAPPanel.cpp
    src/ui/GimbalPanel.cpp
    src/ui/GreylineModal.cpp
    src/ui/PresetsModal.cpp
    src/ui/HistoryPanel.cpp
    src/ui/MoonPanel.cpp
    src/ui/SDOPanel.cpp
    src/ui/WeatherPanel.cpp
    src/ui/ClockAuxPanel.cpp
    src/ui/ENVPanel.cpp
    src/ui/RigControlPanel.cpp
    src/ui/DXInfo.cpp
    src/ui/SatWidget.cpp
    src/ui/ListPanel.cpp
    src/ui/LiveSpotPanel.cpp
    src/ui/LocalPanel.cpp
    src/ui/MapWidget.cpp
    src/ui/MapWidget_Input.cpp
    src/ui/MapWidget_Overlays.cpp
    src/ui/MapViewMenu.cpp
    src/ui/PaneContainer.cpp
    src/ui/PlaceholderWidget.cpp
    src/ui/PropColorCustomizer.cpp
    src/ui/WidgetRegistry.cpp
    src/ui/ColorPicker.cpp
    src/ui/RenderUtils.cpp
    src/ui/RSSBanner.cpp
    src/ui/SatelliteSetup.cpp
    src/ui/SatPanel.cpp
    src/ui/SetupScreen.cpp
    src/ui/SetupScreen_Appearance.cpp
    src/ui/SetupScreen_Config.cpp
    src/ui/SetupScreen_Events.cpp
    src/ui/SetupScreen_Identity.cpp
    src/ui/SetupScreen_Network.cpp
    src/ui/SetupScreen_Rig.cpp
    src/ui/SetupScreen_Services.cpp
    src/ui/SetupScreen_Spotting.cpp
    src/ui/SetupScreen_Update.cpp
    src/ui/SetupScreen_Watchlist.cpp
    src/ui/SetupScreen_Widgets.cpp
    src/ui/TextInput.cpp
    src/ui/SpaceWeatherPanel.cpp
    src/ui/ThemeCustomizer.cpp
    src/ui/TimePanel.cpp
    src/ui/TropoPanel.cpp
    src/ui/LightningPanel.cpp
    src/ui/MeteorPanel.cpp
    src/ui/IonosondePanel.cpp
    src/ui/SolarStormPanel.cpp
    src/ui/SolarTimelinePanel.cpp
    src/ui/CalendarPanel.cpp
    src/ui/StopwatchPanel.cpp
    src/ui/ReminderPanel.cpp
    src/ui/UpdateOverlay.cpp
    src/ui/WidgetSelector.cpp
    src/ui/CallbookPanel.cpp
    src/ui/DstPanel.cpp
    src/ui/WatchlistPanel.cpp
    src/ui/EMEToolPanel.cpp
    src/ui/SantaPanel.cpp
)

if(EMSCRIPTEN)
    set(CMAKE_EXECUTABLE_SUFFIX ".html")
    
    add_executable(hamclock-wasm ${HAMCLOCK_SOURCES})
    
    # Emscripten Flags for SDL2 and Memory.
    # Pthreads are NOT enabled for the WASM build: the app is single-threaded
    # in this context (all threaded services are #ifndef __EMSCRIPTEN__) and
    # enabling pthreads would require SharedArrayBuffer, which in turn requires
    # COOP/COEP HTTP headers on every server that serves the app.  Removing
    # pthreads lets the app be served from any plain HTTP server (nginx, Apache,
    # Python's http.server, etc.) without special header configuration.
    target_link_options(hamclock-wasm PRIVATE
        "-sUSE_SDL=2"
        "-sUSE_SDL_IMAGE=2"
        "-sSDL2_IMAGE_FORMATS=['png','jpg']"
        "-sUSE_SDL_TTF=2"
        "-sUSE_SDL_MIXER=2"
        "-sALLOW_MEMORY_GROWTH=1"
        "-sINITIAL_MEMORY=536870912"
        "-sASSERTIONS=1"
        "-sFETCH=1"
        "-lidbfs.js"
        "-sEXPORTED_RUNTIME_METHODS=['callMain','FS']"
        "-sEXPORTED_FUNCTIONS=['_main','_hamclock_after_idbfs']"
        "--shell-file" "${CMAKE_SOURCE_DIR}/packaging/web/shell.html"
    )

    target_include_directories(hamclock-wasm PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/include
        ${CMAKE_CURRENT_SOURCE_DIR}/src
        ${libpredict_BINARY_DIR}/include
    )

    target_compile_options(hamclock-wasm PRIVATE
        "-sUSE_SDL=2"
        "-sUSE_SDL_TTF=2"
        "-sUSE_SDL_IMAGE=2"
        "-sUSE_SDL_MIXER=2"
    )

    # Link non-SDL dependencies (SDL is handled by flags)
    target_link_libraries(hamclock-wasm PRIVATE
        sqlite3
        nlohmann_json::nlohmann_json
        predict_static
        httplib::httplib
        fmt::fmt
        spdlog::spdlog
    )

else()
    # Desktop Build
    if(WIN32)
        set(HC_WIN32_FLAG WIN32)
    endif()

    if(ANDROID)
        add_library(hamclock-next SHARED ${HAMCLOCK_SOURCES})
    else()
        add_executable(hamclock-next ${HC_WIN32_FLAG} ${HAMCLOCK_SOURCES})
    endif()

    target_include_directories(hamclock-next PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/include
        ${CMAKE_CURRENT_SOURCE_DIR}/src
    )
    target_include_directories(hamclock-next SYSTEM PRIVATE
        ${libpredict_BINARY_DIR}/include
        ${SDL2_INCLUDE_DIRS}
        ${LIBDRM_INCLUDE_DIRS}
    )

    target_link_libraries(hamclock-next PRIVATE
        SDL2::SDL2
        SDL2::SDL2main
        SDL2_ttf
        SDL2_image
        SDL2_mixer
        libcurl
        sqlite3
        nlohmann_json::nlohmann_json
        predict_static
        Threads::Threads
        httplib::httplib
        spdlog::spdlog
        ${LIBDRM_LIBRARIES}
    )

    # TTS: platform-native voice alerts
    if(APPLE AND NOT IOS)
        message(STATUS "TTS: macOS 'say' — voice alerts enabled")
        target_compile_definitions(hamclock-next PRIVATE HAMCLOCK_AUDIO_TTS)
    elseif(WIN32)
        message(STATUS "TTS: Windows SAPI — voice alerts enabled")
        target_compile_definitions(hamclock-next PRIVATE HAMCLOCK_AUDIO_TTS)
        target_link_libraries(hamclock-next PRIVATE ole32 oleaut32)
    else()
        # Linux: flite (optional)
        find_library(FLITE_LIB flite)
        find_library(FLITE_VOICE_LIB flite_cmu_us_kal16)
        find_path(FLITE_INCLUDE_DIR flite/flite.h)
        if(FLITE_LIB AND FLITE_VOICE_LIB AND FLITE_INCLUDE_DIR)
            message(STATUS "TTS: flite found — voice alerts enabled")
            target_compile_definitions(hamclock-next PRIVATE HAMCLOCK_AUDIO_TTS)
            target_link_libraries(hamclock-next PRIVATE
                ${FLITE_LIB} ${FLITE_VOICE_LIB} flite_usenglish flite_cmulex)
            target_include_directories(hamclock-next PRIVATE ${FLITE_INCLUDE_DIR})
        else()
            message(STATUS "TTS: flite not found — voice alerts disabled (install flite1-dev)")
        endif()
    endif()

    if(WIN32)
        add_definitions(-D_WIN32_WINNT=0x0601) # Target Windows 7+
        target_link_libraries(hamclock-next PRIVATE ws2_32 psapi iphlpapi)
    elseif(CMAKE_SYSTEM_NAME STREQUAL "Linux" AND CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
        # On Linux with GCC, link libstdc++ and libgcc statically for better portability
        target_link_options(hamclock-next PRIVATE -static-libstdc++ -static-libgcc)
    endif()

    # Compiler warnings (non-Emscripten only; -Werror omitted until warnings are resolved)
    if(MSVC)
        target_compile_options(hamclock-next PRIVATE /W4)
    else()
        target_compile_options(hamclock-next PRIVATE -Wall -Wextra -Wno-unused-parameter)
    endif()

    # AddressSanitizer + UBSan (opt-in, Debug builds only, Linux/macOS)
    if(ENABLE_ASAN AND NOT WIN32)
        target_compile_options(hamclock-next PRIVATE -fsanitize=address,undefined -fno-omit-frame-pointer)
        target_link_options(hamclock-next PRIVATE -fsanitize=address,undefined)
        message(STATUS "AddressSanitizer + UBSan enabled")
    endif()

    if(APPLE AND NOT IOS)
        # Clean up any leaked flags from dependencies that might have touched global linker flags
        string(REPLACE "-static-libgcc" "" CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS}")
        string(REPLACE "-static-libstdc++" "" CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS}")

        # macOS App Bundle Configuration
        set_target_properties(hamclock-next PROPERTIES
            MACOSX_BUNDLE TRUE
            MACOSX_BUNDLE_GUI_IDENTIFIER com.hamclock.next
            MACOSX_BUNDLE_BUNDLE_NAME "HamClock-Next"
            MACOSX_BUNDLE_BUNDLE_VERSION "${PROJECT_VERSION}"
            MACOSX_BUNDLE_SHORT_VERSION_STRING "${PROJECT_VERSION}"
            MACOSX_BUNDLE_ICON_FILE "HamClockNext"
            MACOSX_BUNDLE_INFO_PLIST "${CMAKE_SOURCE_DIR}/packaging/macos/Info.plist.in"
        )

        # Copy icon into bundle
        set_source_files_properties(
            "${CMAKE_SOURCE_DIR}/packaging/macos/HamClockNext.icns"
            PROPERTIES
            MACOSX_PACKAGE_LOCATION "Resources"
        )

        target_sources(hamclock-next PRIVATE
            "${CMAKE_SOURCE_DIR}/packaging/macos/HamClockNext.icns"
        )
    endif()

    if(IOS)
        set_target_properties(hamclock-next PROPERTIES
            MACOSX_BUNDLE TRUE
            MACOSX_BUNDLE_GUI_IDENTIFIER "com.hamclock.next"
            MACOSX_BUNDLE_BUNDLE_NAME "HamClock-Next"
            MACOSX_BUNDLE_BUNDLE_VERSION "${PROJECT_VERSION}"
            MACOSX_BUNDLE_SHORT_VERSION_STRING "${PROJECT_VERSION}"
            XCODE_ATTRIBUTE_TARGETED_DEVICE_FAMILY "1,2"
            XCODE_ATTRIBUTE_IPHONEOS_DEPLOYMENT_TARGET "16.0"
            XCODE_ATTRIBUTE_CODE_SIGNING_REQUIRED "NO"
            XCODE_ATTRIBUTE_CODE_SIGNING_ALLOWED "NO"
            XCODE_ATTRIBUTE_UISupportedInterfaceOrientations "UIInterfaceOrientationLandscapeLeft UIInterfaceOrientationLandscapeRight"
            XCODE_ATTRIBUTE_UISupportedInterfaceOrientations_iPad "UIInterfaceOrientationLandscapeLeft UIInterfaceOrientationLandscapeRight"
        )
    endif()
    install(TARGETS hamclock-next
        RUNTIME DESTINATION bin
        BUNDLE DESTINATION .
    )
endif()

# --- Custom targets for data updates ---
find_package(Python3 COMPONENTS Interpreter)
if(Python3_Interpreter_FOUND)
    add_custom_target(update-prefixes
        COMMAND ${Python3_EXECUTABLE} ${CMAKE_SOURCE_DIR}/scripts/update_prefixes.py
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        COMMENT "Updating prefix database from country-files.com"
    )
    add_custom_target(update-cities
        COMMAND ${Python3_EXECUTABLE} ${CMAKE_SOURCE_DIR}/scripts/update_cities.py
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        COMMENT "Updating city database from geonames.org"
    )
    add_custom_target(update-data DEPENDS update-prefixes update-cities)
endif()
