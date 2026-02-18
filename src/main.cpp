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
#include "core/PrefixManager.h"
#include "core/RSSData.h"
#include "core/RigData.h"
#include "core/RotatorData.h"
#include "core/SatelliteManager.h"
#include "core/SolarData.h"
#ifdef ENABLE_DEBUG_API
#include "core/UIRegistry.h"
#endif
#include "core/MemoryMonitor.h"
#include "core/SoundManager.h"
#include "core/WidgetType.h"

#include "network/NetworkManager.h"
#include "network/WebServer.h"
#include "services/ADIFProvider.h"
#include "services/ActivityProvider.h"
#include "services/AuroraProvider.h"
#include "services/BandConditionsProvider.h"
#include "services/CallbookProvider.h"
#include "services/ContestProvider.h"
#include "services/DRAPProvider.h"
#include "services/DXClusterProvider.h"
#include "services/DstProvider.h"
#include "services/GPSProvider.h"
#include "services/HistoryProvider.h"
#include "services/IonosondeProvider.h"
#include "services/LiveSpotProvider.h"
#include "services/MoonProvider.h"
#include "services/MufRtProvider.h"
#include "services/NOAAProvider.h"
#include "services/RBNProvider.h"
#include "services/RSSProvider.h"
#include "services/RigService.h"
#include "services/RotatorService.h"
#include "services/SDOProvider.h"
#include "services/SantaProvider.h"
#include "services/WeatherProvider.h"
#include "ui/ADIFPanel.h"
#include "ui/ActivityPanels.h"
#include "ui/AuroraGraphPanel.h"
#include "ui/AuroraPanel.h"
#include "ui/BandConditionsPanel.h"
#include "ui/BeaconPanel.h"
#include "ui/CPUTempPanel.h"
#include "ui/CallbookPanel.h"
#include "ui/ClockAuxPanel.h"
#include "ui/ContestPanel.h"
#include "ui/CountdownPanel.h"
#include "ui/DRAPPanel.h"
#include "ui/DXClusterPanel.h"
#include "ui/DXClusterSetup.h"
#include "ui/DXSatPane.h"
#include "ui/DebugOverlay.h"
#include "ui/DstPanel.h"
#include "ui/EMEToolPanel.h"
#include "ui/EmbeddedFont.h"
#include "ui/FontCatalog.h"
#include "ui/FontManager.h"
#include "ui/GimbalPanel.h"
#include "ui/HistoryPanel.h"
#include "ui/LayoutManager.h"
#include "ui/LiveSpotPanel.h"
#include "ui/LocalPanel.h"
#include "ui/MapWidget.h"
#include "ui/MoonPanel.h"
#include "ui/PaneContainer.h"
#include "ui/PlaceholderWidget.h"
#include "ui/RSSBanner.h"
#include "ui/SDOPanel.h"
#include "ui/SantaPanel.h"
#include "ui/SetupScreen.h"
#include "ui/SpaceWeatherPanel.h"
#include "ui/TextureManager.h"
#include "ui/TimePanel.h"
#include "ui/WatchlistPanel.h"
#include "ui/WeatherPanel.h"
#include "ui/WidgetSelector.h"
#include "ui/icon_png.h"

#include "core/Constants.h"
#include "core/Logger.h"
#include <SDL.h>
#include <SDL_image.h>
#include <SDL_ttf.h>
#ifndef __EMSCRIPTEN__
#include <curl/curl.h>
#endif
#include <fcntl.h>
#include <nlohmann/json.hpp>

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#ifdef _WIN32
#include <winsock2.h>
#endif
#include <memory>
#include <sstream>
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

// Forward declarations for Contexts
struct DashboardContext;

// Global application context (persistent state)
struct AppContext {
  // Core & Configuration
  AppConfig appCfg;
  ConfigManager cfgMgr;
  std::shared_ptr<HamClockState> state;
  bool appRunning = true;

  // SDL Subsystem
  SDL_Window *window = nullptr;
  SDL_Renderer *renderer = nullptr;
  int globalWinW = INITIAL_WIDTH;
  int globalWinH = INITIAL_HEIGHT;
  int globalDrawW = INITIAL_WIDTH;
  int globalDrawH = INITIAL_HEIGHT;

  // Layout Metrics
  float layScale = 1.0f;
  int layLogicalOffX = 0;
  int layLogicalOffY = 0;
  enum class AlignMode { Center, Left, Right };
  AlignMode alignMode = AlignMode::Center;

  // Data Stores
  std::shared_ptr<SolarDataStore> solarStore;
  std::shared_ptr<WatchlistStore> watchlistStore;
  std::shared_ptr<RSSDataStore> rssStore;
  std::shared_ptr<WatchlistHitStore> watchlistHitStore;
  std::shared_ptr<LiveSpotDataStore> spotStore;
  std::shared_ptr<ActivityDataStore> activityStore;
  std::shared_ptr<DXClusterDataStore> dxcStore;
  std::shared_ptr<BandConditionsStore> bandStore;
  std::shared_ptr<ContestStore> contestStore;
  std::shared_ptr<MoonStore> moonStore;
  std::shared_ptr<HistoryStore> historyStore;
  std::shared_ptr<WeatherStore> deWeatherStore;
  std::shared_ptr<WeatherStore> dxWeatherStore;
  std::shared_ptr<CallbookStore> callbookStore;
  std::shared_ptr<DstStore> dstStore;
  std::shared_ptr<ADIFStore> adifStore;
  std::shared_ptr<SantaStore> santaStore;
  std::shared_ptr<RotatorDataStore> rotatorStore;
  std::shared_ptr<RigDataStore> rigStore;

  // Managers & Services
  std::unique_ptr<NetworkManager> netManager;
  PrefixManager prefixMgr;
  std::shared_ptr<DisplayPower> displayPower;
  std::shared_ptr<BrightnessManager> brightnessMgr;
  std::shared_ptr<CPUMonitor> cpuMonitor;

#ifndef __EMSCRIPTEN__
  std::unique_ptr<WebServer> webServer;
  std::unique_ptr<GPSProvider> gpsProvider;
#endif

  // Setup State
  enum class SetupMode { None, Loading, Main, DXCluster };
  SetupMode activeSetup = SetupMode::None;
  std::unique_ptr<Widget> setupWidget;
  std::unique_ptr<FontManager> setupFontMgr;

  // Remote-config reload signal.  WebServer thread sets this to true after a
  // successful POST /api/reload or /set_config; main_tick() reads and clears
  // it, then re-applies the in-memory config to live state (callsign, proxy,
  // themes, etc.) without tearing down the dashboard.
  std::atomic<bool> configReloadRequested{false};

  // Dashboard State (Transient)
  std::unique_ptr<DashboardContext> dashboard;

  void updateLayoutMetrics();
};

// Dashboard context (re-created on exit from setup)
struct DashboardContext {
  // Resources
  FontManager fontMgr;
  TextureManager texMgr;
  FontCatalog fontCatalog;
  DebugOverlay debugOverlay;

  // Providers
  std::unique_ptr<NOAAProvider> noaaProvider;
  std::unique_ptr<RSSProvider> rssProvider;
  std::unique_ptr<LiveSpotProvider> spotProvider;
  std::unique_ptr<ActivityProvider> activityProvider;
  std::unique_ptr<DXClusterProvider> dxcProvider;
  std::unique_ptr<RBNProvider> rbnProvider;
  std::unique_ptr<BandConditionsProvider> bandProvider;
  std::unique_ptr<ContestProvider> contestProvider;
  std::unique_ptr<MoonProvider> moonProvider;
  std::unique_ptr<HistoryProvider> historyProvider;
  std::unique_ptr<WeatherProvider> deWeatherProvider;
  std::unique_ptr<WeatherProvider> dxWeatherProvider;
  std::unique_ptr<SDOProvider> sdoProvider;
  std::unique_ptr<DRAPProvider> drapProvider;
  std::shared_ptr<AuroraProvider> auroraProvider;
  std::shared_ptr<CallbookProvider> callbookProvider;
  std::unique_ptr<DstProvider> dstProvider;
  std::unique_ptr<ADIFProvider> adifProvider;
  std::unique_ptr<MufRtProvider> mufRtProvider;
  std::unique_ptr<IonosondeProvider> ionosondeProvider;
  std::unique_ptr<SantaProvider> santaProvider;
  std::unique_ptr<SatelliteManager> satMgr;

  // Services
#ifndef __EMSCRIPTEN__
  std::unique_ptr<RotatorService> rotatorService;
  std::unique_ptr<RigService> rigService;
#endif

  // UI Components
  std::unique_ptr<TimePanel> timePanel;
  std::unique_ptr<WidgetSelector> widgetSelector;
  std::vector<std::unique_ptr<PaneContainer>> panes;
  std::unique_ptr<LocalPanel> localPanel;
  std::unique_ptr<DXSatPane> dxSatPane;
  std::unique_ptr<MapWidget> mapArea;
  std::unique_ptr<RSSBanner> rssBanner;
  LayoutManager layout;

  // Collections
  std::map<WidgetType, std::unique_ptr<Widget>> widgetPool;
  std::vector<Widget *> widgets;
  std::vector<Widget *> eventWidgets;

  // State
  Uint32 lastFetchMs = 0;
  Uint32 lastResizeMs = 0;
  Uint32 lastFpsUpdate = 0;
  int frames = 0;
  Uint32 lastMouseMotionMs = 0;
  bool cursorVisible = true;
  Uint32 lastSleepAssert = 0;

  DashboardContext(AppContext &ctx);
  ~DashboardContext() = default;

  void update(AppContext &ctx);
  void render(AppContext &ctx);
};

// Helper function to prevent RPi sleep
static void preventRPiSleep(bool prevent, DisplayPower *dp = nullptr) {
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
    LOG_I("Main", "Config loaded: callsign={}", ctx.appCfg.callsign);
    ctx.state->deCallsign = ctx.appCfg.callsign;
    ctx.state->deGrid = ctx.appCfg.grid;
    ctx.state->deLocation = {ctx.appCfg.lat, ctx.appCfg.lon};
    ctx.netManager->setCorsProxyUrl(ctx.appCfg.corsProxyUrl);
    ctx.activeSetup = AppContext::SetupMode::None;
  } else {
    LOG_I("Main", "No saved config found — showing setup screen");
    ctx.activeSetup = AppContext::SetupMode::Main;
  }
}
#endif

// Main tick function for Emscripten/MainLoop
void main_tick();

int main(int argc, char *argv[]) {
#ifndef _WIN32
  SDL_SetMainReady();
#endif
#ifndef __EMSCRIPTEN__
  curl_global_init(CURL_GLOBAL_ALL);
#endif

  g_app = new AppContext();
  AppContext &ctx = *g_app;

  ctx.cfgMgr.init();
#ifndef __EMSCRIPTEN__
  // Native: IDBFS does not exist; init log and DB immediately.
  Log::init(ctx.cfgMgr.configDir().string());
  if (!DatabaseManager::instance().init(ctx.cfgMgr.configDir() /
                                        "hamclock.db")) {
    LOG_E("Main", "Failed to initialize database");
  }
#else
  // WASM: Log::init and DatabaseManager::init are called AFTER IDBFS sync
  // completes inside hamclock_after_idbfs().  If we init them here the log
  // and DB files are created in MEMFS before IndexedDB data is restored, so
  // the fresh empty files would shadow any previously persisted data.
  Log::init(ctx.cfgMgr.configDir().string()); // stderr only until IDBFS ready
#endif

  ctx.displayPower = std::make_shared<DisplayPower>();
  ctx.displayPower->init();

  // Parse command-line
  bool forceFullscreen = false;
  bool forceSoftware = false;
  std::string logLevel = "warn";

  for (int i = 1; i < argc; ++i) {
    std::string arg = argv[i];
    if (arg == "-f" || arg == "--fullscreen") {
      forceFullscreen = true;
    } else if (arg == "-s" || arg == "--software") {
      forceSoftware = true;
    } else if (arg == "--log-level" && i + 1 < argc) {
      logLevel = argv[++i];
    } else if (arg == "-h" || arg == "--help") {
      std::printf("Usage: hamclock-next [options]\n");
      return EXIT_SUCCESS;
    }
  }

  // Set log level
  if (logLevel == "debug" || logLevel == "DEBUG") {
    Log::setLevel(spdlog::level::debug);
  } else if (logLevel == "info" || logLevel == "INFO") {
    Log::setLevel(spdlog::level::info);
  } else if (logLevel == "warn" || logLevel == "WARN") {
    Log::setLevel(spdlog::level::warn);
  } else if (logLevel == "error" || logLevel == "ERROR") {
    Log::setLevel(spdlog::level::err);
  } else {
    Log::setLevel(spdlog::level::warn);
  }

  LOG_INFO("Starting HamClock-Next v{}...", HAMCLOCK_VERSION);

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
  }
#endif

  bool preventSleep = ctx.appCfg.preventSleep;

  // --- Init SDL2 ---
  int numDrivers = SDL_GetNumVideoDrivers();
  std::fprintf(stderr, "SDL Video Drivers available: ");
  for (int i = 0; i < numDrivers; ++i) {
    std::fprintf(stderr, "%s%s", SDL_GetVideoDriver(i),
                 (i == numDrivers - 1) ? "" : ", ");
  }
  std::fprintf(stderr, "\n");

#ifdef _WIN32
  WSADATA wsaData;
  if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
    LOG_ERROR("WSAStartup failed");
    return EXIT_FAILURE;
  }
#endif

  if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER) != 0) {
    LOG_ERROR("SDL_Init failed: {}", SDL_GetError());
    return EXIT_FAILURE;
  }

  int imgFlags = IMG_INIT_PNG | IMG_INIT_JPG;
  if (!(IMG_Init(imgFlags) & imgFlags)) {
    LOG_ERROR("IMG_Init failed: {}", IMG_GetError());
  }

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

  if (!ctx.window) {
    LOG_ERROR("SDL_CreateWindow failed: {}", SDL_GetError());
    return EXIT_FAILURE;
  }

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

  if (TTF_Init() != 0) {
    LOG_ERROR("TTF_Init failed");
    return EXIT_FAILURE;
  }

  // --- Initialize Persistent State ---
  ctx.updateLayoutMetrics();

  ctx.netManager =
      std::make_unique<NetworkManager>(ctx.cfgMgr.configDir() / "cache");
  ctx.netManager->setCorsProxyUrl(ctx.appCfg.corsProxyUrl);
  ctx.prefixMgr.init();
  CitiesManager::getInstance().init();

  ctx.solarStore = std::make_shared<SolarDataStore>();
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
  ctx.santaStore = std::make_shared<SantaStore>();
  ctx.rotatorStore = std::make_shared<RotatorDataStore>();
  ctx.rigStore = std::make_shared<RigDataStore>();
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

  if (ctx.watchlistStore->getAll().empty()) {
    ctx.watchlistStore->add("K1ABC");
    ctx.watchlistStore->add("W1AW");
  }

#ifndef __EMSCRIPTEN__
  ctx.webServer = std::make_unique<WebServer>(
      ctx.renderer, ctx.appCfg, *ctx.state, ctx.cfgMgr, ctx.displayPower,
      ctx.configReloadRequested, ctx.watchlistStore, ctx.solarStore,
      DEFAULT_WEB_SERVER_PORT);
  ctx.webServer->start();

  ctx.gpsProvider = std::make_unique<GPSProvider>(ctx.state.get(), ctx.appCfg);
  ctx.gpsProvider->start();
#endif

  // Init Sound
  SoundManager::getInstance().init();

  // --- Main Loop ---
#ifdef __EMSCRIPTEN__
  emscripten_set_main_loop(main_tick, 0, 1);
#else
  while (ctx.appRunning) {
    main_tick();
  }
#endif

  // Cleanup
  SoundManager::getInstance().cleanup();
  SDL_DestroyRenderer(ctx.renderer);
  SDL_DestroyWindow(ctx.window);
  SDL_Quit();
  return EXIT_SUCCESS;
}

// =========================================================================================
// Implementation
// =========================================================================================

void AppContext::updateLayoutMetrics() {
  SDL_GetWindowSize(window, &globalWinW, &globalWinH);
  SDL_GetRendererOutputSize(renderer, &globalDrawW, &globalDrawH);

  if (FIDELITY_MODE) {
    float sw = static_cast<float>(globalDrawW) / LOGICAL_WIDTH;
    float sh = static_cast<float>(globalDrawH) / LOGICAL_HEIGHT;
    layScale = std::min(sw, sh);
    int logicalW = static_cast<int>(globalDrawW / layScale);
    int logicalH = static_cast<int>(globalDrawH / layScale);
    int xSpace = logicalW - LOGICAL_WIDTH;
    int ySpace = logicalH - LOGICAL_HEIGHT;

    switch (alignMode) {
    case AlignMode::Center:
      layLogicalOffX = xSpace / 2;
      layLogicalOffY = ySpace / 2;
      break;
    case AlignMode::Left:
      layLogicalOffX = 0;
      layLogicalOffY = 0;
      break;
    case AlignMode::Right:
      layLogicalOffX = xSpace;
      layLogicalOffY = ySpace / 2;
      break;
    }
  } else {
    layScale = 1.0f;
    layLogicalOffX = 0;
    layLogicalOffY = 0;
  }
}

DashboardContext::DashboardContext(AppContext &ctx)
    : fontMgr(), texMgr(), fontCatalog(fontMgr), debugOverlay(fontMgr),
      satMgr(std::make_unique<SatelliteManager>(*ctx.netManager)) {
  // Reset idle timer to now so the cursor-hide logic doesn't fire immediately
  lastMouseMotionMs = SDL_GetTicks();
  // Load font
  if (!fontMgr.loadFromMemory(assets_font_ttf, assets_font_ttf_len,
                              DEFAULT_FONT_SIZE)) {
    std::fprintf(stderr, "Warning: text rendering disabled\n");
  }
  fontMgr.setCatalog(&fontCatalog);

  // Compute render scale
  int drawW, drawH;
  SDL_GetRendererOutputSize(ctx.renderer, &drawW, &drawH);
  float rs = static_cast<float>(drawH) / LOGICAL_HEIGHT;
  fontMgr.setRenderScale(rs);

  // Initializers
  auto solarStore = ctx.solarStore;
  auto watchlistStore = ctx.watchlistStore;
  auto rssStore = ctx.rssStore;
  auto watchlistHitStore = ctx.watchlistHitStore;
  auto spotStore = ctx.spotStore;
  auto activityStore = ctx.activityStore;
  auto dxcStore = ctx.dxcStore;
  auto bandStore = ctx.bandStore;
  auto contestStore = ctx.contestStore;
  auto moonStore = ctx.moonStore;
  auto historyStore = ctx.historyStore;
  auto deWeatherStore = ctx.deWeatherStore;
  auto dxWeatherStore = ctx.dxWeatherStore;
  auto callbookStore = ctx.callbookStore;
  auto dstStore = ctx.dstStore;
  auto adifStore = ctx.adifStore;
  auto santaStore = ctx.santaStore;
  auto rotatorStore = ctx.rotatorStore;
  auto rigStore = ctx.rigStore;
  auto state = ctx.state;
  auto &netManager = *ctx.netManager;
  auto &appCfg = ctx.appCfg;

  auto auroraHistoryStore = std::make_shared<AuroraHistoryStore>();
  noaaProvider = std::make_unique<NOAAProvider>(
      netManager, solarStore, auroraHistoryStore, state.get());
  noaaProvider->fetch();

  rssProvider = std::make_unique<RSSProvider>(netManager, rssStore);
  rssProvider->fetch();

  spotProvider = std::make_unique<LiveSpotProvider>(netManager, spotStore,
                                                    appCfg, state.get(),
                                                    dxcStore);
  spotProvider->fetch();

#ifndef __EMSCRIPTEN__
  rotatorService =
      std::make_unique<RotatorService>(rotatorStore, appCfg, state.get());
  rotatorService->start();
  rigService = std::make_unique<RigService>(rigStore, appCfg, state.get());
  rigService->start();
#endif

  satMgr->fetch();
#ifndef __EMSCRIPTEN__
  satMgr->setRotatorService(rotatorService.get());
#endif
  satMgr->setObserver(appCfg.lat, appCfg.lon);

  activityProvider =
      std::make_unique<ActivityProvider>(netManager, activityStore);
  activityProvider->fetch();

  dxcProvider = std::make_unique<DXClusterProvider>(
      dxcStore, ctx.prefixMgr, watchlistStore, watchlistHitStore, state.get());
#ifndef __EMSCRIPTEN__
  dxcProvider->start(appCfg);
#endif

  rbnProvider =
      std::make_unique<RBNProvider>(dxcStore, ctx.prefixMgr, state.get());
#ifndef __EMSCRIPTEN__
  rbnProvider->start(appCfg);
#endif

  bandProvider =
      std::make_unique<BandConditionsProvider>(solarStore, bandStore);
  bandProvider->update();

  contestProvider = std::make_unique<ContestProvider>(netManager, contestStore);
  contestProvider->fetch();

  moonProvider = std::make_unique<MoonProvider>(netManager, moonStore);
  moonProvider->update(appCfg.lat, appCfg.lon);

  historyProvider = std::make_unique<HistoryProvider>(netManager, historyStore);
  historyProvider->fetchFlux();
  historyProvider->fetchSSN();
  historyProvider->fetchKp();

  deWeatherProvider =
      std::make_unique<WeatherProvider>(netManager, deWeatherStore);
  deWeatherProvider->fetch(state->deLocation.lat, state->deLocation.lon);

  dxWeatherProvider =
      std::make_unique<WeatherProvider>(netManager, dxWeatherStore);
  dxWeatherProvider->fetch(state->dxLocation.lat, state->dxLocation.lon);

  sdoProvider = std::make_unique<SDOProvider>(netManager);
  drapProvider = std::make_unique<DRAPProvider>(netManager);
  auroraProvider = std::make_shared<AuroraProvider>(netManager);

  callbookProvider =
      std::make_shared<CallbookProvider>(netManager, callbookStore);
  callbookProvider->lookup("K1ABC");

  dstProvider = std::make_unique<DstProvider>(netManager, dstStore);
  dstProvider->fetch();

  adifProvider = std::make_unique<ADIFProvider>(adifStore, ctx.prefixMgr);
  adifProvider->fetch(ctx.cfgMgr.configDir() / "logs.adif");

  mufRtProvider = std::make_unique<MufRtProvider>(netManager);
  mufRtProvider->update();

  ionosondeProvider = std::make_unique<IonosondeProvider>(netManager);
  ionosondeProvider->update();

  santaProvider = std::make_unique<SantaProvider>(santaStore);
  santaProvider->update();

  SDL_Color cyan = {0, 200, 255, 255};
  timePanel =
      std::make_unique<TimePanel>(0, 0, 0, 0, fontMgr, texMgr, appCfg.callsign);
  timePanel->setCallColor(appCfg.callsignColor);
  timePanel->setOnConfigChanged(
      [&ctx, this](const std::string &call, SDL_Color color) {
        ctx.appCfg.callsign = call;
        ctx.appCfg.callsignColor = color;
        ctx.cfgMgr.save(ctx.appCfg);
      });

  widgetSelector = std::make_unique<WidgetSelector>(fontMgr);

  // Helper for pool
  auto addToPool = [&](WidgetType type) {
    switch (type) {
    case WidgetType::SOLAR:
      widgetPool[type] =
          std::make_unique<SpaceWeatherPanel>(0, 0, 0, 0, fontMgr, solarStore);
      break;
    case WidgetType::DX_CLUSTER:
#ifndef __EMSCRIPTEN__
      widgetPool[type] = std::make_unique<DXClusterPanel>(
          0, 0, 0, 0, fontMgr, dxcStore, rigService.get(), &appCfg);
#else
      widgetPool[type] = std::make_unique<DXClusterPanel>(
          0, 0, 0, 0, fontMgr, dxcStore, nullptr, &appCfg);
#endif
      break;
    case WidgetType::LIVE_SPOTS:
      widgetPool[type] = std::make_unique<LiveSpotPanel>(
          0, 0, 0, 0, fontMgr, *spotProvider, spotStore, appCfg, ctx.cfgMgr);
      break;
    case WidgetType::BAND_CONDITIONS:
      widgetPool[type] =
          std::make_unique<BandConditionsPanel>(0, 0, 0, 0, fontMgr, bandStore);
      break;
    case WidgetType::CONTESTS:
      widgetPool[type] =
          std::make_unique<ContestPanel>(0, 0, 0, 0, fontMgr, contestStore);
      break;
    case WidgetType::CALLBOOK:
      widgetPool[type] =
          std::make_unique<CallbookPanel>(0, 0, 0, 0, fontMgr, callbookStore);
      break;
    case WidgetType::DST_INDEX:
      widgetPool[type] =
          std::make_unique<DstPanel>(0, 0, 0, 0, fontMgr, dstStore);
      break;
    case WidgetType::WATCHLIST:
      widgetPool[type] = std::make_unique<WatchlistPanel>(
          0, 0, 0, 0, fontMgr, watchlistStore, watchlistHitStore);
      break;
    case WidgetType::EME_TOOL:
      widgetPool[type] =
          std::make_unique<EMEToolPanel>(0, 0, 0, 0, fontMgr, moonStore);
      break;
    case WidgetType::SANTA_TRACKER:
      widgetPool[type] =
          std::make_unique<SantaPanel>(0, 0, 0, 0, fontMgr, santaStore);
      break;
    case WidgetType::ON_THE_AIR: {
      auto ontaPanel = std::make_unique<ONTAPanel>(
          0, 0, 0, 0, fontMgr, *activityProvider, activityStore);
      ontaPanel->setFilter(appCfg.ontaFilter);
      ontaPanel->setOnFilterChanged([&ctx](const std::string &f) {
        ctx.appCfg.ontaFilter = f;
        ctx.cfgMgr.save(ctx.appCfg);
      });
      widgetPool[type] = std::move(ontaPanel);
      break;
    }
    case WidgetType::DX_PEDITIONS:
      widgetPool[type] = std::make_unique<DXPedPanel>(
          0, 0, 0, 0, fontMgr, *activityProvider, activityStore);
      break;
    case WidgetType::GIMBAL:
      widgetPool[type] =
          std::make_unique<GimbalPanel>(0, 0, 0, 0, fontMgr, rotatorStore);
      break;
    case WidgetType::MOON:
      widgetPool[type] = std::make_unique<MoonPanel>(
          0, 0, 0, 0, fontMgr, texMgr, netManager, moonStore);
      break;
    case WidgetType::CLOCK_AUX:
      widgetPool[type] = std::make_unique<ClockAuxPanel>(0, 0, 0, 0, fontMgr);
      break;
    case WidgetType::HISTORY_FLUX:
      widgetPool[type] = std::make_unique<HistoryPanel>(
          0, 0, 0, 0, fontMgr, texMgr, historyStore, "flux");
      break;
    case WidgetType::HISTORY_SSN:
      widgetPool[type] = std::make_unique<HistoryPanel>(
          0, 0, 0, 0, fontMgr, texMgr, historyStore, "ssn");
      break;
    case WidgetType::HISTORY_KP:
      widgetPool[type] = std::make_unique<HistoryPanel>(
          0, 0, 0, 0, fontMgr, texMgr, historyStore, "kp");
      break;
    case WidgetType::DRAP:
      widgetPool[type] = std::make_unique<DRAPPanel>(0, 0, 0, 0, fontMgr,
                                                     texMgr, *drapProvider);
      break;
    case WidgetType::AURORA:
      widgetPool[type] = std::make_unique<AuroraPanel>(0, 0, 0, 0, fontMgr,
                                                       texMgr, *auroraProvider);
      break;
    case WidgetType::AURORA_GRAPH:
      widgetPool[type] = std::make_unique<AuroraGraphPanel>(0, 0, 0, 0, fontMgr,
                                                            auroraHistoryStore);
      break;
    case WidgetType::ADIF:
      widgetPool[type] =
          std::make_unique<ADIFPanel>(0, 0, 0, 0, fontMgr, adifStore);
      break;
    case WidgetType::COUNTDOWN:
      widgetPool[type] =
          std::make_unique<CountdownPanel>(0, 0, 0, 0, fontMgr, appCfg);
      break;
    case WidgetType::DE_WEATHER:
      widgetPool[type] = std::make_unique<WeatherPanel>(
          0, 0, 0, 0, fontMgr, deWeatherStore, "DE Weather");
      break;
    case WidgetType::DX_WEATHER:
      widgetPool[type] = std::make_unique<WeatherPanel>(
          0, 0, 0, 0, fontMgr, dxWeatherStore, "DX Weather");
      break;
    case WidgetType::NCDXF:
      widgetPool[type] = std::make_unique<BeaconPanel>(0, 0, 0, 0, fontMgr);
      break;
    case WidgetType::SDO:
      widgetPool[type] =
          std::make_unique<SDOPanel>(0, 0, 0, 0, fontMgr, texMgr, *sdoProvider);
      break;
    case WidgetType::CPU_TEMP:
      widgetPool[type] = std::make_unique<CPUTempPanel>(
          0, 0, 0, 0, fontMgr, ctx.cpuMonitor, appCfg.useMetric);
      break;
    default:
      widgetPool[type] = std::make_unique<PlaceholderWidget>(
          0, 0, 0, 0, fontMgr, widgetTypeDisplayName(type), cyan);
      break;
    }
  };

  std::vector<WidgetType> allTypes = {
      WidgetType::SOLAR,         WidgetType::DX_CLUSTER,
      WidgetType::LIVE_SPOTS,    WidgetType::BAND_CONDITIONS,
      WidgetType::CONTESTS,      WidgetType::ON_THE_AIR,
      WidgetType::GIMBAL,        WidgetType::MOON,
      WidgetType::CLOCK_AUX,     WidgetType::DX_PEDITIONS,
      WidgetType::DE_WEATHER,    WidgetType::DX_WEATHER,
      WidgetType::NCDXF,         WidgetType::SDO,
      WidgetType::HISTORY_FLUX,  WidgetType::HISTORY_KP,
      WidgetType::HISTORY_SSN,   WidgetType::DRAP,
      WidgetType::AURORA,        WidgetType::AURORA_GRAPH,
      WidgetType::ADIF,          WidgetType::COUNTDOWN,
      WidgetType::CALLBOOK,      WidgetType::DST_INDEX,
      WidgetType::WATCHLIST,     WidgetType::EME_TOOL,
      WidgetType::SANTA_TRACKER, WidgetType::CPU_TEMP};
  for (auto t : allTypes)
    addToPool(t);

  for (int i = 0; i < 4; ++i) {
    panes.push_back(std::make_unique<PaneContainer>(
        0, 0, 0, 0, WidgetType::SOLAR, fontMgr));
    panes.back()->setWidgetFactory(
        [&](WidgetType t) { return widgetPool[t].get(); });
  }

  panes[0]->setRotation(appCfg.pane1Rotation, appCfg.rotationIntervalS);
  panes[1]->setRotation(appCfg.pane2Rotation, appCfg.rotationIntervalS);
  panes[2]->setRotation(appCfg.pane3Rotation, appCfg.rotationIntervalS);
  panes[3]->setRotation(appCfg.pane4Rotation, appCfg.rotationIntervalS);

  auto onPaneSelectionRequested = [&, allTypes](int paneIdx, int mx, int my) {
    (void)mx;
    (void)my;
    std::vector<WidgetType> available = allTypes;
    if (paneIdx == 3) {
      available = {WidgetType::NCDXF, WidgetType::SOLAR, WidgetType::DX_WEATHER,
                   WidgetType::DE_WEATHER};
    }
    std::vector<WidgetType> current = panes[paneIdx]->getRotation();
    std::vector<WidgetType> forbidden;
    for (int i = 0; i < 4; ++i) {
      if (i == paneIdx)
        continue;
      auto rot = panes[i]->getRotation();
      forbidden.insert(forbidden.end(), rot.begin(), rot.end());
    }
    widgetSelector->show(
        paneIdx, available, current, forbidden,
        [&ctx, this](int idx, const std::vector<WidgetType> &finalSelection) {
          panes[idx]->setRotation(finalSelection, ctx.appCfg.rotationIntervalS);
          ctx.appCfg.pane1Rotation = panes[0]->getRotation();
          ctx.appCfg.pane2Rotation = panes[1]->getRotation();
          ctx.appCfg.pane3Rotation = panes[2]->getRotation();
          ctx.appCfg.pane4Rotation = panes[3]->getRotation();
          ctx.cfgMgr.save(ctx.appCfg);
        });
  };
  for (int i = 0; i < 4; ++i) {
    panes[i]->setOnSelectionRequested(onPaneSelectionRequested, i);
  }

  localPanel =
      std::make_unique<LocalPanel>(0, 0, 0, 0, fontMgr, state, deWeatherStore);
  dxSatPane = std::make_unique<DXSatPane>(0, 0, 0, 0, fontMgr, texMgr, state,
                                          *satMgr, dxWeatherStore);
  dxSatPane->setObserver(appCfg.lat, appCfg.lon);
  dxSatPane->restoreState(appCfg.panelMode, appCfg.selectedSatellite);
  dxSatPane->setMapTrackVisible(appCfg.showSatTrack);
  dxSatPane->setOnModeChanged(
      [&ctx](const std::string &mode, const std::string &satName) {
        ctx.appCfg.panelMode = mode;
        ctx.appCfg.selectedSatellite = satName;
        ctx.cfgMgr.save(ctx.appCfg);
      });
  dxSatPane->setOnMapTrackToggle([&ctx](bool enabled) {
    ctx.appCfg.showSatTrack = enabled;
    ctx.cfgMgr.save(ctx.appCfg);
  });

  mapArea = std::make_unique<MapWidget>(0, 0, 0, 0, texMgr, fontMgr, netManager,
                                        state, appCfg);
  mapArea->setOnConfigChanged([&ctx] { ctx.cfgMgr.save(ctx.appCfg); });
  mapArea->setSpotStore(spotStore);
  mapArea->setDXClusterStore(dxcStore);
  mapArea->setADIFStore(adifStore);
  mapArea->setMufRtProvider(mufRtProvider.get());
  mapArea->setAuroraStore(auroraHistoryStore);
  mapArea->setIonosondeProvider(ionosondeProvider.get());
  mapArea->setSolarDataStore(ctx.solarStore.get());
  // NOAAProvider seems to populate solar data?
  // Let's check main.cpp earlier.

  rssBanner = std::make_unique<RSSBanner>(139, 412, 660, 68, fontMgr, rssStore);
  rssBanner->setEnabled(appCfg.rssEnabled);
  if (!appCfg.rssEnabled)
    rssProvider->setEnabled(false);

  // Now that rssBanner and rssProvider exist, extend the MapWidget config
  // callback to propagate rssEnabled changes to both.
  mapArea->setOnConfigChanged([&ctx, &dash = *this]() {
    ctx.cfgMgr.save(ctx.appCfg);
    if (dash.rssBanner)
      dash.rssBanner->setEnabled(ctx.appCfg.rssEnabled);
    if (dash.rssProvider)
      dash.rssProvider->setEnabled(ctx.appCfg.rssEnabled);
  });

  // Layout
  if (FIDELITY_MODE)
    layout.setFidelityMode(true);
  layout.addWidget(Zone::TopBar, timePanel.get(), 2.0f);
  layout.addWidget(Zone::TopBar, panes[0].get(), 1.5f);
  layout.addWidget(Zone::TopBar, panes[1].get(), 1.5f);
  layout.addWidget(Zone::TopBar, panes[2].get(), 1.5f);
  layout.addWidget(Zone::TopBar, panes[3].get(), 0.6f);
  layout.addWidget(Zone::SidePanel, localPanel.get());
  layout.addWidget(Zone::SidePanel, dxSatPane.get());
  layout.addWidget(Zone::MainStage, mapArea.get());

  // Apply Theme

  for (auto const &[type, widget] : widgetPool) {
    if (widget) {
      widget->setTheme(appCfg.theme);
      widget->setMetric(appCfg.useMetric);
    }
  }
  timePanel->setTheme(appCfg.theme);
  timePanel->setMetric(appCfg.useMetric);
  localPanel->setTheme(appCfg.theme);
  localPanel->setMetric(appCfg.useMetric);
  dxSatPane->setTheme(appCfg.theme);
  dxSatPane->setMetric(appCfg.useMetric);
  mapArea->setTheme(appCfg.theme);
  mapArea->setMetric(appCfg.useMetric);
  rssBanner->setTheme(appCfg.theme);
  rssBanner->setMetric(appCfg.useMetric);
  widgetSelector->setTheme(appCfg.theme);
  widgetSelector->setMetric(appCfg.useMetric);
  for (auto &p : panes) {
    p->setTheme(appCfg.theme);
    p->setMetric(appCfg.useMetric);
  }

  texMgr.setLowMemCallback([this]() {
    LOG_W("Main", "Low memory signal: flushing FontManager cache");
    fontMgr.clearCache();
  });

  // Populate widgets/eventWidgets vector
  widgets = {timePanel.get(),     panes[0].get(), panes[1].get(),
             panes[2].get(),      panes[3].get(), localPanel.get(),
             dxSatPane.get(),     mapArea.get(),  rssBanner.get(),
             widgetSelector.get()};

  eventWidgets = {widgetSelector.get(), timePanel.get(), panes[0].get(),
                  panes[1].get(),       panes[2].get(),  panes[3].get(),
                  localPanel.get(),     dxSatPane.get(), mapArea.get(),
                  rssBanner.get()};

  lastFetchMs = SDL_GetTicks();
  lastFpsUpdate = SDL_GetTicks();
  frames = 0;

  // Initial layout calculation
  fontCatalog.recalculate(LOGICAL_WIDTH, LOGICAL_HEIGHT);
  layout.recalculate(LOGICAL_WIDTH, LOGICAL_HEIGHT, ctx.layLogicalOffX,
                     ctx.layLogicalOffY);
  rssBanner->onResize(139 + ctx.layLogicalOffX, 412 + ctx.layLogicalOffY, 660,
                      68);
}

void DashboardContext::update(AppContext &ctx) {
  auto &appCfg = ctx.appCfg;

  ctx.updateLayoutMetrics();

  Uint32 now = SDL_GetTicks();

  // Background refresh every 15 minutes
  if (now - lastFetchMs > 15 * 60 * 1000) {
    noaaProvider->fetch();
    rssProvider->fetch();
    spotProvider->fetch();
    satMgr->fetch();
    activityProvider->fetch();
    bandProvider->update();
    contestProvider->fetch();
    moonProvider->update(appCfg.lat, appCfg.lon);
    deWeatherProvider->fetch(ctx.state->deLocation.lat,
                             ctx.state->deLocation.lon);
    dxWeatherProvider->fetch(ctx.state->dxLocation.lat,
                             ctx.state->dxLocation.lon);
    historyProvider->fetchFlux();
    historyProvider->fetchSSN();
    historyProvider->fetchKp();
    adifProvider->fetch(ctx.cfgMgr.configDir() / "logs.adif");
    mufRtProvider->update();
    ionosondeProvider->update();
    lastFetchMs = now;
  }

  SDL_Event event;
  while (SDL_PollEvent(&event)) {
    if (event.type == SDL_MOUSEMOTION || event.type == SDL_MOUSEBUTTONDOWN ||
        event.type == SDL_MOUSEBUTTONUP || event.type == SDL_FINGERDOWN ||
        event.type == SDL_FINGERMOTION) {
      lastMouseMotionMs = SDL_GetTicks();
      if (!cursorVisible) {
        SDL_ShowCursor(SDL_ENABLE);
        cursorVisible = true;
      }
    }

    switch (event.type) {
    case SDL_QUIT:
      ctx.appRunning = false;
      return;
    case SDL_KEYDOWN: {
      bool consumed = false;
      Widget *activeModal = nullptr;
      for (auto *w : eventWidgets) {
        if (w->isModalActive()) {
          activeModal = w;
          break;
        }
      }
      if (activeModal) {
        consumed =
            activeModal->onKeyDown(event.key.keysym.sym, event.key.keysym.mod);
      } else {
        for (auto *w : eventWidgets) {
          if (w->onKeyDown(event.key.keysym.sym, event.key.keysym.mod)) {
            consumed = true;
            break;
          }
        }
      }
      if (!consumed) {
        if (event.key.keysym.sym == SDLK_q &&
            (event.key.keysym.mod & KMOD_CTRL)) {
          ctx.appRunning = false;
        }
      }
      break;
    }
    case SDL_FINGERDOWN:
    case SDL_MOUSEBUTTONDOWN:
      lastMouseMotionMs = SDL_GetTicks();
      if (!cursorVisible) {
        SDL_ShowCursor(SDL_ENABLE);
        cursorVisible = true;
        if (appCfg.preventSleep)
          preventRPiSleep(true, ctx.displayPower.get());
      }
      break;
    case SDL_WINDOWEVENT:
      if (event.window.event == SDL_WINDOWEVENT_SIZE_CHANGED) {
        ctx.updateLayoutMetrics();
        {
          float ns = static_cast<float>(ctx.globalDrawH) / LOGICAL_HEIGHT;
          float old = fontMgr.renderScale();
          if (ns > 0.5f && std::fabs(ns - old) / old > 0.05f) {
            fontMgr.setRenderScale(ns);
            // Recalculate UI
            fontMgr.clearCache();
            fontCatalog.recalculate(LOGICAL_WIDTH, LOGICAL_HEIGHT);
            layout.recalculate(LOGICAL_WIDTH, LOGICAL_HEIGHT,
                               ctx.layLogicalOffX, ctx.layLogicalOffY);
            rssBanner->onResize(139 + ctx.layLogicalOffX,
                                412 + ctx.layLogicalOffY, 660, 68);
          }
          lastResizeMs = SDL_GetTicks();
        }
        if (!FIDELITY_MODE) {
          fontCatalog.recalculate(event.window.data1, event.window.data2);
          layout.recalculate(event.window.data1, event.window.data2);
        }
        render(ctx); // renderFrame
      } else if (event.window.event == SDL_WINDOWEVENT_EXPOSED) {
        render(ctx);
      }
      break;
    default:
      break;
    }

    // Dispatch other events
    if (event.type == SDL_MOUSEMOTION || event.type == SDL_MOUSEBUTTONUP ||
        event.type == SDL_MOUSEWHEEL) {
      // ... logic from main ...
      // MOUSEMOTION
      if (event.type == SDL_MOUSEMOTION) {
        int mx = event.motion.x, my = event.motion.y;
        if (FIDELITY_MODE) {
          float pixX = event.motion.x * static_cast<float>(ctx.globalDrawW) /
                       ctx.globalWinW;
          float pixY = event.motion.y * static_cast<float>(ctx.globalDrawH) /
                       ctx.globalWinH;
          mx = static_cast<int>(pixX / ctx.layScale);
          my = static_cast<int>(pixY / ctx.layScale);
        }
        Widget *activeModal = nullptr;
        for (auto *w : eventWidgets)
          if (w->isModalActive()) {
            activeModal = w;
            break;
          }
        if (activeModal)
          activeModal->onMouseMove(mx, my);
        else
          for (auto *w : eventWidgets)
            w->onMouseMove(mx, my);
      }
      // MOUSEBUTTONUP
      else if (event.type == SDL_MOUSEBUTTONUP) {
        if (event.button.button == SDL_BUTTON_LEFT) {
          int mx = event.button.x, my = event.button.y;
          if (FIDELITY_MODE) {
            float pixX = event.button.x * static_cast<float>(ctx.globalDrawW) /
                         ctx.globalWinW;
            float pixY = event.button.y * static_cast<float>(ctx.globalDrawH) /
                         ctx.globalWinH;
            mx = static_cast<int>(pixX / ctx.layScale);
            my = static_cast<int>(pixY / ctx.layScale);
          }
          Widget *activeModal = nullptr;
          for (auto *w : eventWidgets)
            if (w->isModalActive()) {
              activeModal = w;
              break;
            }
          if (activeModal)
            activeModal->onMouseUp(mx, my, SDL_GetModState());
          else
            for (auto *w : eventWidgets)
              if (w->onMouseUp(mx, my, SDL_GetModState()))
                break;
        }
      }
      // MOUSEWHEEL
      else if (event.type == SDL_MOUSEWHEEL) {
        int scrollY = event.wheel.y;
#if SDL_VERSION_ATLEAST(2, 0, 18)
        if (event.wheel.direction == SDL_MOUSEWHEEL_FLIPPED)
          scrollY = -scrollY;
#endif
        for (auto *w : eventWidgets)
          if (w->onMouseWheel(scrollY))
            break;
      }
    }
  }

  if (timePanel->isSetupRequested()) {
    timePanel->clearSetupRequest();
    ctx.activeSetup = AppContext::SetupMode::Main;
    return; // Next main_tick will switch
  }

  // Check DXCluster setup
  DXClusterPanel *dxc =
      dynamic_cast<DXClusterPanel *>(widgetPool[WidgetType::DX_CLUSTER].get());
  if (dxc && dxc->isSetupRequested()) {
    dxc->clearSetupRequest();
    ctx.activeSetup = AppContext::SetupMode::DXCluster;
    return;
  }

  mapArea->setPredictor(dxSatPane->activePredictor());
  auto *gimbal =
      dynamic_cast<GimbalPanel *>(widgetPool[WidgetType::GIMBAL].get());
  if (gimbal) {
    gimbal->setPredictor(dxSatPane->activePredictor());
    gimbal->setObserver(appCfg.lat, appCfg.lon);
  }

  // Recalculate UI call logic
  if (lastResizeMs && (SDL_GetTicks() - lastResizeMs > 200)) {
    lastResizeMs = 0;
    int dw, dh;
    SDL_GetRendererOutputSize(ctx.renderer, &dw, &dh);
    float ns = static_cast<float>(dh) / LOGICAL_HEIGHT;
    if (ns > 0.5f && std::fabs(ns - fontMgr.renderScale()) > 0.01f) {
      fontMgr.setRenderScale(ns);
      fontMgr.clearCache();
      fontCatalog.recalculate(LOGICAL_WIDTH, LOGICAL_HEIGHT);
      layout.recalculate(LOGICAL_WIDTH, LOGICAL_HEIGHT, ctx.layLogicalOffX,
                         ctx.layLogicalOffY);
      rssBanner->onResize(139 + ctx.layLogicalOffX, 412 + ctx.layLogicalOffY,
                          660, 68);
    }
  }

#ifndef __EMSCRIPTEN__
  if (cursorVisible && (SDL_GetTicks() - lastMouseMotionMs > 10000)) {
    SDL_ShowCursor(SDL_DISABLE);
    cursorVisible = false;
  }
#endif

  if (appCfg.preventSleep && (now - lastSleepAssert > 30000)) {
    preventRPiSleep(true);
    lastSleepAssert = now;
  }

  for (auto *w : widgets)
    w->update();
  satMgr->update();
  ctx.brightnessMgr->update();
}

void DashboardContext::render(AppContext &ctx) {
  SDL_SetRenderDrawColor(ctx.renderer, 0, 0, 0, 255);
  SDL_RenderClear(ctx.renderer);

  if (FIDELITY_MODE) {
    SDL_RenderSetViewport(ctx.renderer, nullptr);
    SDL_RenderSetScale(ctx.renderer, ctx.layScale, ctx.layScale);
  }

  Widget *activeModal = nullptr;
  for (auto *w : widgets) {
    if (w->isModalActive())
      activeModal = w;
    SDL_Rect clip = w->getRect();
    SDL_RenderSetClipRect(ctx.renderer, &clip);
    w->render(ctx.renderer);
  }
  SDL_RenderSetClipRect(ctx.renderer, nullptr);

  if (activeModal) {
    SDL_SetRenderDrawBlendMode(ctx.renderer, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(ctx.renderer, 0, 0, 0, 150);
    SDL_Rect full = {0, 0, LOGICAL_WIDTH, LOGICAL_HEIGHT};
    SDL_RenderFillRect(ctx.renderer, &full);
    activeModal->renderModal(ctx.renderer);
  }

  SDL_RenderPresent(ctx.renderer);
  if (FIDELITY_MODE) {
    SDL_RenderSetScale(ctx.renderer, 1.0f, 1.0f);
  }
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
    // Destroy dashboard if needed
    if (ctx.dashboard)
      ctx.dashboard.reset();

    ctx.updateLayoutMetrics();

    // Initial setup init
    if (!ctx.setupWidget) {
      auto setupFontMgr = std::make_unique<FontManager>();
      setupFontMgr->loadFromMemory(assets_font_ttf, assets_font_ttf_len,
                                   DEFAULT_FONT_SIZE);
      if (FIDELITY_MODE)
        setupFontMgr->setRenderScale(ctx.layScale);

      ctx.setupFontMgr = std::move(setupFontMgr);

      int setupW = LOGICAL_WIDTH;
      int setupH = LOGICAL_HEIGHT;
      int setupX = 0;
      int setupY = 0;

      if (ctx.activeSetup == AppContext::SetupMode::Main) {
        auto s = std::make_unique<SetupScreen>(setupX, setupY, setupW, setupH,
                                               *ctx.setupFontMgr,
                                               *ctx.brightnessMgr);
        s->setConfig(ctx.appCfg);
        ctx.setupWidget = std::move(s);
      } else if (ctx.activeSetup == AppContext::SetupMode::DXCluster) {
        auto s = std::make_unique<DXClusterSetup>(setupX, setupY, setupW,
                                                  setupH, *ctx.setupFontMgr);
        s->setConfig(ctx.appCfg);
        ctx.setupWidget = std::move(s);
      }
      SDL_StartTextInput();
    }

    // Logic
    bool setupDone = false;
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
        else if (event.type == SDL_MOUSEBUTTONUP) {
          int smx = event.button.x, smy = event.button.y;
          if (FIDELITY_MODE) {
            float pixX = event.button.x * static_cast<float>(ctx.globalDrawW) /
                         ctx.globalWinW;
            float pixY = event.button.y * static_cast<float>(ctx.globalDrawH) /
                         ctx.globalWinH;
            smx = static_cast<int>(pixX / ctx.layScale);
            smy = static_cast<int>(pixY / ctx.layScale);
          }
          ctx.setupWidget->onMouseUp(smx, smy, SDL_GetModState());
        } else if (event.type == SDL_WINDOWEVENT &&
                   event.window.event == SDL_WINDOWEVENT_SIZE_CHANGED) {
          ctx.updateLayoutMetrics();
          ctx.setupWidget->onResize(0, 0, LOGICAL_WIDTH, LOGICAL_HEIGHT);
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
    SDL_RenderPresent(ctx.renderer);
    if (FIDELITY_MODE) {
      SDL_RenderSetScale(ctx.renderer, 1.0f, 1.0f);
    }

    // Check Done
    if (ctx.activeSetup == AppContext::SetupMode::Main) {
      if (static_cast<SetupScreen *>(ctx.setupWidget.get())->isComplete())
        setupDone = true;
    } else if (ctx.activeSetup == AppContext::SetupMode::DXCluster) {
      if (static_cast<DXClusterSetup *>(ctx.setupWidget.get())->isComplete())
        setupDone = true;
    }

    if (setupDone) {
      SDL_StopTextInput();
      // Save logic
      if (ctx.activeSetup == AppContext::SetupMode::Main) {
        auto *s = static_cast<SetupScreen *>(ctx.setupWidget.get());
        if (!s->wasCancelled())
          ctx.appCfg = s->getConfig();
      } else {
        if (static_cast<DXClusterSetup *>(ctx.setupWidget.get())->isSaved())
          ctx.appCfg = static_cast<DXClusterSetup *>(ctx.setupWidget.get())
                           ->updateConfig(ctx.appCfg);
      }
      ctx.cfgMgr.save(ctx.appCfg);
      ctx.setupWidget.reset();
      ctx.setupFontMgr.reset();
      ctx.activeSetup = AppContext::SetupMode::None;
      // Update state
      ctx.state->deCallsign = ctx.appCfg.callsign;
      ctx.state->deGrid = ctx.appCfg.grid;
      ctx.state->deLocation = {ctx.appCfg.lat, ctx.appCfg.lon};
    }

  } else {
    // Dashboard
    if (!ctx.dashboard) {
      ctx.dashboard = std::make_unique<DashboardContext>(ctx);
    }

    // Apply any config changes injected by the WebServer API (RPi/framebuffer
    // remote-control scenario).  The WebServer thread writes to ctx.appCfg
    // under the config mutex and then sets this flag; we re-apply live state
    // here on the main thread so no SDL calls happen off-thread.
    if (ctx.configReloadRequested.exchange(false, std::memory_order_acq_rel)) {
      ctx.state->deCallsign = ctx.appCfg.callsign;
      ctx.state->deGrid = ctx.appCfg.grid;
      ctx.state->deLocation = {ctx.appCfg.lat, ctx.appCfg.lon};
      ctx.netManager->setCorsProxyUrl(ctx.appCfg.corsProxyUrl);
      // Re-apply theme/metric to all live widgets without rebuilding dashboard
      if (ctx.dashboard) {
        for (auto const &[type, widget] : ctx.dashboard->widgetPool)
          if (widget) {
            widget->setTheme(ctx.appCfg.theme);
            widget->setMetric(ctx.appCfg.useMetric);
          }
        ctx.dashboard->timePanel->setTheme(ctx.appCfg.theme);
        ctx.dashboard->timePanel->setMetric(ctx.appCfg.useMetric);
        ctx.dashboard->mapArea->setTheme(ctx.appCfg.theme);
        ctx.dashboard->mapArea->setMetric(ctx.appCfg.useMetric);
        ctx.dashboard->localPanel->setTheme(ctx.appCfg.theme);
        ctx.dashboard->localPanel->setMetric(ctx.appCfg.useMetric);
        ctx.dashboard->widgetSelector->setTheme(ctx.appCfg.theme);
      }
      LOG_I("Main", "Config reloaded from remote API: callsign={}",
            ctx.appCfg.callsign);
    }

    ctx.dashboard->update(ctx);
    ctx.dashboard->render(ctx);

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
