#include "core/ActivityLocationManager.h"
#include "core/AuroraHistoryStore.h"
#include "core/BrightnessManager.h"
#include "core/CPUMonitor.h"
#include "core/CitiesManager.h"
#include "core/ConfigManager.h"
#include "core/DXClusterData.h"
#include "core/DatabaseManager.h"
#include "core/DisplayPower.h"
#include "core/HamClockState.h"
#include "core/LiveSpotData.h"
#include "core/LoTWActivityData.h"
#include "core/ClublogData.h"
#include "core/PrefixManager.h"
#include "core/RSSData.h"
#include "core/RigData.h"
#include "core/RotatorData.h"
#include "core/SatelliteManager.h"
#include "core/SolarData.h"
#include "core/SoundManager.h"
#include "ui/WidgetRegistry.h"
#include "core/WorkerService.h"

#include "network/FrameCapture.h"
#include "network/NetworkManager.h"
#include "network/WebServer.h"
#include "services/ADIFProvider.h"
#include "services/ActivityProvider.h"
#include "services/AlertsProvider.h"
#include "services/AsteroidProvider.h"
#include "services/AuroraProvider.h"
#include "services/BME280Provider.h"
#include "services/LTR329Provider.h"
#include "services/BandConditionsProvider.h"
#include "services/BeaconProvider.h"
#include "services/CallbookProvider.h"
#include "services/ContestProvider.h"
#include "services/DRAPProvider.h"
#include "services/DXClusterProvider.h"
#include "services/DstProvider.h"
#include "services/FccProvider.h"
#include "services/ForecastProvider.h"
#include "services/GPSProvider.h"
#include "services/HistoryProvider.h"
#include "services/HurricaneProvider.h"
#include "services/IonosondeProvider.h"
#include "services/LightningProvider.h"
#include "services/LiveSpotProvider.h"
#include "services/MarineProvider.h"
#include "services/MeteorProvider.h"
#include "services/MoonProvider.h"
#include "services/MufRtProvider.h"
#include "services/NOAAProvider.h"
#include "services/RBNProvider.h"
#include "services/QRZProvider.h"
#include "services/RSSProvider.h"
#include "services/ReachProvider.h"
#include "services/RepeaterProvider.h"
#include "services/RigService.h"
#include "services/RotatorService.h"
#include "services/SDOProvider.h"
#include "services/SantaProvider.h"
#include "services/SolarStormProvider.h"
#include "services/TropoProvider.h"
#include "services/UpdateChecker.h"
#include "services/WeatherProvider.h"
#include "services/WinlinkProvider.h"
#include "ui/ADIFPanel.h"
#include "ui/ActivityPanels.h"
#include "ui/AlertsPanel.h"
#include "ui/AsteroidPanel.h"
#include "ui/AuroraGraphPanel.h"
#include "ui/AuroraPanel.h"
#include "ui/BandConditionsPanel.h"
#include "ui/BeaconPanel.h"
#include "ui/CallbookPanel.h"
#include "ui/ClockAuxPanel.h"
#include "ui/ContestPanel.h"
#include "ui/CountdownPanel.h"
#include "ui/DRAPPanel.h"
#include "ui/DXClusterPanel.h"
#include "ui/DXInfo.h"
#include "ui/DebugOverlay.h"
#include "ui/DstPanel.h"
#include "ui/EMEToolPanel.h"
#include "ui/ENVPanel.h"
#include "ui/EmbeddedFont.h"
#ifndef __EMSCRIPTEN__
#include "ui/EmbeddedGlyphFont.h"
#endif
#include "ui/FontCatalog.h"
#include "ui/FontManager.h"
#include "ui/ForecastPanel.h"
#include "ui/GimbalPanel.h"
#include "ui/HistoryPanel.h"
#include "ui/HurricanePanel.h"
#include "ui/IonosondePanel.h"
#include "ui/LayoutManager.h"
#include "ui/LightningPanel.h"
#include "ui/LiveSpotPanel.h"
#include "ui/LocalPanel.h"
#include "ui/MapWidget.h"
#include "ui/MarinePanel.h"
#include "ui/MeteorPanel.h"
#include "ui/MoonPanel.h"
#include "ui/PaneContainer.h"
#include "ui/PlaceholderWidget.h"
#include "ui/RSSBanner.h"
#include "ui/ReminderPanel.h"
#include "ui/RepeaterPanel.h"
#include "ui/SDOPanel.h"
#include "ui/SantaPanel.h"
#include "ui/SetupScreen.h"
#include "ui/SolarStormPanel.h"
#include "ui/SpaceWeatherPanel.h"
#include "ui/StopwatchPanel.h"
#include "ui/SysInfoPanel.h"
#include "ui/TextureManager.h"
#include "ui/TimePanel.h"
#include "ui/TropoPanel.h"
#include "ui/UpdateOverlay.h"
#include "ui/WatchlistPanel.h"
#include "ui/WeatherPanel.h"
#include "ui/WidgetSelector.h"
#include "ui/WinlinkPanel.h"
#include "ui/icon_png.h"

#include "core/Constants.h"
#include "core/GreylineDXData.h"
#include "core/Logger.h"
#include "services/GreylineDXProvider.h"
#include "ui/GreylineDXPanel.h"
#include "ui/RigControlPanel.h"
#include "ui/SolarTimelinePanel.h"
#include "core/CalendarData.h"
#include "ui/CalendarPanel.h"
#include <SDL.h>
#include <SDL_image.h>
#include <SDL_syswm.h>
// SDL_syswm.h pulls in X11 headers on Linux which define None, Success, etc.
// Undefine them before any other headers that use those identifiers as names.
#ifdef None
#undef None
#endif
#ifdef Success
#undef Success
#endif
#include <SDL_ttf.h>
#ifndef __EMSCRIPTEN__
#include <curl/curl.h>
#endif
#include <fcntl.h>
#include <nlohmann/json.hpp>

#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#ifdef _WIN32
#include <winsock2.h>
#endif
#include <memory>
#ifdef __linux__
#include <unistd.h>
#endif

#ifdef _WIN32
#include <io.h>
#define access _access
#define F_OK 0
#endif
#include <vector>

using namespace HamClock;

#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#include <emscripten/html5.h>
#endif

#include "core/DashboardContext.h"


// Helper function to prevent RPi sleep
void preventRPiSleep(bool prevent, DisplayPower *dp = nullptr) {
#ifdef __linux__
#ifndef __EMSCRIPTEN__
  if (prevent) {
    if (dp) {
      dp->setPower(true);
    } else {
      (void)system("vcgencmd display_power 1 > /dev/null 2>&1");
    }

    // Disable console blanking via escape sequences (framebuffer fallback)
    int fd = open("/dev/tty1", O_WRONLY);
    if (fd >= 0) {
      const char *disableBlank = "\033[9;0]";
      const char *forceUnblank = "\033[14]";
      (void)write(fd, disableBlank, 6);
      (void)write(fd, forceUnblank, 4);
      close(fd);
    }
  }
#endif
#endif
}

// Global pointer for Emscripten
static AppContext *g_app = nullptr;

static std::string s_logLevel = "warn";
static void applyLogLevel(const std::string &level) {
  if (level == "trace" || level == "TRACE")       Log::setLevel(spdlog::level::trace);
  else if (level == "debug" || level == "DEBUG")  Log::setLevel(spdlog::level::debug);
  else if (level == "info" || level == "INFO")    Log::setLevel(spdlog::level::info);
  else if (level == "warn" || level == "WARN")    Log::setLevel(spdlog::level::warn);
  else if (level == "error" || level == "ERROR")  Log::setLevel(spdlog::level::err);
}

#ifdef __EMSCRIPTEN__
// Called from JavaScript (via Module._hamclock_after_idbfs) once IDBFS has
// synced from IndexedDB.  Only then is it safe to open files in the config
// directory, because before this point the directory contents are whatever
// SDL/Emscripten pre-populated in MEMFS (empty on a fresh session).
extern "C" EMSCRIPTEN_KEEPALIVE void hamclock_after_idbfs() {
  if (!g_app) {
    std::fprintf(stderr, "hamclock_after_idbfs: called before g_app init!\n");
    return;
  }
  AppContext &ctx = *g_app;

  // Initialize the database NOW — after IDBFS is populated — so we open any
  // existing DB that came from IndexedDB rather than creating a fresh one.
  if (!DatabaseManager::instance().init(ctx.cfgMgr.configDir() /
                                        "hamclock.db")) {
    LOG_E("Main", "Failed to initialize database");
  }

  LOG_I("Main", "IDBFS sync complete, configDir={}",
        ctx.cfgMgr.configDir().string());

  if (ctx.cfgMgr.load(ctx.appCfg)) {
    if (s_logLevel == "warn") applyLogLevel(ctx.appCfg.logLevel); // config overrides default only
    LOG_I("Main", "Config loaded: callsign={}", ctx.appCfg.callsign);
    ctx.state->deCallsign = ctx.appCfg.callsign;
    ctx.state->deGrid = ctx.appCfg.grid;
    ctx.state->deLocation = {ctx.appCfg.lat, ctx.appCfg.lon};
    ctx.netManager->setCorsProxyUrl(ctx.appCfg.corsProxyUrl);
    ctx.netManager->setHubConfig(ctx.appCfg.hubMode, ctx.appCfg.hubIp,
                                 ctx.appCfg.hubPort);
    ctx.displayPower->setMethodByName(ctx.appCfg.displayPowerMethod);
    SoundManager::getInstance().setMuted(ctx.appCfg.audioMuted);
    SoundManager::getInstance().setVolume(ctx.appCfg.audioVolume);
    ctx.activeSetup = AppContext::SetupMode::None;
  } else {
    LOG_I("Main", "No saved config found — showing setup screen");
    ctx.activeSetup = AppContext::SetupMode::Main;
  }
}
#endif

// Main tick function for Emscripten/MainLoop
void main_tick();

uint32_t HamClock::AE_BASE_EVENT = 0;

int main(int argc, char *argv[]) {
#ifndef _WIN32
  SDL_SetMainReady();
#endif
#ifndef __EMSCRIPTEN__
  curl_global_init(CURL_GLOBAL_ALL);
#endif

  // Initialize the worker service right away
  WorkerService::getInstance();

  g_app = new AppContext();
  AppContext &ctx = *g_app;

  ctx.cfgMgr.init();
#ifndef __EMSCRIPTEN__
  // Native: IDBFS does not exist; init log and DB immediately.
  Log::init(ctx.cfgMgr.configDir().string());
  auto s_startupT0 = std::chrono::steady_clock::now();
  auto logStartupPhase = [&](const char *phase) {
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                  std::chrono::steady_clock::now() - s_startupT0)
                  .count();
    LOG_I("Startup", "{}: +{}ms", phase, ms);
  };
  if (!DatabaseManager::instance().init(ctx.cfgMgr.configDir() /
                                        "hamclock.db")) {
    LOG_E("Main", "Failed to initialize database");
  }
  logStartupPhase("core init (log+db)");
#else
  // WASM: Log::init and DatabaseManager::init are called AFTER IDBFS sync
  // completes inside hamclock_after_idbfs().  If we init them here the log
  // and DB files are created in MEMFS before IndexedDB data is restored, so
  // the fresh empty files would shadow any previously persisted data.
  Log::init(ctx.cfgMgr.configDir().string()); // stderr only until IDBFS ready
#endif

  // Parse command-line
  bool forceFullscreen = false;
  bool forceSoftware = false;
  bool forceLiveWeb = false;

  for (int i = 1; i < argc; ++i) {
    std::string arg = argv[i];
    if (arg == "-f" || arg == "--fullscreen") {
      forceFullscreen = true;
    } else if (arg == "-s" || arg == "--software") {
      forceSoftware = true;
    } else if (arg == "--live-web") {
      forceLiveWeb = true;
    } else if (arg == "--show-cache-stats") {
      ctx.appCfg.showCacheStats = true;
    } else if (arg == "--no-audio") {
      SoundManager::getInstance().disable();
    } else if (arg == "--log-level" && i + 1 < argc) {
      s_logLevel = argv[++i];
    } else if (arg == "-h" || arg == "--help") {
      std::printf("Usage: hamclock-next [options]\n");
      return EXIT_SUCCESS;
    }
  }

  applyLogLevel(s_logLevel);

  // Headless detection: offscreen/dummy SDL driver or Docker environment
  bool headlessMode = false;
  {
    const char *vd = SDL_GetHint(SDL_HINT_VIDEODRIVER);
    if (!vd)
      vd = getenv("SDL_VIDEODRIVER");
    if (vd && (strcmp(vd, "offscreen") == 0 || strcmp(vd, "dummy") == 0))
      headlessMode = true;
  }
  if (!headlessMode) {
    if (std::filesystem::exists("/.dockerenv"))
      headlessMode = true;
  }
  bool liveWebEnabled = forceLiveWeb || headlessMode;

  // When headless, force the offscreen SDL video driver before SDL_Init
  if (headlessMode) {
    SDL_SetHint(SDL_HINT_VIDEODRIVER, "offscreen");
    LOG_I("Main", "Headless mode: using offscreen SDL driver");
  }

  ctx.displayPower = std::make_shared<DisplayPower>();

  LOG_INFO("Starting HamClock-Next {}...", HAMCLOCK_VERSION);

#ifdef __EMSCRIPTEN__
  // In WASM, IDBFS sync is async. Config is loaded later by
  // hamclock_after_idbfs() once IndexedDB data is available.
  ctx.activeSetup = AppContext::SetupMode::Loading;
#else
  if (ctx.cfgMgr.configPath().empty()) {
    std::fprintf(stderr, "Warning: could not resolve config path\n");
    ctx.activeSetup = AppContext::SetupMode::Main;
  } else if (!ctx.cfgMgr.load(ctx.appCfg)) {
    ctx.activeSetup = AppContext::SetupMode::Main;
  } else {
    if (s_logLevel == "warn") applyLogLevel(ctx.appCfg.logLevel); // config overrides default only
    ctx.displayPower->setMethodByName(ctx.appCfg.displayPowerMethod);
    SoundManager::getInstance().setMuted(ctx.appCfg.audioMuted);
    SoundManager::getInstance().setVolume(ctx.appCfg.audioVolume);
  }
#endif

  bool preventSleep = ctx.appCfg.preventSleep;

  // --- Init SDL2 ---
#ifdef _WIN32
  WSADATA wsaData;
  if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
    LOG_ERROR("WSAStartup failed");
    return EXIT_FAILURE;
  }
#endif

  SDL_SetHint(SDL_HINT_APP_NAME, "HamClock-Next");
  if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER | SDL_INIT_EVENTS) != 0) {
    LOG_ERROR("SDL_Init failed: {}", SDL_GetError());
    return EXIT_FAILURE;
  }

  AE_BASE_EVENT = SDL_RegisterEvents(2);
  if (AE_BASE_EVENT == (uint32_t)-1) {
    LOG_W("Main", "Failed to reserve user events for background tasks");
  }

  int imgFlags = IMG_INIT_PNG | IMG_INIT_JPG;
  if (!(IMG_Init(imgFlags) & imgFlags)) {
    LOG_ERROR("IMG_Init failed: {}", IMG_GetError());
  }
#ifndef __EMSCRIPTEN__
  logStartupPhase("SDL ready");
#endif

  if (preventSleep) {
    SDL_DisableScreenSaver();
    preventRPiSleep(true, ctx.displayPower.get());
  } else {
    SDL_EnableScreenSaver();
  }

  if (forceSoftware) {
    SDL_SetHint(SDL_HINT_RENDER_DRIVER, "software");
  } else {
    SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "best");
    SDL_GL_SetAttribute(SDL_GL_MULTISAMPLEBUFFERS, 0);
    SDL_GL_SetAttribute(SDL_GL_MULTISAMPLESAMPLES, 0);
#if (defined(__arm__) || defined(__aarch64__)) && !defined(__APPLE__)
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_ES);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 2);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 0);
#endif
    SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 0);
    SDL_GL_SetAttribute(SDL_GL_STENCIL_SIZE, 0);
    SDL_GL_SetAttribute(SDL_GL_RED_SIZE, 8);
    SDL_GL_SetAttribute(SDL_GL_GREEN_SIZE, 8);
    SDL_GL_SetAttribute(SDL_GL_BLUE_SIZE, 8);
    SDL_GL_SetAttribute(SDL_GL_ALPHA_SIZE, 0);
  }

#ifdef __EMSCRIPTEN__
  // Size the SDL window to the browser viewport at startup
  ctx.globalWinW = EM_ASM_INT({ return window.innerWidth; });
  ctx.globalWinH = EM_ASM_INT({ return window.innerHeight; });
#endif

  Uint32 windowFlags = SDL_WINDOW_SHOWN | SDL_WINDOW_RESIZABLE;
  if (!forceSoftware) {
    windowFlags |= SDL_WINDOW_OPENGL;
  }

  if (forceFullscreen) {
    windowFlags |= SDL_WINDOW_FULLSCREEN_DESKTOP;
  }

#ifdef __ANDROID__
  // Always fullscreen on Android
  windowFlags |= SDL_WINDOW_FULLSCREEN_DESKTOP;
#endif

  ctx.window = SDL_CreateWindow("HamClock-Next", SDL_WINDOWPOS_CENTERED,
                                SDL_WINDOWPOS_CENTERED, ctx.globalWinW,
                                ctx.globalWinH, windowFlags);

  if (!ctx.window) {
    // Fallback to software
    windowFlags &= ~SDL_WINDOW_OPENGL;
    SDL_SetHint(SDL_HINT_RENDER_DRIVER, "software");
    ctx.window = SDL_CreateWindow("HamClock-Next", SDL_WINDOWPOS_CENTERED,
                                  SDL_WINDOWPOS_CENTERED, ctx.globalWinW,
                                  ctx.globalWinH, windowFlags);
  }
#ifndef __EMSCRIPTEN__
  logStartupPhase("window created");
#endif

  if (!ctx.window) {
    LOG_ERROR("SDL_CreateWindow failed: {}", SDL_GetError());
    return EXIT_FAILURE;
  }

#if defined(__linux__) && !defined(__EMSCRIPTEN__) && defined(SDL_VIDEO_DRIVER_KMSDRM)
  {
    SDL_SysWMinfo wmInfo;
    SDL_VERSION(&wmInfo.version);
    if (SDL_GetWindowWMInfo(ctx.window, &wmInfo) &&
        wmInfo.subsystem == SDL_SYSWM_KMSDRM) {
      ctx.displayPower->setDrmFd(wmInfo.info.kmsdrm.drm_fd);
    }
  }
#endif

#ifdef __EMSCRIPTEN__
  // Resize the SDL window (and its backing canvas) whenever the browser
  // viewport changes size.  The existing SDL_WINDOWEVENT_SIZE_CHANGED handler
  // in main_tick() does all the layout recalculation automatically.
  emscripten_set_resize_callback(
      EMSCRIPTEN_EVENT_TARGET_WINDOW, ctx.window, false,
      [](int, const EmscriptenUiEvent *e, void *ud) -> EM_BOOL {
        int w = e->windowInnerWidth;
        int h = e->windowInnerHeight;
        SDL_SetWindowSize(static_cast<SDL_Window *>(ud), w, h);
        SDL_Event ev{};
        ev.type = SDL_WINDOWEVENT;
        ev.window.event = SDL_WINDOWEVENT_SIZE_CHANGED;
        ev.window.data1 = w;
        ev.window.data2 = h;
        SDL_PushEvent(&ev);
        return EM_TRUE;
      });
#endif

  // Icon
  {
    SDL_RWops *rw = SDL_RWFromMem((void *)icon_png, sizeof(icon_png));
    SDL_Surface *iconSurface = IMG_Load_RW(rw, 1);
    if (iconSurface) {
      SDL_SetWindowIcon(ctx.window, iconSurface);
      SDL_FreeSurface(iconSurface);
    }
  }

  Uint32 rendererFlags = 0;
#ifndef __EMSCRIPTEN__
  rendererFlags |= SDL_RENDERER_PRESENTVSYNC;
#endif
  if (!forceSoftware)
    rendererFlags |= SDL_RENDERER_ACCELERATED;
  else
    rendererFlags |= SDL_RENDERER_SOFTWARE;

  ctx.renderer = SDL_CreateRenderer(ctx.window, -1, rendererFlags);
  if (!ctx.renderer) {
    LOG_ERROR("SDL_CreateRenderer failed");
    return EXIT_FAILURE;
  }
#ifndef __EMSCRIPTEN__
  logStartupPhase("renderer created");
#endif

  if (TTF_Init() != 0) {
    LOG_ERROR("TTF_Init failed");
    return EXIT_FAILURE;
  }
#ifndef __EMSCRIPTEN__
  FontManager::setGlyphFont(glyphs_subset_ttf, glyphs_subset_ttf_len);
  logStartupPhase("fonts loaded");
#endif

  // --- Initialize Persistent State ---
  ctx.updateLayoutMetrics();

  ctx.netManager =
      std::make_unique<NetworkManager>(ctx.cfgMgr.configDir() / "cache");
  ctx.netManager->setCorsProxyUrl(ctx.appCfg.corsProxyUrl);
  ctx.netManager->setHubConfig(ctx.appCfg.hubMode, ctx.appCfg.hubIp,
                               ctx.appCfg.hubPort);
#ifndef __EMSCRIPTEN__
  logStartupPhase("network manager ready");
#endif

  ActivityLocationManager::getInstance().init(*ctx.netManager,
                                              ctx.cfgMgr.configDir() / "cache");

  ctx.prefixMgr.init();
  CitiesManager::getInstance().init();

  ctx.solarStore = std::make_shared<SolarDataStore>();
  ctx.auroraHistoryStore = std::make_shared<AuroraHistoryStore>();
  ctx.auroraHistoryStore->setStoragePath(ctx.cfgMgr.configDir() / "cache" /
                                         "aurora_history.json");
  ctx.auroraHistoryStore->load();
  ctx.watchlistStore = std::make_shared<WatchlistStore>();
  ctx.rssStore = std::make_shared<RSSDataStore>();
  ctx.watchlistHitStore = std::make_shared<WatchlistHitStore>();
  ctx.spotStore = std::make_shared<LiveSpotDataStore>();
  ctx.spotStore->setSelectedBandsMask(ctx.appCfg.liveSpotsBands);
  ctx.activityStore = std::make_shared<ActivityDataStore>();
  ctx.dxcStore = std::make_shared<DXClusterDataStore>();
  ctx.bandStore = std::make_shared<BandConditionsStore>();
  ctx.contestStore = std::make_shared<ContestStore>();
  ctx.moonStore = std::make_shared<MoonStore>();
  ctx.historyStore = std::make_shared<HistoryStore>();
  ctx.deWeatherStore = std::make_shared<WeatherStore>();
  ctx.dxWeatherStore = std::make_shared<WeatherStore>();
  ctx.callbookStore = std::make_shared<CallbookStore>();
  ctx.dstStore = std::make_shared<DstStore>();
  ctx.adifStore = std::make_shared<ADIFStore>();
  ctx.lotwActivityStore = std::make_shared<LoTWActivityStore>();
  ctx.clublogStore = std::make_shared<ClublogStore>();
  ctx.santaStore = std::make_shared<SantaStore>();
  ctx.rotatorStore = std::make_shared<RotatorDataStore>();
  ctx.rigStore = std::make_shared<RigDataStore>();
  ctx.alertsStore = std::make_shared<AlertsStore>();
  ctx.forecastStore = std::make_shared<ForecastStore>();
  ctx.repeaterStore = std::make_shared<RepeaterStore>();
  ctx.hurricaneStore = std::make_shared<HurricaneStore>();
  ctx.marineStore = std::make_shared<MarineStore>();
  ctx.winlinkStore = std::make_shared<WinlinkStore>();
  ctx.greylineDXStore = std::make_shared<GreylineDXStore>();
  ctx.auroraMapStore = std::make_shared<AuroraMapStore>();
  ctx.calendarStore = std::make_shared<CalendarStore>();
  ctx.calendarStore->setCachePath(ctx.cfgMgr.configDir() / "calendar_cache.json");
  ctx.calendarStore->loadCache();
  ctx.state = std::make_shared<HamClockState>();

  ctx.state->deCallsign = ctx.appCfg.callsign;
  ctx.state->deGrid = ctx.appCfg.grid;
  ctx.state->deLocation = {ctx.appCfg.lat, ctx.appCfg.lon};

  ctx.cpuMonitor = std::make_shared<CPUMonitor>();
  ctx.cpuMonitor->init();

  ctx.brightnessMgr = std::make_shared<BrightnessManager>();
  ctx.brightnessMgr->init();
  ctx.brightnessMgr->setBrightness(ctx.appCfg.brightness);
  ctx.brightnessMgr->setScheduleEnabled(ctx.appCfg.brightnessSchedule);
  ctx.brightnessMgr->setDimTime(ctx.appCfg.dimHour, ctx.appCfg.dimMinute);
  ctx.brightnessMgr->setBrightTime(ctx.appCfg.brightHour,
                                   ctx.appCfg.brightMinute);

  for (const auto &call : ctx.appCfg.watchlist)
    ctx.watchlistStore->add(call);

#ifndef __EMSCRIPTEN__
  ctx.frameCapture = std::make_unique<FrameCapture>();
  if (headlessMode)
    ctx.frameCapture->setMaxFps(30);
  ctx.frameCapture->start();
  ctx.updateChecker = std::make_unique<UpdateChecker>(*ctx.netManager);
  ctx.updateChecker->fetch();
  ctx.webServer = std::make_unique<WebServer>(
      ctx.renderer, ctx.appCfg, *ctx.state, ctx.cfgMgr, ctx.displayPower,
      ctx.configReloadRequested, ctx.watchlistStore, ctx.solarStore,
      ctx.contestStore, ctx.dxcStore, ctx.spotStore, ctx.cpuMonitor,
      DEFAULT_WEB_SERVER_PORT);
  ctx.webServer->setFrameCapture(ctx.frameCapture.get());
  ctx.webServer->setLiveWebEnabled(liveWebEnabled);
  ctx.webServer->setNetworkManager(ctx.netManager.get());
  ctx.webServer->setActivityStore(ctx.activityStore.get());
  ctx.webServer->setRotationControl(&ctx.rotationCmd, &ctx.rotationCmdPane,
                                    &ctx.rotationCmdWidget);
  ctx.webServer->setMapReloadFlag(&ctx.mapUpdateRequested);
  ctx.webServer->setMarineStore(ctx.marineStore);
  ctx.webServer->start();

  ctx.gpsProvider = std::make_unique<GPSProvider>(ctx.state.get(), ctx.appCfg);
  ctx.gpsProvider->start();

  ctx.bmeProvider = std::make_unique<BME280Provider>(ctx.deWeatherStore);
  ctx.bmeProvider->start();

  ctx.ltr329Provider = std::make_unique<LTR329Provider>();
  ctx.ltr329Provider->start();
  if (ctx.appCfg.ltr329AutoDim && ctx.brightnessMgr) {
    ctx.brightnessMgr->setLtr329Provider(ctx.ltr329Provider.get());
    ctx.brightnessMgr->setLtr329AutoDim(true);
  }
  logStartupPhase("managers ready (entering main loop)");
#endif

  // Audio device is opened lazily on first playAlarm() call.
  // Use --no-audio to permanently suppress audio (e.g. displays with buzzers).

  // --- Main Loop ---
#ifdef __EMSCRIPTEN__
  emscripten_set_main_loop(main_tick, 10, 1);
#else
  while (ctx.appRunning) {
    static constexpr Uint32 kTargetFrameMs = 20; // 50fps event loop; render throttled internally
    static Uint32 s_lastFrameMs = 0;
    main_tick();
    Uint32 elapsed = SDL_GetTicks() - s_lastFrameMs;
    if (!headlessMode && elapsed < kTargetFrameMs)
      SDL_Delay(kTargetFrameMs - elapsed);
    else if (headlessMode && elapsed < 10)
      SDL_Delay(10 - elapsed);
    s_lastFrameMs = SDL_GetTicks();
  }
#endif

  // Cleanup
  WorkerService::getInstance().stop();
  SoundManager::getInstance().cleanup();
  SDL_DestroyRenderer(ctx.renderer);
  SDL_DestroyWindow(ctx.window);
  SDL_Quit();
  return EXIT_SUCCESS;
}


void main_tick() {
  if (!g_app)
    return;
  AppContext &ctx = *g_app;

#ifdef __EMSCRIPTEN__
  // Waiting for IDBFS sync — render a blank frame and return.
  if (ctx.activeSetup == AppContext::SetupMode::Loading) {
    SDL_SetRenderDrawColor(ctx.renderer, 0, 0, 0, 255);
    SDL_RenderClear(ctx.renderer);
    SDL_RenderPresent(ctx.renderer);
    return;
  }
#endif

  if (ctx.activeSetup != AppContext::SetupMode::None) {
    // Destroy dashboard if needed — null WebServer's raw aliases first to
    // prevent heap-use-after-free in in-flight HTTP handlers (same guard as
    // the config-reload path below).
    if (ctx.dashboard) {
#ifndef __EMSCRIPTEN__
      if (ctx.webServer) {
        ctx.webServer->setPanes(nullptr);
        ctx.webServer->setTimePanel(nullptr);
        ctx.webServer->setRssBanner(nullptr);
        ctx.webServer->setSatelliteManager(nullptr);
        ctx.webServer->setRotatorService(nullptr);
        ctx.webServer->setStopwatch(nullptr);
        ctx.webServer->setADIFProvider(nullptr);
      }
#endif
      ctx.dashboard.reset();
    }

    ctx.updateLayoutMetrics();

    // Initial setup init
    if (!ctx.setupWidget) {
      auto setupFontMgr = std::make_unique<FontManager>();
      setupFontMgr->loadFromMemory(assets_font_ttf, assets_font_ttf_len,
                                   DEFAULT_FONT_SIZE);
#ifndef __EMSCRIPTEN__
      if (!ctx.appCfg.fontPath.empty()) {
        if (!setupFontMgr->loadFromFile(ctx.appCfg.fontPath, DEFAULT_FONT_SIZE))
          setupFontMgr->loadFromMemory(assets_font_ttf, assets_font_ttf_len,
                                       DEFAULT_FONT_SIZE);
      }
#endif
      if (FIDELITY_MODE)
        setupFontMgr->setRenderScale(ctx.layScale);

      ctx.setupCatalog = std::make_unique<FontCatalog>(*setupFontMgr);
      setupFontMgr->setCatalog(ctx.setupCatalog.get());
      ctx.setupCatalog->recalculate(LOGICAL_WIDTH, LOGICAL_HEIGHT);

      ctx.setupFontMgr = std::move(setupFontMgr);

      int setupW = LOGICAL_WIDTH;
      int setupH = LOGICAL_HEIGHT;
      int setupX = ctx.layLogicalOffX;
      int setupY = ctx.layLogicalOffY;

      if (ctx.activeSetup == AppContext::SetupMode::Main) {
        auto s = std::make_unique<SetupScreen>(setupX, setupY, setupW, setupH,
                                               *ctx.setupFontMgr,
                                               *ctx.brightnessMgr,
                                               ctx.displayPower);
        s->setConfig(ctx.appCfg);
        if (ctx.startOnUpdateTab) {
          s->setStartTab(SetupScreen::Tab::Update);
          ctx.startOnUpdateTab = false;
        } else if (ctx.startOnServicesTab) {
          s->setStartTab(SetupScreen::Tab::Services);
          ctx.startOnServicesTab = false;
        }
#ifndef __EMSCRIPTEN__
        if (ctx.webServer && ctx.webServer->isLiveWebEnabled()) {
          std::string url = "Live Web Control: http://" + NetworkManager::getLocalIP() + ":" + std::to_string(HamClock::DEFAULT_WEB_SERVER_PORT);
          s->setLiveWebUrl(url);
        }
        if (ctx.updateChecker)
          s->setUpdateChecker(ctx.updateChecker.get());
#endif
        ctx.setupWidget = std::move(s);
      }
#ifndef __ANDROID__
      SDL_StartTextInput();
#endif
    }

    // Logic
    bool setupDone = false;
    static float setupFingerScrollAccum = 0.0f;
    static bool setupFingerWasScrolling = false;
    SDL_Event event;
    while (SDL_PollEvent(&event)) {
      if (event.type == SDL_QUIT) {
        ctx.appRunning = false;
#ifdef __EMSCRIPTEN__
        emscripten_cancel_main_loop();
#endif
        return;
      }

      // Dispatch to Setup Widget
      if (ctx.setupWidget) {
        if (event.type == SDL_KEYDOWN)
          ctx.setupWidget->onKeyDown(event.key.keysym.sym,
                                     event.key.keysym.mod);
        else if (event.type == SDL_TEXTINPUT)
          ctx.setupWidget->onTextInput(event.text.text);
        else if (event.type == SDL_MOUSEBUTTONDOWN) {
          if (event.button.which == SDL_TOUCH_MOUSEID && setupFingerWasScrolling)
            ; // suppress: touch gesture was a scroll, not a tap
          else {
            int smx = event.button.x, smy = event.button.y;
            if (FIDELITY_MODE && event.button.windowID != 0) {
              float pixX = event.button.x * static_cast<float>(ctx.globalDrawW) /
                           ctx.globalWinW;
              float pixY = event.button.y * static_cast<float>(ctx.globalDrawH) /
                           ctx.globalWinH;
              smx = static_cast<int>(pixX / ctx.layScale);
              smy = static_cast<int>(pixY / ctx.layScale);
            }
            ctx.setupWidget->onMouseDown(smx, smy, SDL_GetModState(),
                                         event.button.clicks);
          }
        } else if (event.type == SDL_MOUSEBUTTONUP) {
          if (!(event.button.which == SDL_TOUCH_MOUSEID && setupFingerWasScrolling)) {
            int smx = event.button.x, smy = event.button.y;
            if (FIDELITY_MODE && event.button.windowID != 0) {
              float pixX = event.button.x * static_cast<float>(ctx.globalDrawW) /
                           ctx.globalWinW;
              float pixY = event.button.y * static_cast<float>(ctx.globalDrawH) /
                           ctx.globalWinH;
              smx = static_cast<int>(pixX / ctx.layScale);
              smy = static_cast<int>(pixY / ctx.layScale);
            }
            ctx.setupWidget->onMouseUp(smx, smy, SDL_GetModState(),
                                       event.button.clicks);
          }
        } else if (event.type == SDL_MOUSEMOTION) {
          int smx = event.motion.x, smy = event.motion.y;
          if (FIDELITY_MODE && event.motion.windowID != 0) {
            float pixX = event.motion.x * static_cast<float>(ctx.globalDrawW) /
                         ctx.globalWinW;
            float pixY = event.motion.y * static_cast<float>(ctx.globalDrawH) /
                         ctx.globalWinH;
            smx = static_cast<int>(pixX / ctx.layScale);
            smy = static_cast<int>(pixY / ctx.layScale);
          }
          ctx.setupWidget->onMouseMove(smx, smy);
        } else if (event.type == SDL_MOUSEWHEEL) {
          int scrollY = event.wheel.y;
#if SDL_VERSION_ATLEAST(2, 0, 18)
          if (event.wheel.direction == SDL_MOUSEWHEEL_FLIPPED)
            scrollY = -scrollY;
#endif
          ctx.setupWidget->onMouseWheel(scrollY);
        } else if (event.type == SDL_FINGERDOWN) {
          setupFingerScrollAccum = 0.0f;
          setupFingerWasScrolling = false;
        } else if (event.type == SDL_FINGERMOTION) {
          static constexpr float kScrollStep = 0.04f;
          setupFingerScrollAccum += event.tfinger.dy;
          while (setupFingerScrollAccum <= -kScrollStep) {
            setupFingerScrollAccum += kScrollStep;
            setupFingerWasScrolling = true;
            ctx.setupWidget->onMouseWheel(-1);
          }
          while (setupFingerScrollAccum >= kScrollStep) {
            setupFingerScrollAccum -= kScrollStep;
            setupFingerWasScrolling = true;
            ctx.setupWidget->onMouseWheel(1);
          }
        } else if (event.type == SDL_WINDOWEVENT &&
                   event.window.event == SDL_WINDOWEVENT_SIZE_CHANGED) {
          ctx.updateLayoutMetrics();
          if (ctx.setupFontMgr) {
            ctx.setupFontMgr->setRenderScale(ctx.layScale);
            ctx.setupFontMgr->clearCache();
          }
          if (ctx.setupCatalog) {
            ctx.setupCatalog->recalculate(LOGICAL_WIDTH, LOGICAL_HEIGHT);
          }
          ctx.setupWidget->onResize(ctx.layLogicalOffX, ctx.layLogicalOffY,
                                    LOGICAL_WIDTH, LOGICAL_HEIGHT);
        } else if (event.type >= AE_BASE_EVENT) {
          switch (event.type - AE_BASE_EVENT) {
            case AE_TOUCH: {
              int mx = static_cast<int>(reinterpret_cast<intptr_t>(event.user.data1));
              int my = static_cast<int>(reinterpret_cast<intptr_t>(event.user.data2));
              ctx.setupWidget->onMouseDown(mx, my, 0, 1);
              ctx.setupWidget->onMouseUp(mx, my, 0, 1);
              break;
            }
            case AE_WHEEL: {
              int dy = static_cast<int>(reinterpret_cast<intptr_t>(event.user.data1));
              ctx.setupWidget->onMouseWheel(dy);
              break;
            }
          }
        }
      }
    }

    ctx.setupWidget->update();

    // Render
    SDL_SetRenderDrawColor(ctx.renderer, 0, 0, 0, 255);
    SDL_RenderClear(ctx.renderer);
    if (FIDELITY_MODE) {
      SDL_RenderSetViewport(ctx.renderer, nullptr);
      SDL_RenderSetScale(ctx.renderer, ctx.layScale, ctx.layScale);
    }
    ctx.setupWidget->render(ctx.renderer);
#ifndef __EMSCRIPTEN__
    if (ctx.frameCapture)
      ctx.frameCapture->capture(ctx.renderer);
#endif
    SDL_RenderPresent(ctx.renderer);
    if (FIDELITY_MODE) {
      SDL_RenderSetScale(ctx.renderer, 1.0f, 1.0f);
    }

    // Check Done
    if (ctx.activeSetup == AppContext::SetupMode::Main) {
      if (static_cast<SetupScreen *>(ctx.setupWidget.get())->isComplete())
        setupDone = true;
    }

    if (setupDone) {
      SDL_StopTextInput();
      // Save logic
      if (ctx.activeSetup == AppContext::SetupMode::Main) {
        auto *s = static_cast<SetupScreen *>(ctx.setupWidget.get());
        if (!s->wasCancelled()) {
          // Merge: getConfig() starts from the current config and only
          // overwrites the fields SetupScreen manages, so all other fields
          // (asteroid prefs, live spots, alarms, etc.) survive untouched.
          ctx.appCfg = s->getConfig(ctx.appCfg);
          SoundManager::getInstance().setMuted(ctx.appCfg.audioMuted);
          SoundManager::getInstance().setVolume(ctx.appCfg.audioVolume);

          // Sync watchlist store from updated config
          auto oldW = ctx.watchlistStore->getAll();
          for (const auto &c : oldW)
            ctx.watchlistStore->remove(c);
          for (const auto &c : ctx.appCfg.watchlist)
            ctx.watchlistStore->add(c);
        }
      }
      ctx.cfgMgr.save(ctx.appCfg);
      if (ctx.dashboard && ctx.dashboard->spotProvider)
        ctx.dashboard->spotProvider->updateConfig(ctx.appCfg);
      ctx.setupWidget.reset();
      ctx.setupFontMgr.reset();
      ctx.setupCatalog.reset();
      ctx.activeSetup = AppContext::SetupMode::None;
      // Update state
      ctx.state->deCallsign = ctx.appCfg.callsign;
      ctx.state->deGrid = ctx.appCfg.grid;
      ctx.state->deLocation = {ctx.appCfg.lat, ctx.appCfg.lon};
      if (ctx.dashboard && ctx.dashboard->satMgr)
        ctx.dashboard->satMgr->setObserver(ctx.appCfg.lat, ctx.appCfg.lon);

      // Re-apply theme, font, rotations and layout immediately
      if (ctx.dashboard) {
        // Re-apply custom font if configured
        if (!ctx.appCfg.fontPath.empty()) {
          if (!ctx.dashboard->fontMgr.loadFromFile(ctx.appCfg.fontPath,
                                                   DEFAULT_FONT_SIZE))
            ctx.dashboard->fontMgr.loadFromMemory(assets_font_ttf,
                                                  assets_font_ttf_len,
                                                  DEFAULT_FONT_SIZE);
        } else {
          ctx.dashboard->fontMgr.loadFromMemory(assets_font_ttf,
                                                assets_font_ttf_len,
                                                DEFAULT_FONT_SIZE);
        }
        ctx.dashboard->fontCatalog.recalculate(LOGICAL_WIDTH, LOGICAL_HEIGHT);

        // Propagate theme, metric, and font changes to all dashboard widgets
        for (auto *w : ctx.dashboard->widgets) {
          if (w) {
            w->setTheme(ctx.appCfg.theme);
            w->setMetric(ctx.appCfg.useMetric);
            w->onFontChanged();
          }
        }
        // Re-apply rotation interval and widget lists to top-bar panes (0-3).
        // applySidePanelMode handles panes 4-5; panes 0-3 must be updated here
        // so that changes to rotationIntervalS take effect without restart.
        auto &panes = ctx.dashboard->panes;
        const auto &cfg = ctx.appCfg;
        if (panes.size() >= 4) {
          panes[0]->setRotation(cfg.pane1Rotation, cfg.rotationIntervalS, cfg.syncRotation);
          panes[1]->setRotation(cfg.pane2Rotation, cfg.rotationIntervalS, cfg.syncRotation);
          panes[2]->setRotation(cfg.pane3Rotation, cfg.rotationIntervalS, cfg.syncRotation);
          panes[3]->setRotation(cfg.pane4Rotation, cfg.rotationIntervalS, cfg.syncRotation);
        }
        ctx.dashboard->applySidePanelMode(ctx.appCfg.pane5Rotation.empty()
                                              ? "de_info"
                                              : ctx.appCfg.pane5Rotation[0],
                                          ctx);
      }
    }

  } else {
    // Dashboard
    if (!ctx.dashboard) {
      // Ensure the background loadCache() thread has finished before providers
      // start calling fetchAsync() — otherwise cache entries are not yet in the
      // memory map and every provider falls through to a live network fetch.
      // SDL init typically consumes more time than loadCache() needs, so this
      // returns immediately in the common case.
#ifndef __EMSCRIPTEN__
      ctx.netManager->waitForCacheLoad();
#endif
      ctx.dashboard = std::make_unique<DashboardContext>(ctx);
#ifndef __EMSCRIPTEN__
      // Apply custom font if configured (embedded font used by default)
      if (!ctx.appCfg.fontPath.empty()) {
        if (!ctx.dashboard->fontMgr.loadFromFile(ctx.appCfg.fontPath,
                                                 DEFAULT_FONT_SIZE))
          ctx.dashboard->fontMgr.loadFromMemory(assets_font_ttf,
                                                assets_font_ttf_len,
                                                DEFAULT_FONT_SIZE);
        ctx.dashboard->fontCatalog.recalculate(LOGICAL_WIDTH, LOGICAL_HEIGHT);
        for (auto *w : ctx.dashboard->widgets) {
          if (w)
            w->onFontChanged();
        }
      }
      if (ctx.webServer) {
        ctx.webServer->setSatelliteManager(ctx.dashboard->satMgr.get());
        ctx.webServer->setRotatorService(ctx.dashboard->rotatorService.get());
        ctx.webServer->setPanes(&ctx.dashboard->panes);
        ctx.webServer->setTimePanel(ctx.dashboard->timePanel.get());
        ctx.webServer->setRssBanner(ctx.dashboard->rssBanner.get());
        ctx.webServer->setPaneExpandControl(&ctx.paneExpandCmd);
        ctx.webServer->setWeatherStore(ctx.deWeatherStore);
        ctx.webServer->setBMEProvider(ctx.bmeProvider.get());
        ctx.webServer->setBrightnessManager(ctx.brightnessMgr);
        ctx.webServer->setStopwatch(static_cast<StopwatchPanel *>(
            ctx.dashboard->widgetFactory_("stopwatch")));
        ctx.webServer->setCalendarStore(ctx.calendarStore);
      }
      if (!ctx.startupAnnounceDone) {
        ctx.startupAnnounceDone = true;
        SoundManager::getInstance().speak("HamClock system online");
      }
#endif
    }

    // Hold locationMutex for the entire update+render window so the WebServer
    // thread cannot tear location/fps string fields mid-frame.  The lock is
    // released at the end of this else-block (or at any early return) via RAII.
    // Typical hold time: ~1–20 ms per tick; WebServer gets the lock in the
    // SDL_Delay gap between ticks.
    std::lock_guard<std::mutex> locLk(ctx.state->locationMutex);

    // Apply any config changes injected by the WebServer API (RPi/framebuffer
    // remote-control scenario).  The WebServer thread writes to ctx.appCfg
    // under the config mutex and then sets this flag; we re-apply live state
    // here on the main thread so no SDL calls happen off-thread.
    if (ctx.configReloadRequested.exchange(false, std::memory_order_acq_rel)) {
      ctx.state->deCallsign = ctx.appCfg.callsign;
      ctx.state->deGrid = ctx.appCfg.grid;
      ctx.state->deLocation = {ctx.appCfg.lat, ctx.appCfg.lon};
      ctx.netManager->setCorsProxyUrl(ctx.appCfg.corsProxyUrl);
      ctx.netManager->setHubConfig(ctx.appCfg.hubMode, ctx.appCfg.hubIp,
                                   ctx.appCfg.hubPort);
      ctx.displayPower->setMethodByName(ctx.appCfg.displayPowerMethod);
      SoundManager::getInstance().setMuted(ctx.appCfg.audioMuted);

      // Reload WatchlistStore from the updated config
      ctx.watchlistStore->clear();
      for (const auto &call : ctx.appCfg.watchlist) {
        ctx.watchlistStore->add(call);
      }

      LOG_I("Main", "Config reloaded from remote API: callsign={}",
            ctx.appCfg.callsign);

      // Rebuild dashboard to refresh all widgets with new config (host, port,
      // overlays, theme, metric, etc.)
      if (ctx.dashboard) {
        // Null out WebServer's dashboard-owned raw pointers before teardown to
        // prevent heap-use-after-free: WebServer threads can be mid-handler
        // reading panes_/timePanel_/etc. while the main thread destroys them.
#ifndef __EMSCRIPTEN__
        if (ctx.webServer) {
          ctx.webServer->setPanes(nullptr);
          ctx.webServer->setTimePanel(nullptr);
          ctx.webServer->setRssBanner(nullptr);
          ctx.webServer->setSatelliteManager(nullptr);
          ctx.webServer->setRotatorService(nullptr);
          ctx.webServer->setStopwatch(nullptr);
          ctx.webServer->setADIFProvider(nullptr);
        }
#endif
        ctx.dashboard.reset();
        LOG_I("Main", "Dashboard rebuild triggered by remote config reload");
        return; // Exit tick early; dashboard will be re-created on next frame
      }
    }

    // Surgical map updates (no full reset)
    if (ctx.mapUpdateRequested.exchange(false, std::memory_order_acq_rel)) {
      if (ctx.dashboard && ctx.dashboard->mapArea) {
        // MapWidget detects config changes in its own update() call, 
        // we just need to ensure we don't trigger a full reload.
        LOG_I("Main", "Lightweight map update triggered");
      }
    }

    // Process rotation control commands from REST API (WebServer thread).
    int rcmd = ctx.rotationCmd.exchange(0, std::memory_order_acq_rel);
    if (rcmd != 0 && ctx.dashboard) {
      int rpane = ctx.rotationCmdPane.load(std::memory_order_relaxed);
      int rwidget = ctx.rotationCmdWidget.load(std::memory_order_relaxed);
      auto applyPane = [&](PaneContainer &p) {
        if (rcmd == 1)
          p.setPaused(true);
        else if (rcmd == 2)
          p.setPaused(false);
        else if (rcmd == 3)
          p.forceAdvance();
        else if (rcmd == 4 && rwidget >= 0)
        { auto all = WidgetRegistry::instance().getAll(true); if (rwidget < (int)all.size()) p.jumpToType(all[rwidget]->typeId); }
      };
      if (rpane >= 0 && rpane < (int)ctx.dashboard->panes.size()) {
        applyPane(*ctx.dashboard->panes[rpane]);
      } else {
        for (auto &p : ctx.dashboard->panes)
          applyPane(*p);
      }
    }

    // Process pane expand/collapse commands from REST API.
    int ecmd = ctx.paneExpandCmd.exchange(-1, std::memory_order_acq_rel);
    if (ecmd >= 0 && ctx.dashboard)
      ctx.dashboard->expandPane(ecmd, ctx);
    else if (ecmd == -2 && ctx.dashboard)
      ctx.dashboard->restorePane(ctx);

    // Process set_pane commands queued from the REST API network thread.
    // These must run here (main/render thread) because setRotation() calls
    // onResize() which may call SDL_DestroyTexture (e.g. DXClusterPanel).
    // Drain the entire queue each frame — rapid-fire API calls (e.g. resetting
    // all 6 panes at once) must all land, not collapse to the last one.
#ifndef __EMSCRIPTEN__
    if (ctx.webServer && ctx.dashboard) {
      auto cmds = ctx.webServer->drainPendingPaneSets();
      const int intS  = ctx.appCfg.rotationIntervalS;
      const bool sync = ctx.appCfg.syncRotation;
      for (const auto &psc : cmds) {
        if (psc.pane < 0 || psc.pane >= (int)ctx.dashboard->panes.size())
          continue;
        auto &p = ctx.dashboard->panes[psc.pane];
        if (psc.action == 1) {
          p->forceAdvance();
        } else if (psc.action == 2 && !psc.widget.empty()) {
          auto rot = p->getRotation();
          if (std::find(rot.begin(), rot.end(), psc.widget) == rot.end()) {
            rot.push_back(psc.widget);
            p->setRotation(rot, intS, sync);
          }
        } else if (psc.action == 3 && !psc.widget.empty()) {
          auto rot = p->getRotation();
          rot.erase(std::remove(rot.begin(), rot.end(), psc.widget), rot.end());
          if (!rot.empty())
            p->setRotation(rot, intS, sync);
        } else if (psc.action == 4 && !psc.widget.empty()) {
          p->setRotation({psc.widget}, intS, sync);
        }
      }
    }
#endif // __EMSCRIPTEN__

    // Always call update() — this processes SDL events and keeps interaction
    // responsive. Only render() is throttled.
    ctx.dashboard->update(ctx);

#ifndef __EMSCRIPTEN__
    // Render at 2fps at idle; boost to 20fps for 2s after any mouse/touch input.
    static Uint32 s_lastRenderMs = 0;
    static constexpr Uint32 kIdleIntervalMs = 500;  // 2fps
    static constexpr Uint32 kActiveIntervalMs = 50; // 20fps during interaction
    static constexpr Uint32 kMouseWindowMs = 2000;  // stay fast 2s after input
    const Uint32 renderNow = SDL_GetTicks();
    const bool mouseActive =
        (renderNow - ctx.dashboard->lastMouseMotionMs < kMouseWindowMs);
    const Uint32 renderInterval =
        mouseActive ? kActiveIntervalMs : kIdleIntervalMs;
    if (renderNow - s_lastRenderMs >= renderInterval) {
      s_lastRenderMs = renderNow;
      ctx.dashboard->render(ctx);
    }
#else
    ctx.dashboard->render(ctx);
#endif

    if (!ctx.appRunning) {
#ifdef __EMSCRIPTEN__
      emscripten_cancel_main_loop();
#endif
    }
  }
}

#ifdef _WIN32
#include <windows.h>
int WINAPI WinMain(HINSTANCE hInst, HINSTANCE hPrev, LPSTR szCmdLine, int sw) {
  return main(__argc, __argv);
}
#endif
