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
// NTP sync request
static constexpr uint32_t SDL_USER_EVENT_NTP_SYNC = 0x8002;
// Stopwatch control events
static constexpr uint32_t SDL_USER_EVENT_STOPWATCH_RUN = 0x8010;
static constexpr uint32_t SDL_USER_EVENT_STOPWATCH_STOP = 0x8011;
static constexpr uint32_t SDL_USER_EVENT_STOPWATCH_RESET = 0x8012;
static constexpr uint32_t SDL_USER_EVENT_STOPWATCH_COUNTDOWN = 0x8013;

// Base for custom application events, registered at runtime
extern uint32_t AE_BASE_EVENT;
// Specific app event offsets from the base
static constexpr uint32_t AE_SATELLITE_TRACK_READY = 1;
static constexpr uint32_t AE_RSS_DATA_READY = 2;
static constexpr uint32_t AE_SOLAR_DATA_READY = 3;
static constexpr uint32_t AE_AURORA_DATA_READY = 4;
static constexpr uint32_t AE_ACTIVITY_DATA_READY = 5;
static constexpr uint32_t AE_WEATHER_DATA_READY = 6;
static constexpr uint32_t AE_CONTEST_DATA_READY = 7;
static constexpr uint32_t AE_HISTORY_DATA_READY = 8;
static constexpr uint32_t AE_PROP_DATA_READY = 9;
static constexpr uint32_t AE_ASTEROID_ELEMENTS_READY = 10;
static constexpr uint32_t AE_ALERTS_DATA_READY = 11;
static constexpr uint32_t AE_FORECAST_DATA_READY = 12;
static constexpr uint32_t AE_REPEATER_DATA_READY = 13;
static constexpr uint32_t AE_HURRICANE_DATA_READY = 14;
static constexpr uint32_t AE_MARINE_DATA_READY = 15;
static constexpr uint32_t AE_WINLINK_DATA_READY = 16;
static constexpr uint32_t AE_MAP_IMAGE_READY = 17;
static constexpr uint32_t AE_MOON_IMAGE_READY = 18;
static constexpr uint32_t AE_TOUCH = 19; // REST /set_touch: data1=x, data2=y (logical)
static constexpr uint32_t AE_WHEEL = 20; // REST /set_wheel: data1=deltaY (positive=up, negative=down)
static constexpr uint32_t AE_SATELLITE_DATA_READY = 21;
static constexpr uint32_t AE_UPDATE_DATA_READY = 22;
static constexpr uint32_t AE_SPACEWX_ALERT_READY = 23;
static constexpr uint32_t AE_MARINE_LOOKUP_READY = 24;
static constexpr uint32_t AE_DX_ALERT = 25;


} // namespace HamClock
