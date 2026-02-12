cmake_minimum_required(VERSION 3.18)

# libpredict v2.0.0 uses cmake_minimum_required(VERSION 2.8) which modern CMake rejects
set(CMAKE_POLICY_VERSION_MINIMUM 3.5 CACHE STRING "Compat for old FetchContent deps")

project(HamClock-Next
    VERSION 0.1.0
    LANGUAGES C CXX
)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# --- System Libraries ---

set(THREADS_PREFER_PTHREAD_FLAG ON)
find_package(Threads REQUIRED)
find_package(CURL REQUIRED)

# On some platforms (like RPi 32-bit), we need to explicitly link atomic for C++20
if(CMAKE_SYSTEM_PROCESSOR MATCHES "arm" OR CMAKE_SYSTEM_PROCESSOR MATCHES "aarch64")
    link_libraries(atomic)
endif()

# SDL2 Core
find_package(SDL2 REQUIRED)

# SDL2 Extensions (often don't have CMake config files on Linux/Pi)
find_package(PkgConfig REQUIRED)

pkg_check_modules(SDL2_TTF REQUIRED IMPORTED_TARGET SDL2_ttf)
pkg_check_modules(SDL2_IMAGE REQUIRED IMPORTED_TARGET SDL2_image)

# --- Header-only / FetchContent Libraries ---

include(FetchContent)

FetchContent_Declare(
    nlohmann_json
    GIT_REPOSITORY https://github.com/nlohmann/json.git
    GIT_TAG        v3.11.3
    GIT_SHALLOW    TRUE
)

FetchContent_Declare(
    libpredict
    GIT_REPOSITORY https://github.com/la1k/libpredict.git
    GIT_TAG        v2.0.0
    GIT_SHALLOW    TRUE
)

FetchContent_Declare(
    httplib
    GIT_REPOSITORY https://github.com/yhirose/cpp-httplib.git
    GIT_TAG        v0.14.3
    GIT_SHALLOW    TRUE
)

# Populate libpredict manually so we can skip its tests subdirectory.
# Its tests use deprecated C++ APIs (std::ptr_fun, std::not1) that fail with -Werror on GCC 15.
FetchContent_GetProperties(libpredict)
if(NOT libpredict_POPULATED)
    FetchContent_Populate(libpredict)
    # Replicate the essential setup from libpredict's root CMakeLists.txt
    include(GNUInstallDirs)
    set(LIBPREDICT_INCLUDE_DIR ${libpredict_BINARY_DIR}/include)
    include_directories(AFTER ${LIBPREDICT_INCLUDE_DIR})
    # Generate the public header (predict.h from predict.h.in)
    add_subdirectory(${libpredict_SOURCE_DIR}/include ${libpredict_BINARY_DIR}/include)
    # Build only the library, not tests/travis/docs
    add_subdirectory(${libpredict_SOURCE_DIR}/src ${libpredict_BINARY_DIR}/src)
endif()

FetchContent_MakeAvailable(nlohmann_json httplib)

# --- Application Target ---

add_executable(hamclock-next
    src/main.cpp
    src/core/ConfigManager.cpp
    src/core/OrbitPredictor.cpp
    src/core/SatelliteManager.cpp
    src/core/PrefixManager.cpp
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
    src/ui/ActivityPanels.cpp
    src/ui/ADIFPanel.cpp
    src/ui/AuroraPanel.cpp
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
)

target_include_directories(hamclock-next PRIVATE
    ${CMAKE_SOURCE_DIR}/include
    ${SDL2_TTF_INCLUDE_DIRS}
    ${SDL2_IMAGE_INCLUDE_DIRS}
)

target_link_libraries(hamclock-next PRIVATE
    SDL2::SDL2
    PkgConfig::SDL2_TTF
    PkgConfig::SDL2_IMAGE
    CURL::libcurl
    nlohmann_json::nlohmann_json
    predict
    Threads::Threads
    httplib::httplib
)

# --- No assets to copy (now embedded) ---
