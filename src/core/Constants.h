#pragma once

#include <cstdint>

// Project-wide constants for HamClock-Next

namespace HamClock {

// Logical coordinate system dimensions (800x480 reference resolution)
static constexpr int LOGICAL_WIDTH = 800;
static constexpr int LOGICAL_HEIGHT = 480;

// Initial window dimensions
static constexpr int INITIAL_WIDTH = 800;
static constexpr int INITIAL_HEIGHT = 480;

// Default web server port
static constexpr int DEFAULT_WEB_SERVER_PORT = 8080;

// Frame rate control (~30 FPS to save CPU on embedded devices)
static constexpr int FRAME_DELAY_MS = 33;

// Layout mode (true = pixel-perfect 800x480 layout, false = responsive)
static constexpr bool FIDELITY_MODE = true;

// Default font size
static constexpr int DEFAULT_FONT_SIZE = 24;

// Custom SDL user event for blocking sleep
static constexpr uint32_t SDL_USER_EVENT_BLOCK_SLEEP = 0x8001;

// Base for custom application events, registered at runtime
extern uint32_t AE_BASE_EVENT;
// Specific app event offsets from the base
static constexpr uint32_t AE_SATELLITE_TRACK_READY = 1;
static constexpr uint32_t AE_RSS_DATA_READY = 2;

} // namespace HamClock
