cmake_minimum_required(VERSION 3.18)

# libpredict v2.0.0 uses cmake_minimum_required(VERSION 2.8) which modern CMake rejects
set(CMAKE_POLICY_VERSION_MINIMUM 3.5 CACHE STRING "Compat for old FetchContent deps")

project(HamClock-Next
    VERSION 0.5.0
    LANGUAGES C CXX
)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
add_compile_definitions(HAMCLOCK_VERSION="v0.5B")

# --- Build Options ---
option(ENABLE_DEBUG_API "Enable debug API endpoints and live view (increases CPU usage)" OFF)

if(ENABLE_DEBUG_API)
    add_compile_definitions(ENABLE_DEBUG_API)
    message(STATUS "Debug API enabled (live view, debug endpoints)")
else()
    message(STATUS "Debug API disabled (production build)")
endif()

# --- Dependencies (FetchContent - The "Universal Build" Strategy) ---
include(FetchContent)

# 1. SDL2 (Base)
set(SDL_STATIC ON CACHE BOOL "Build static SDL library")
set(SDL_SHARED OFF CACHE BOOL "Build shared SDL library")
set(SDL_TEST OFF CACHE BOOL "Disable SDL tests")
set(SDL_JOYSTICK OFF CACHE BOOL "Disable SDL joystick")
set(SDL_HAPTIC OFF CACHE BOOL "Disable SDL haptic")
set(SDL_SENSOR OFF CACHE BOOL "Disable SDL sensor")

set(SDL_PIPEWIRE OFF CACHE BOOL "Disable SDL PipeWire audio")

FetchContent_Declare(
    SDL2
    GIT_REPOSITORY https://github.com/libsdl-org/SDL.git
    GIT_TAG        release-2.30.2
    GIT_SHALLOW    TRUE
)

# 2. SDL2_ttf
set(SDL2TTF_VENDORED ON CACHE BOOL "Use vendored dependencies for SDL2_ttf")
FetchContent_Declare(
    SDL2_ttf
    GIT_REPOSITORY https://github.com/libsdl-org/SDL_ttf.git
    GIT_TAG        release-2.22.x
    GIT_SHALLOW    TRUE
)

# 3. SDL2_image
set(SDL2IMAGE_VENDORED ON CACHE BOOL "Use vendored dependencies for SDL2_image")
set(SDL2IMAGE_SAMPLES OFF CACHE BOOL "Disable SDL2_image samples")
set(SDL2IMAGE_WEBP OFF CACHE BOOL "Disable WebP to avoid MinGW link errors")
set(SDL2IMAGE_TIF OFF CACHE BOOL "Disable TIFF - not used")
set(SDL2IMAGE_JXL OFF CACHE BOOL "Disable JXL - not used")
set(SDL2IMAGE_AVIF OFF CACHE BOOL "Disable AVIF - not used")
FetchContent_Declare(
    SDL2_image
    GIT_REPOSITORY https://github.com/libsdl-org/SDL_image.git
    GIT_TAG        release-2.8.x
    GIT_SHALLOW    TRUE
)

# 4. mbedtls (for static SSL on Linux/RPi)
if(NOT WIN32)
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
    # Export paths for CURL's FindMbedTLS using absolute paths to bypass export set checks
    set(MBEDTLS_INCLUDE_DIR "${mbedtls_SOURCE_DIR}/include" CACHE PATH "" FORCE)
    set(MBEDTLS_LIBRARY "${mbedtls_BINARY_DIR}/library/libmbedtls.a" CACHE FILEPATH "" FORCE)
    set(MBEDX509_LIBRARY "${mbedtls_BINARY_DIR}/library/libmbedx509.a" CACHE FILEPATH "" FORCE)
    set(MBEDCRYPTO_LIBRARY "${mbedtls_BINARY_DIR}/library/libmbedcrypto.a" CACHE FILEPATH "" FORCE)
endif()

# 5. CURL
if(WIN32)
    # Windows always uses vendored static build
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
    set(BUILD_SHARED_LIBS OFF CACHE BOOL "Build static libs")
    set(HTTP_ONLY ON CACHE BOOL "Build HTTP-only curl")
    if(WIN32)
        set(CURL_USE_OPENSSL OFF CACHE BOOL "Disable OpenSSL on Windows (use Schannel)")
        set(CURL_USE_SCHANNEL ON CACHE BOOL "Use Windows native SSL")
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

# 5. SQLite3 (Using the official amalgamation)
FetchContent_Declare(
    sqlite3_amalgamation
    URL https://www.sqlite.org/2024/sqlite-amalgamation-3470000.zip
)

# 6. JSON / Predict / HTTP / Log
FetchContent_Declare(
    nlohmann_json
    GIT_REPOSITORY https://github.com/nlohmann/json.git
    GIT_TAG        v3.11.3
)
FetchContent_Declare(
    libpredict
    GIT_REPOSITORY https://github.com/la1k/libpredict.git
    GIT_TAG        v2.0.0
)
FetchContent_Declare(
    httplib
    GIT_REPOSITORY https://github.com/yhirose/cpp-httplib.git
    GIT_TAG        v0.14.3
)
FetchContent_Declare(
    spdlog
    GIT_REPOSITORY https://github.com/gabime/spdlog.git
    GIT_TAG        v1.12.0
)

# Order matters: SDL must be available before extensions
if(USE_SYSTEM_CURL)
    FetchContent_MakeAvailable(SDL2 sqlite3_amalgamation nlohmann_json httplib spdlog)
    add_library(libcurl ALIAS CURL::libcurl)
else()
    FetchContent_MakeAvailable(SDL2 CURL sqlite3_amalgamation nlohmann_json httplib spdlog)
endif()

# Since the amalgamation ZIP doesn't have a CMakeLists.txt, we define the target here
if(NOT TARGET sqlite3)
    FetchContent_GetProperties(sqlite3_amalgamation)
    add_library(sqlite3 STATIC 
        ${sqlite3_amalgamation_SOURCE_DIR}/sqlite3.c
    )
    target_include_directories(sqlite3 PUBLIC ${sqlite3_amalgamation_SOURCE_DIR})
    if(UNIX)
        target_link_libraries(sqlite3 PRIVATE m dl)
    endif()
endif()

# SDL Extensions need SDL2 targets to be defined
FetchContent_MakeAvailable(SDL2_ttf SDL2_image)

# Special handling for libpredict (skipping tests)
FetchContent_GetProperties(libpredict)
if(NOT libpredict_POPULATED)
    if(APPLE)
        # macOS ld doesn't support --version-script used by libpredict shared build
        set(PREDICT_SHARED OFF CACHE BOOL "Force static libpredict on macOS" FORCE)
    endif()
    FetchContent_Populate(libpredict)
    include(GNUInstallDirs)
    set(LIBPREDICT_INCLUDE_DIR ${libpredict_BINARY_DIR}/include)
    include_directories(AFTER ${LIBPREDICT_INCLUDE_DIR})
    add_subdirectory(${libpredict_SOURCE_DIR}/include ${libpredict_BINARY_DIR}/include)
    add_subdirectory(${libpredict_SOURCE_DIR}/src ${libpredict_BINARY_DIR}/src)
endif()


# --- System Libraries (non-FetchContent) ---
set(THREADS_PREFER_PTHREAD_FLAG ON)
find_package(Threads REQUIRED)

# On some platforms (like RPi 32-bit), we need to explicitly link atomic for C++20
if(CMAKE_SYSTEM_PROCESSOR MATCHES "arm" OR CMAKE_SYSTEM_PROCESSOR MATCHES "aarch64")
    link_libraries(atomic)
endif()


# --- Application Target ---

if(WIN32)
    set(HC_WIN32_FLAG WIN32)
endif()

add_executable(hamclock-next ${HC_WIN32_FLAG}
    src/main.cpp
    src/core/Logger.cpp
    src/core/ConfigManager.cpp
    src/core/DatabaseManager.cpp
    src/core/OrbitPredictor.cpp
    src/core/DXClusterData.cpp
    src/core/SatelliteManager.cpp
    src/core/PrefixManager.cpp
    src/core/CitiesManager.cpp
    src/network/NetworkManager.cpp
    src/network/WebServer.cpp
    src/services/NOAAProvider.cpp
    src/services/RSSProvider.cpp
    src/services/ADIFProvider.cpp
    src/services/ActivityProvider.cpp
    src/services/DRAPProvider.cpp
    src/services/LiveSpotProvider.cpp
    src/services/DXClusterProvider.cpp
    src/services/AuroraProvider.cpp
    src/services/HistoryProvider.cpp
    src/services/BandConditionsProvider.cpp
    src/services/ContestProvider.cpp
    src/services/MoonProvider.cpp
    src/services/SDOProvider.cpp
    src/services/WeatherProvider.cpp
    src/services/CallbookProvider.cpp
    src/services/DstProvider.cpp
    src/services/SantaProvider.cpp
    src/ui/ActivityPanels.cpp
    src/ui/ADIFPanel.cpp
    src/ui/AuroraPanel.cpp
    src/ui/AuroraGraphPanel.cpp
    src/ui/DXClusterPanel.cpp
    src/ui/DXClusterSetup.cpp
    src/ui/BandConditionsPanel.cpp
    src/ui/BeaconPanel.cpp
    src/ui/ContestPanel.cpp
    src/ui/CountdownPanel.cpp
    src/ui/DRAPPanel.cpp
    src/ui/GimbalPanel.cpp
    src/ui/HistoryPanel.cpp
    src/ui/MoonPanel.cpp
    src/ui/SDOPanel.cpp
    src/ui/WeatherPanel.cpp
    src/ui/ClockAuxPanel.cpp
    src/ui/DXPanel.cpp
    src/ui/DXSatPane.cpp
    src/ui/ListPanel.cpp
    src/ui/LiveSpotPanel.cpp
    src/ui/LocalPanel.cpp
    src/ui/MapWidget.cpp
    src/ui/PaneContainer.cpp
    src/ui/PlaceholderWidget.cpp
    src/ui/RenderUtils.cpp
    src/ui/RSSBanner.cpp
    src/ui/SatPanel.cpp
    src/ui/SetupScreen.cpp
    src/ui/SpaceWeatherPanel.cpp
    src/ui/TimePanel.cpp
    src/ui/WidgetSelector.cpp
    src/ui/CallbookPanel.cpp
    src/ui/DstPanel.cpp
    src/ui/WatchlistPanel.cpp
    src/ui/EMEToolPanel.cpp
    src/ui/SantaPanel.cpp
)

target_include_directories(hamclock-next PRIVATE
    ${CMAKE_SOURCE_DIR}/include
)

target_link_libraries(hamclock-next PRIVATE
    SDL2::SDL2-static
    SDL2_ttf::SDL2_ttf-static
    SDL2_image::SDL2_image-static
    libcurl
    sqlite3
    nlohmann_json::nlohmann_json
    predict_static
    Threads::Threads
    httplib::httplib
    spdlog::spdlog
)

if(WIN32)
    target_link_libraries(hamclock-next PRIVATE ws2_32)
else()
    # On Linux, link libstdc++ and libgcc statically for better portability
    target_link_options(hamclock-next PRIVATE -static-libstdc++ -static-libgcc)
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
