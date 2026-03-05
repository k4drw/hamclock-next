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
#include "core/PrefixManager.h"
#include "core/RSSData.h"
#include "core/RigData.h"
#include "core/RotatorData.h"
#include "core/SatelliteManager.h"
#include "core/SolarData.h"
#include "core/SoundManager.h"
#include "core/WidgetType.h"
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
#include "services/BandConditionsProvider.h"
#include "services/BeaconProvider.h"
#include "services/CallbookProvider.h"
#include "services/CloudProvider.h"
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
#include "ui/DXClusterSetup.h"
#include "ui/DXSatPane.h"
#include "ui/DebugOverlay.h"
#include "ui/DstPanel.h"
#include "ui/EMEToolPanel.h"
#include "ui/ENVPanel.h"
#include "ui/EmbeddedFont.h"
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
  ConfigManager &cfgMgr = ConfigManager::instance();
  std::shared_ptr<HamClockState> state;
  bool appRunning = true;
  bool showActionHighlights = false;

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
  std::shared_ptr<AuroraHistoryStore> auroraHistoryStore;
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
  std::shared_ptr<AlertsStore> alertsStore;
  std::shared_ptr<ForecastStore> forecastStore;
  std::shared_ptr<RepeaterStore> repeaterStore;
  std::shared_ptr<HurricaneStore> hurricaneStore;
  std::shared_ptr<MarineStore> marineStore;
  std::shared_ptr<WinlinkStore> winlinkStore;
  std::shared_ptr<DRAPDataStore> drapDataStore;
  std::shared_ptr<XRayHistoryStore> xrayHistoryStore;
  std::shared_ptr<GreylineDXStore> greylineDXStore;
  std::shared_ptr<AuroraMapStore> auroraMapStore;

  // Managers & Services
  std::unique_ptr<NetworkManager> netManager;
  PrefixManager prefixMgr;
  std::shared_ptr<DisplayPower> displayPower;
  std::shared_ptr<BrightnessManager> brightnessMgr;
  std::shared_ptr<CPUMonitor> cpuMonitor;

#ifndef __EMSCRIPTEN__
  std::unique_ptr<WebServer> webServer;
  std::unique_ptr<FrameCapture> frameCapture;
  std::unique_ptr<UpdateChecker> updateChecker;
  std::unique_ptr<GPSProvider> gpsProvider;
#endif
  std::unique_ptr<BME280Provider> bmeProvider;

  // Setup State
  enum class SetupMode { None, Loading, Main, DXCluster };
  SetupMode activeSetup = SetupMode::None;
  std::unique_ptr<Widget> setupWidget;
  std::unique_ptr<FontManager> setupFontMgr;
  std::unique_ptr<FontCatalog> setupCatalog;

  // Remote-config reload signal.  WebServer thread sets this to true after a
  // successful POST /api/reload or /set_config; main_tick() reads and clears
  // it, then re-applies the in-memory config to live state (callsign, proxy,
  // themes, etc.) without tearing down the dashboard.
  std::atomic<bool> configReloadRequested{false};
  // Rotation control commands written by WebServer thread, consumed on main
  // thread. rotationCmd: 0=none, 1=pause, 2=resume, 3=next, 4=jump-to-widget
  // rotationCmdPane: -1=all panes, 0-5=specific pane
  // rotationCmdWidget: WidgetType as int (used with cmd=4)
  std::atomic<int> rotationCmd{0};
  std::atomic<int> rotationCmdPane{-1};
  std::atomic<int> rotationCmdWidget{-1};
  bool startOnUpdateTab = false;
  bool startOnServicesTab = false;

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
  FccProvider fccProvider;
  std::unique_ptr<DstProvider> dstProvider;
  std::unique_ptr<ADIFProvider> adifProvider;
  std::unique_ptr<MufRtProvider> mufRtProvider;
  std::unique_ptr<CloudProvider> cloudProvider;
  std::unique_ptr<IonosondeProvider> ionosondeProvider;
  std::unique_ptr<ReachProvider> reachProvider;
  std::unique_ptr<SantaProvider> santaProvider;
  std::unique_ptr<TropoProvider> tropoProvider;
  std::unique_ptr<LightningProvider> lightningProvider;
  std::unique_ptr<MeteorProvider> meteorProvider;
  std::unique_ptr<SolarStormProvider> solarStormProvider;
  std::unique_ptr<SatelliteManager> satMgr;
  std::unique_ptr<AsteroidProvider> asteroidProvider;
  std::unique_ptr<BeaconProvider> beaconProvider;
  std::unique_ptr<AlertsProvider> alertsProvider;
  std::unique_ptr<ForecastProvider> forecastProvider;
  std::unique_ptr<RepeaterProvider> repeaterProvider;
  std::unique_ptr<HurricaneProvider> hurricaneProvider;
  std::unique_ptr<MarineProvider> marineProvider;
  std::unique_ptr<WinlinkProvider> winlinkProvider;
  std::unique_ptr<GreylineDXProvider> greylineDXProvider;

  // Services
#ifndef __EMSCRIPTEN__
  std::unique_ptr<RotatorService> rotatorService;
  std::unique_ptr<RigService> rigService;
#endif

  // UI Components
  std::unique_ptr<TimePanel> timePanel;
  std::unique_ptr<WidgetSelector> widgetSelector;
  std::vector<std::unique_ptr<PaneContainer>> panes;
  std::unique_ptr<MapWidget> mapArea;
  std::unique_ptr<RSSBanner> rssBanner;
  std::unique_ptr<UpdateOverlay> updateOverlay;
  LayoutManager layout;

  // Collections
  std::map<WidgetType, std::unique_ptr<Widget>> widgetPool;
  std::vector<Widget *> widgets;
  std::vector<Widget *> eventWidgets;

  // Widget factory — stored as a member so pane containers hold a valid [this]
  // capture rather than a dangling reference to a constructor-local lambda.
  std::function<Widget *(WidgetType)> widgetFactory_;

  // State
  Uint32 lastFetchMs = 0;
  Uint32 lastGreylineFetchMs = 0;
  Uint32 lastReachFetchMs = 0;
  Uint32 lastDrapFetchMs = 0;
  Uint32 lastResizeMs = 0;
  Uint32 lastFpsUpdate = 0;
  int frames = 0;
  Uint32 lastMouseMotionMs = 0;
  bool cursorVisible = true;
  Uint32 lastSleepAssert = 0;
  Uint32 lastMemLogMs = 0;

  // State for background data aggregation
  std::vector<std::string> rssHeadlines[3];
  bool rssDataDirty = false;

  DashboardContext(AppContext &ctx);
  ~DashboardContext() = default;

  void applySidePanelMode(WidgetType chosen, AppContext &ctx);
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
    ctx.netManager->setHubConfig(ctx.appCfg.hubMode, ctx.appCfg.hubIp,
                                 ctx.appCfg.hubPort);
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
  bool forceLiveWeb = false;
  std::string logLevel = "warn";

  for (int i = 1; i < argc; ++i) {
    std::string arg = argv[i];
    if (arg == "-f" || arg == "--fullscreen") {
      forceFullscreen = true;
    } else if (arg == "-s" || arg == "--software") {
      forceSoftware = true;
    } else if (arg == "--live-web") {
      forceLiveWeb = true;
    } else if (arg == "--no-audio") {
      SoundManager::getInstance().disable();
    } else if (arg == "--log-level" && i + 1 < argc) {
      logLevel = argv[++i];
    } else if (arg == "-h" || arg == "--help") {
      std::printf("Usage: hamclock-next [options]\n");
      return EXIT_SUCCESS;
    }
  }

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
  ctx.netManager->setHubConfig(ctx.appCfg.hubMode, ctx.appCfg.hubIp,
                               ctx.appCfg.hubPort);

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
  if (ctx.watchlistStore->getAll().empty()) {
    ctx.watchlistStore->add("K1ABC");
    ctx.watchlistStore->add("W1AW");
    ctx.appCfg.watchlist = {"K1ABC", "W1AW"};
  }

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
  ctx.webServer->start();

  ctx.gpsProvider = std::make_unique<GPSProvider>(ctx.state.get(), ctx.appCfg);
  ctx.gpsProvider->start();

  ctx.bmeProvider = std::make_unique<BME280Provider>(ctx.deWeatherStore);
  ctx.bmeProvider->start();
#endif

  // Audio device is opened lazily on first playAlarm() call.
  // Use --no-audio to permanently suppress audio (e.g. displays with buzzers).

  // --- Main Loop ---
#ifdef __EMSCRIPTEN__
  emscripten_set_main_loop(main_tick, 10, 1);
#else
  while (ctx.appRunning) {
    static constexpr Uint32 kTargetFrameMs = 100; // ~10 FPS
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

  // Generate procedural textures
  texMgr.generateLineTexture(ctx.renderer, "line_aa");
  texMgr.generateMarkerTextures(ctx.renderer);
  texMgr.generateWhiteTexture(ctx.renderer);
  texMgr.generateBlackTexture(ctx.renderer);

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
  auto auroraHistoryStore = ctx.auroraHistoryStore;
  auto auroraMapStore = ctx.auroraMapStore;
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

  // Returns true if the widget appears in any pane rotation.
  // AURORA_GRAPH is always treated as configured: its history store
  // needs continuous sampling even when the pane is off-screen.
  auto isWidgetConfigured = [&](WidgetType type) -> bool {
    if (type == WidgetType::AURORA_GRAPH)
      return true;
    for (auto t : appCfg.pane1Rotation)
      if (t == type)
        return true;
    for (auto t : appCfg.pane2Rotation)
      if (t == type)
        return true;
    for (auto t : appCfg.pane3Rotation)
      if (t == type)
        return true;
    for (auto t : appCfg.pane4Rotation)
      if (t == type)
        return true;
    for (auto t : appCfg.pane5Rotation)
      if (t == type)
        return true;
    for (auto t : appCfg.pane6Rotation)
      if (t == type)
        return true;
    return false;
  };
  const bool isMasterMode = (appCfg.hubMode == HubMode::Master);

  ctx.xrayHistoryStore = std::make_shared<XRayHistoryStore>();
  ctx.drapDataStore = std::make_shared<DRAPDataStore>();
  noaaProvider =
      std::make_unique<NOAAProvider>(netManager, solarStore, auroraHistoryStore,
                                     ctx.xrayHistoryStore, state.get());
  noaaProvider->setDrapStore(ctx.drapDataStore);
  noaaProvider->setAuroraMapStore(ctx.auroraMapStore);
  if (isMasterMode || isWidgetConfigured(WidgetType::SOLAR) ||
      isWidgetConfigured(WidgetType::AURORA) ||
      isWidgetConfigured(WidgetType::AURORA_GRAPH) ||
      isWidgetConfigured(WidgetType::DRAP) ||
      appCfg.propOverlay == PropOverlayType::Aurora)
    noaaProvider->fetch();
  if (appCfg.propOverlay == PropOverlayType::Drap)
    noaaProvider->fetchDRAP();
  if (appCfg.propOverlay == PropOverlayType::Aurora)
    noaaProvider->fetchAuroraMap();

  rssProvider = std::make_unique<RSSProvider>(netManager, rssStore);
  rssProvider->fetch();

  spotProvider = std::make_unique<LiveSpotProvider>(
      netManager, spotStore, appCfg, state.get(), dxcStore);
  if (isMasterMode || isWidgetConfigured(WidgetType::LIVE_SPOTS) ||
      appCfg.propOverlay != PropOverlayType::None)
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

  // Re-fetch POTA spots once the parks CSV is parsed so spots get coordinates.
  // The CSV is read/parsed in a background thread; spots often arrive first.
  ActivityLocationManager::getInstance().setOnParksReady([this]() {
    if (activityProvider)
      activityProvider->fetch();
  });

  dxcProvider = std::make_unique<DXClusterProvider>(
      dxcStore, ctx.prefixMgr, watchlistStore, watchlistHitStore, state.get());
#ifndef __EMSCRIPTEN__
  if (isMasterMode || isWidgetConfigured(WidgetType::DX_CLUSTER))
    dxcProvider->start(appCfg);
#endif

  rbnProvider =
      std::make_unique<RBNProvider>(dxcStore, ctx.prefixMgr, state.get());
#ifndef __EMSCRIPTEN__
  if ((isMasterMode || isWidgetConfigured(WidgetType::DX_CLUSTER)) &&
      appCfg.rbnEnabled)
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
  if (isMasterMode || isWidgetConfigured(WidgetType::HISTORY_FLUX))
    historyProvider->fetchFlux();
  if (isMasterMode || isWidgetConfigured(WidgetType::HISTORY_SSN))
    historyProvider->fetchSSN();
  if (isMasterMode || isWidgetConfigured(WidgetType::HISTORY_KP))
    historyProvider->fetchKp();

  deWeatherProvider =
      std::make_unique<WeatherProvider>(netManager, deWeatherStore, 0);
  deWeatherProvider->fetch(state->deLocation.lat, state->deLocation.lon);

  dxWeatherProvider =
      std::make_unique<WeatherProvider>(netManager, dxWeatherStore, 1);
  dxWeatherProvider->fetch(state->dxLocation.lat, state->dxLocation.lon);

  sdoProvider = std::make_unique<SDOProvider>(netManager);
  drapProvider = std::make_unique<DRAPProvider>(netManager, ctx.drapDataStore);
  auroraProvider = std::make_shared<AuroraProvider>(netManager);

  callbookProvider =
      std::make_shared<CallbookProvider>(netManager, callbookStore);
  callbookProvider->lookup("K1ABC");

  dstProvider = std::make_unique<DstProvider>(netManager, dstStore);
  if (isMasterMode || isWidgetConfigured(WidgetType::DST_INDEX))
    dstProvider->fetch();

  adifProvider = std::make_unique<ADIFProvider>(adifStore, ctx.prefixMgr);
  adifProvider->fetch(ctx.cfgMgr.configDir() / "logs.adif");

  mufRtProvider = std::make_unique<MufRtProvider>(netManager);
  mufRtProvider->update();

  cloudProvider = std::make_unique<CloudProvider>(netManager);
  cloudProvider->update();

  asteroidProvider = std::make_unique<AsteroidProvider>(netManager);
  asteroidProvider->update();

  beaconProvider = std::make_unique<BeaconProvider>();

  alertsProvider =
      std::make_unique<AlertsProvider>(netManager, ctx.alertsStore);
  if (isMasterMode || isWidgetConfigured(WidgetType::ALERTS))
    alertsProvider->fetch(appCfg.lat, appCfg.lon);

  forecastProvider =
      std::make_unique<ForecastProvider>(netManager, ctx.forecastStore);
  if (isMasterMode || isWidgetConfigured(WidgetType::FORECAST))
    forecastProvider->fetch(appCfg.lat, appCfg.lon);

  repeaterProvider =
      std::make_unique<RepeaterProvider>(netManager, ctx.repeaterStore);
  // RepeaterBook API requires auth key — fetch disabled until configured.
  // repeaterProvider->fetch(appCfg.lat, appCfg.lon);

  hurricaneProvider =
      std::make_unique<HurricaneProvider>(netManager, ctx.hurricaneStore);
  if (isMasterMode || isWidgetConfigured(WidgetType::HURRICANE))
    hurricaneProvider->fetch();

  marineProvider =
      std::make_unique<MarineProvider>(netManager, ctx.marineStore);
  // Default NOAA tide station (Lake Worth FL) + NDBC buoy 41114 (FL Atlantic).
  // TODO: make configurable via settings.
  if (isMasterMode || isWidgetConfigured(WidgetType::MARINE))
    marineProvider->fetch(appCfg.marineStation, appCfg.marineBuoy);

  winlinkProvider =
      std::make_unique<WinlinkProvider>(netManager, ctx.winlinkStore);
  // Winlink API requires access key — fetch disabled until configured.
  // winlinkProvider->fetch(appCfg.lat, appCfg.lon);

  greylineDXProvider =
      std::make_unique<GreylineDXProvider>(ctx.prefixMgr, ctx.greylineDXStore);
  greylineDXProvider->update();

  santaProvider = std::make_unique<SantaProvider>(santaStore);
  santaProvider->update();

  tropoProvider = std::make_unique<TropoProvider>(netManager);
  tropoProvider->setCallback([this](const TropoData &d) {
    if (widgetPool.count(WidgetType::TROPO)) {
      static_cast<TropoPanel *>(widgetPool[WidgetType::TROPO].get())
          ->updateData(d);
    }
  });

  lightningProvider = std::make_unique<LightningProvider>(netManager);
  lightningProvider->setCallback([this](const LightningData &d) {
    if (widgetPool.count(WidgetType::LIGHTNING)) {
      static_cast<LightningPanel *>(widgetPool[WidgetType::LIGHTNING].get())
          ->updateData(d);
    }
  });

  meteorProvider = std::make_unique<MeteorProvider>();
  meteorProvider->setCallback([this](const MeteorData &d) {
    if (widgetPool.count(WidgetType::METEOR)) {
      static_cast<MeteorPanel *>(widgetPool[WidgetType::METEOR].get())
          ->updateData(d);
    }
  });

  solarStormProvider = std::make_unique<SolarStormProvider>(netManager);
  solarStormProvider->setCallback([this](const SolarStormData &d) {
    if (widgetPool.count(WidgetType::SOLAR_STORM)) {
      static_cast<SolarStormPanel *>(widgetPool[WidgetType::SOLAR_STORM].get())
          ->updateData(d);
    }
  });

  ionosondeProvider = std::make_unique<IonosondeProvider>(netManager);
  ionosondeProvider->setCallback([this](const IonosondeData &d) {
    if (widgetPool.count(WidgetType::IONOSONDE)) {
      static_cast<IonosondePanel *>(widgetPool[WidgetType::IONOSONDE].get())
          ->updateData(d);
    }
  });

  reachProvider = std::make_unique<ReachProvider>(netManager, state);
  reachProvider->setCallback([this](const ReachData &d) {
    if (mapArea) {
      mapArea->onPropDataReady(PropOverlayType::Heatmap, d.grid);
    }
  });

  timePanel =
      std::make_unique<TimePanel>(0, 0, 0, 0, fontMgr, texMgr, appCfg.callsign);
  timePanel->setCallColor(appCfg.callsignColor);
  timePanel->setOnConfigChanged(
      [&ctx](const std::string &call, SDL_Color color) {
        ctx.appCfg.callsign = call;
        ctx.appCfg.callsignColor = color;
        ctx.cfgMgr.save(ctx.appCfg);
      });

  timePanel->setOnPauseRotation([this, &tp = *timePanel]() {
    bool nowPaused = !panes[0]->isPaused();
    for (auto &p : panes)
      p->setPaused(nowPaused);
    tp.setRotationPaused(nowPaused);
  });

  timePanel->setOnNextRotation([this]() {
    for (auto &p : panes)
      p->forceAdvance();
  });

  timePanel->initPresets(&appCfg, [this, &ctx]() {
    panes[0]->setRotation(ctx.appCfg.pane1Rotation,
                          ctx.appCfg.rotationIntervalS,
                          ctx.appCfg.syncRotation);
    panes[1]->setRotation(ctx.appCfg.pane2Rotation,
                          ctx.appCfg.rotationIntervalS,
                          ctx.appCfg.syncRotation);
    panes[2]->setRotation(ctx.appCfg.pane3Rotation,
                          ctx.appCfg.rotationIntervalS,
                          ctx.appCfg.syncRotation);
    panes[3]->setRotation(ctx.appCfg.pane4Rotation,
                          ctx.appCfg.rotationIntervalS,
                          ctx.appCfg.syncRotation);
    panes[4]->setRotation(ctx.appCfg.pane5Rotation,
                          ctx.appCfg.rotationIntervalS,
                          ctx.appCfg.syncRotation);
    panes[5]->setRotation(ctx.appCfg.pane6Rotation,
                          ctx.appCfg.rotationIntervalS,
                          ctx.appCfg.syncRotation);
    ctx.cfgMgr.save(ctx.appCfg);
  });

  widgetSelector = std::make_unique<WidgetSelector>(fontMgr);

  // Helper for pool (Lazy loading) — assigned to a member so pane-container
  // factory lambdas (captured by [this]) don't dangle after construction.
  widgetFactory_ = [this, &ctx](WidgetType type) -> Widget * {
    auto &appCfg = ctx.appCfg;
    auto &netManager = *ctx.netManager;
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
    auto state = ctx.state;
    auto auroraHistoryStore = ctx.auroraHistoryStore;
    if (widgetPool.count(type) && widgetPool[type])
      return widgetPool[type].get();

    switch (type) {
    case WidgetType::SOLAR:
      widgetPool[type] = std::make_unique<SpaceWeatherPanel>(
          0, 0, 0, 0, fontMgr, texMgr, solarStore, ctx.xrayHistoryStore);
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
          std::make_unique<DstPanel>(0, 0, 0, 0, fontMgr, texMgr, dstStore);
      break;
    case WidgetType::WATCHLIST:
      widgetPool[type] = std::make_unique<WatchlistPanel>(
          0, 0, 0, 0, fontMgr, watchlistStore, watchlistHitStore);
      break;
    case WidgetType::EME_TOOL:
      widgetPool[type] = std::make_unique<EMEToolPanel>(0, 0, 0, 0, fontMgr,
                                                        texMgr, moonStore);
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
      widgetPool[type] = std::make_unique<GimbalPanel>(0, 0, 0, 0, fontMgr,
                                                       texMgr, rotatorStore);
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
      widgetPool[type] = std::make_unique<AuroraGraphPanel>(
          0, 0, 0, 0, fontMgr, texMgr, auroraHistoryStore);
      break;
    case WidgetType::ADIF:
      widgetPool[type] =
          std::make_unique<ADIFPanel>(0, 0, 0, 0, fontMgr, adifStore);
      break;
    case WidgetType::COUNTDOWN:
      widgetPool[type] = std::make_unique<CountdownPanel>(
          0, 0, 0, 0, fontMgr, ctx.appCfg,
          [&ctx]() { ctx.cfgMgr.save(ctx.appCfg); });
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
      widgetPool[type] =
          std::make_unique<BeaconPanel>(0, 0, 0, 0, fontMgr, *beaconProvider);
      break;
    case WidgetType::SDO:
      widgetPool[type] =
          std::make_unique<SDOPanel>(0, 0, 0, 0, fontMgr, texMgr, *sdoProvider);
      break;
    case WidgetType::SYS_INFO:
      widgetPool[type] = std::make_unique<SysInfoPanel>(
          0, 0, 0, 0, fontMgr, ctx.cpuMonitor, ctx.state, appCfg.useMetric);
      break;
    case WidgetType::ASTEROID:
      widgetPool[type] = std::make_unique<AsteroidPanel>(
          0, 0, 0, 0, fontMgr, *asteroidProvider, ctx.state, &appCfg,
          [&ctx]() { ctx.cfgMgr.save(ctx.appCfg); });
      break;
    case WidgetType::ALERTS:
      widgetPool[type] =
          std::make_unique<AlertsPanel>(0, 0, 0, 0, fontMgr, ctx.alertsStore);
      break;
    case WidgetType::FORECAST:
      widgetPool[type] = std::make_unique<ForecastPanel>(0, 0, 0, 0, fontMgr,
                                                         ctx.forecastStore);
      break;
    case WidgetType::REPEATER_DIR:
      widgetPool[type] = std::make_unique<RepeaterPanel>(0, 0, 0, 0, fontMgr,
                                                         ctx.repeaterStore);
      break;
    case WidgetType::HURRICANE:
      widgetPool[type] = std::make_unique<HurricanePanel>(0, 0, 0, 0, fontMgr,
                                                          ctx.hurricaneStore);
      break;
    case WidgetType::MARINE:
      widgetPool[type] =
          std::make_unique<MarinePanel>(0, 0, 0, 0, fontMgr, ctx.marineStore);
      break;
    case WidgetType::WINLINK:
      widgetPool[type] =
          std::make_unique<WinlinkPanel>(0, 0, 0, 0, fontMgr, ctx.winlinkStore);
      break;
    case WidgetType::GREYLINE_DX:
      widgetPool[type] = std::make_unique<GreylineDXPanel>(0, 0, 0, 0, fontMgr,
                                                           ctx.greylineDXStore);
      break;
    case WidgetType::STOPWATCH:
      widgetPool[type] = std::make_unique<StopwatchPanel>(0, 0, 0, 0, fontMgr);
      break;
    case WidgetType::REMINDER:
      widgetPool[type] = std::make_unique<ReminderPanel>(
          0, 0, 0, 0, fontMgr, ctx.appCfg, ctx.cfgMgr, *callbookProvider,
          callbookStore, fccProvider);
      break;
    case WidgetType::TROPO:
      widgetPool[type] = std::make_unique<TropoPanel>(0, 0, 0, 0, fontMgr);
      break;
    case WidgetType::LIGHTNING:
      widgetPool[type] = std::make_unique<LightningPanel>(0, 0, 0, 0, fontMgr);
      break;
    case WidgetType::METEOR:
      widgetPool[type] =
          std::make_unique<MeteorPanel>(0, 0, 0, 0, fontMgr, texMgr);
      break;
    case WidgetType::IONOSONDE:
      widgetPool[type] =
          std::make_unique<IonosondePanel>(0, 0, 0, 0, fontMgr, texMgr);
      break;
    case WidgetType::SOLAR_STORM:
      widgetPool[type] =
          std::make_unique<SolarStormPanel>(0, 0, 0, 0, fontMgr, texMgr);
      break;
    case WidgetType::DE_INFO:
      widgetPool[type] = std::make_unique<LocalPanel>(0, 0, 0, 0, fontMgr,
                                                      state, deWeatherStore);
      break;
    case WidgetType::DX_INFO: {
      auto p = std::make_unique<DXSatPane>(0, 0, 0, 0, fontMgr, texMgr, state,
                                           *satMgr, dxWeatherStore);
      p->setObserver(appCfg.lat, appCfg.lon);
      p->restoreState(appCfg.panelMode, appCfg.selectedSatellite);
      p->setMapTrackVisible(appCfg.showSatTrack);
      p->setOnModeChanged(
          [&ctx](const std::string &mode, const std::string &satName) {
            ctx.appCfg.panelMode = mode;
            ctx.appCfg.selectedSatellite = satName;
            ctx.cfgMgr.save(ctx.appCfg);
          });
      p->setOnMapTrackToggle([&ctx](bool enabled) {
        ctx.appCfg.showSatTrack = enabled;
        ctx.cfgMgr.save(ctx.appCfg);
      });
      widgetPool[type] = std::move(p);
      break;
    }
    case WidgetType::ENV_TEMP:
      widgetPool[type] = std::make_unique<ENVPanel>(
          0, 0, 0, 0, fontMgr, deWeatherStore, WidgetType::ENV_TEMP);
      break;
    case WidgetType::ENV_PRESSURE:
      widgetPool[type] = std::make_unique<ENVPanel>(
          0, 0, 0, 0, fontMgr, deWeatherStore, WidgetType::ENV_PRESSURE);
      break;
    case WidgetType::ENV_HUMIDITY:
      widgetPool[type] = std::make_unique<ENVPanel>(
          0, 0, 0, 0, fontMgr, deWeatherStore, WidgetType::ENV_HUMIDITY);
      break;
    case WidgetType::ENV_DEWPOINT:
      widgetPool[type] = std::make_unique<ENVPanel>(
          0, 0, 0, 0, fontMgr, deWeatherStore, WidgetType::ENV_DEWPOINT);
      break;
    default:
      widgetPool[type] = std::make_unique<PlaceholderWidget>(
          0, 0, 0, 0, fontMgr, widgetTypeDisplayName(type),
          SDL_Color{0, 200, 255, 255});
      break;
    }

    // Wire callbacks for newly created widgets
    if (type == WidgetType::DX_CLUSTER) {
      auto *dxcPanel = dynamic_cast<DXClusterPanel *>(widgetPool[type].get());
      if (dxcPanel) {
        dxcPanel->setOnSpotActivated(
            [state, activityStore](const DXClusterSpot &spot) {
              state->dxCallsign = spot.txCall;
              state->dxLocation = {spot.txLat, spot.txLon};
              state->dxGrid = spot.txGrid;
              state->dxActive = (spot.txLat != 0.0 || spot.txLon != 0.0);
              auto ad = activityStore->get();
              ad.hasSelection = false;
              activityStore->set(ad);
            });
        dxcPanel->setOnSpotDeactivated([state]() {
          if (state->mapDxActive) {
            state->dxLocation = state->mapDxLocation;
            state->dxGrid = state->mapDxGrid;
            state->dxActive = true;
          } else {
            state->dxActive = false;
          }
          state->dxCallsign.clear();
        });
      }
    } else if (type == WidgetType::ON_THE_AIR) {
      auto *ontaPanel = dynamic_cast<ONTAPanel *>(widgetPool[type].get());
      if (ontaPanel) {
        ontaPanel->setOnSpotActivated([state, dxcStore](const ONTASpot &spot) {
          state->dxCallsign = spot.call;
          state->dxLocation = {spot.lat, spot.lon};
          state->dxGrid = (spot.lat != 0.0 || spot.lon != 0.0)
                              ? Astronomy::latLonToGrid(spot.lat, spot.lon)
                              : "";
          state->dxActive = (spot.lat != 0.0 || spot.lon != 0.0);
          dxcStore->clearSelection();
        });
        ontaPanel->setOnSpotDeactivated([state]() {
          if (state->mapDxActive) {
            state->dxLocation = state->mapDxLocation;
            state->dxGrid = state->mapDxGrid;
            state->dxActive = true;
          } else {
            state->dxActive = false;
          }
          state->dxCallsign.clear();
        });
      }
    }

    if (widgetPool[type]) {
      widgetPool[type]->setTheme(appCfg.theme);
      widgetPool[type]->setMetric(appCfg.useMetric);
    }

    return widgetPool[type].get();
  };

  std::vector<WidgetType> allTypes = {
      WidgetType::ADIF,          WidgetType::ALERTS,
      WidgetType::ASTEROID,      WidgetType::AURORA,
      WidgetType::AURORA_GRAPH,  WidgetType::BAND_CONDITIONS,
      WidgetType::CALLBOOK,      WidgetType::CLOCK_AUX,
      WidgetType::CONTESTS,      WidgetType::COUNTDOWN,
      WidgetType::DE_WEATHER,    WidgetType::DRAP,
      WidgetType::DST_INDEX,     WidgetType::DX_CLUSTER,
      WidgetType::DX_PEDITIONS,  WidgetType::DX_WEATHER,
      WidgetType::EME_TOOL,      WidgetType::FORECAST,
      WidgetType::GIMBAL,        WidgetType::HISTORY_FLUX,
      WidgetType::HISTORY_KP,    WidgetType::HISTORY_SSN,
      WidgetType::HURRICANE,     WidgetType::LIVE_SPOTS,
      WidgetType::MARINE,        WidgetType::MOON,
      WidgetType::NCDXF,         WidgetType::ON_THE_AIR,
      WidgetType::SANTA_TRACKER, WidgetType::SDO,
      WidgetType::SOLAR,         WidgetType::SYS_INFO,
      WidgetType::WATCHLIST,     WidgetType::STOPWATCH,
      WidgetType::REMINDER,      WidgetType::TROPO,
      WidgetType::LIGHTNING,     WidgetType::METEOR,
      WidgetType::IONOSONDE,     WidgetType::SOLAR_STORM,
      WidgetType::DE_INFO,       WidgetType::DX_INFO,
      WidgetType::ENV_TEMP,      WidgetType::ENV_PRESSURE,
      WidgetType::ENV_HUMIDITY,  WidgetType::ENV_DEWPOINT,
      WidgetType::GREYLINE_DX};

  if (!appCfg.repeaterBookKey.empty()) {
    allTypes.push_back(WidgetType::REPEATER_DIR);
  }
  if (!appCfg.winlinkKey.empty()) {
    allTypes.push_back(WidgetType::WINLINK);
  }

  // Remove eager loading loop
  // for (auto t : allTypes)
  //   addToPool(t);

  // Callback wiring moved to getOrAddWidget for lazy compatibility

  for (int i = 0; i < 6; ++i) {
    panes.push_back(std::make_unique<PaneContainer>(
        0, 0, 0, 0, WidgetType::SOLAR, fontMgr));
    // Capture [this] — widgetFactory_ is a member, so this is safe post-ctor.
    panes.back()->setWidgetFactory(
        [this](WidgetType t) { return widgetFactory_(t); });
  }

  panes[0]->setRotation(appCfg.pane1Rotation, appCfg.rotationIntervalS,
                        appCfg.syncRotation);
  panes[1]->setRotation(appCfg.pane2Rotation, appCfg.rotationIntervalS,
                        appCfg.syncRotation);
  panes[2]->setRotation(appCfg.pane3Rotation, appCfg.rotationIntervalS,
                        appCfg.syncRotation);
  panes[3]->setRotation(appCfg.pane4Rotation, appCfg.rotationIntervalS,
                        appCfg.syncRotation);
  panes[4]->setRotation(appCfg.pane5Rotation, appCfg.rotationIntervalS,
                        appCfg.syncRotation);
  panes[5]->setRotation(appCfg.pane6Rotation, appCfg.rotationIntervalS,
                        appCfg.syncRotation);

  auto onPaneSelectionRequested = [&, allTypes](int paneIdx, int mx, int my) {
    (void)mx;
    (void)my;
    if (paneIdx == 4 || paneIdx == 5) {
      // Side-panel mode picker: choose among the 4 supported modes
      static const std::vector<WidgetType> sideAvail = {
          WidgetType::DE_INFO, WidgetType::DX_CLUSTER, WidgetType::ON_THE_AIR,
          WidgetType::LIVE_SPOTS};
      auto rot4 = panes[4]->getRotation();
      WidgetType cur4 = rot4.empty() ? WidgetType::DE_INFO : rot4[0];
      // Normalize: DX_INFO in pane5 → show DE_INFO as current selection
      std::vector<WidgetType> sideCurrent = {cur4};
      widgetSelector->show(
          4, sideAvail, sideCurrent, {},
          [this, &ctx](int /*idx*/, const std::vector<WidgetType> &sel) {
            if (!sel.empty())
              this->applySidePanelMode(sel[0], ctx);
          },
          /*singleSelect=*/true);
      return;
    }
    std::vector<WidgetType> available = allTypes;
    if (!ctx.bmeProvider->isAvailable()) {
      available.erase(std::remove_if(available.begin(), available.end(),
                                     [](WidgetType t) {
                                       return t == WidgetType::ENV_TEMP ||
                                              t == WidgetType::ENV_PRESSURE ||
                                              t == WidgetType::ENV_HUMIDITY ||
                                              t == WidgetType::ENV_DEWPOINT;
                                     }),
                      available.end());
    }
    if (paneIdx == 3) { // Pane 4 (top-right small pane)
      available = {WidgetType::NCDXF, WidgetType::SOLAR, WidgetType::DX_WEATHER,
                   WidgetType::DE_WEATHER};
    }
    std::vector<WidgetType> current = panes[paneIdx]->getRotation();
    std::vector<WidgetType> forbidden;
    for (int i = 0; i < 6; ++i) {
      if (i == paneIdx)
        continue;
      auto rot = panes[i]->getRotation();
      forbidden.insert(forbidden.end(), rot.begin(), rot.end());
    }
    widgetSelector->show(
        paneIdx, available, current, forbidden,
        [&ctx, this](int idx, const std::vector<WidgetType> &finalSelection) {
          panes[idx]->setRotation(finalSelection, ctx.appCfg.rotationIntervalS,
                                  ctx.appCfg.syncRotation);
          ctx.appCfg.pane1Rotation = panes[0]->getRotation();
          ctx.appCfg.pane2Rotation = panes[1]->getRotation();
          ctx.appCfg.pane3Rotation = panes[2]->getRotation();
          ctx.appCfg.pane4Rotation = panes[3]->getRotation();
          ctx.appCfg.pane5Rotation = panes[4]->getRotation();
          ctx.appCfg.pane6Rotation = panes[5]->getRotation();
          ctx.cfgMgr.save(ctx.appCfg);
        });
  };
  for (int i = 0; i < 6; ++i) {
    panes[i]->setOnSelectionRequested(onPaneSelectionRequested, i);
    panes[i]->setOnConfigRequested([&ctx](WidgetType type) {
      if (type == WidgetType::DX_CLUSTER) {
        ctx.activeSetup = AppContext::SetupMode::DXCluster;
      } else {
        // Most widgets handle internal setup or don't have one.
        // Don't open global setup generically.
      }
    });
  }

  mapArea = std::make_unique<MapWidget>(0, 0, 0, 0, texMgr, fontMgr, netManager,
                                        state, appCfg);
  mapArea->setOnConfigChanged([&ctx] { ctx.cfgMgr.save(ctx.appCfg); });
  mapArea->setSpotStore(spotStore);
  mapArea->setDXClusterStore(dxcStore);
  mapArea->setADIFStore(adifStore);
  mapArea->setMufRtProvider(mufRtProvider.get());
  mapArea->setCloudProvider(cloudProvider.get());
  mapArea->setBeaconProvider(beaconProvider.get());
  mapArea->setAuroraStore(auroraHistoryStore);
  mapArea->setAuroraMapStore(auroraMapStore);
  mapArea->setDrapStore(ctx.drapDataStore);
  mapArea->setIonosondeProvider(ionosondeProvider.get());
  mapArea->setSolarDataStore(ctx.solarStore.get());
  mapArea->setActivityStore(ctx.activityStore);

  std::vector<PaneContainer *> panePtrs;
  for (const auto &p : panes)
    panePtrs.push_back(p.get());
  mapArea->setPanes(panePtrs);

  // Wire predictor from DX_INFO widget to map and gimbal
  auto *dxSatWidget =
      dynamic_cast<DXSatPane *>(widgetFactory_(WidgetType::DX_INFO));
  if (dxSatWidget) {
    mapArea->setPredictor(dxSatWidget->activePredictor());
  }

  // NOAAProvider seems to populate solar data?  // Let's check main.cpp
  // earlier.

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
  layout.addWidget(Zone::SidePanel, panes[4].get());
  if (!appCfg.pane6Rotation.empty()) {
    layout.addWidget(Zone::SidePanel, panes[5].get());
  } else {
    panes[5]->onResize(0, 0, 0, 0); // hide when not in layout
  }
  layout.addWidget(Zone::MainStage, mapArea.get());

  // Apply Theme

  // Apply Theme to existing widgets
  for (auto const &[type, widget] : widgetPool) {
    if (widget) {
      widget->setTheme(appCfg.theme);
      widget->setMetric(appCfg.useMetric);
    }
  }
  timePanel->setTheme(appCfg.theme);
  timePanel->setMetric(appCfg.useMetric);
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

  {
    auto &memMon = MemoryMonitor::getInstance();
    LOG_I("Main", "System RAM: {:.0f} MB, low-memory mode: {}",
          memMon.getTotalRAM() / 1024.0 / 1024.0,
          memMon.isLowMemoryDevice() ? "YES" : "NO");
    if (memMon.isLowMemoryDevice()) {
      texMgr.setMaxCacheSize(15);
      LOG_I("Main", "Low-memory device: capping texture cache to 15");
      fontMgr.setTextCacheLimit(100);
      LOG_I("Main", "Low-memory device: capping font text cache to 100");
    } else {
      texMgr.setMaxCacheSize(50);
    }
  }

  // Populate widgets/eventWidgets vector
  widgets = {timePanel.get(),     panes[0].get(), panes[1].get(),
             panes[2].get(),      panes[3].get(), panes[4].get(),
             panes[5].get(),      mapArea.get(),  rssBanner.get(),
             widgetSelector.get()};

  eventWidgets = {widgetSelector.get(), timePanel.get(), panes[0].get(),
                  panes[1].get(),       panes[2].get(),  panes[3].get(),
                  panes[4].get(),       panes[5].get(),  mapArea.get(),
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

void DashboardContext::applySidePanelMode(WidgetType chosen, AppContext &ctx) {
  std::vector<WidgetType> p5 = {chosen};
  std::vector<WidgetType> p6 =
      (chosen == WidgetType::DE_INFO)
          ? std::vector<WidgetType>{WidgetType::DX_INFO}
          : std::vector<WidgetType>{};
  panes[4]->setRotation(p5, ctx.appCfg.rotationIntervalS,
                        ctx.appCfg.syncRotation);
  panes[5]->setRotation(p6, ctx.appCfg.rotationIntervalS,
                        ctx.appCfg.syncRotation);
  layout.removeWidget(panes[5].get());
  if (!p6.empty()) {
    layout.addWidget(Zone::SidePanel, panes[5].get());
  } else {
    panes[5]->onResize(0, 0, 0, 0); // hide phantom pane when not in layout
  }
  if (FIDELITY_MODE)
    layout.recalculate(LOGICAL_WIDTH, LOGICAL_HEIGHT, ctx.layLogicalOffX,
                       ctx.layLogicalOffY);
  else
    layout.recalculate(ctx.globalWinW, ctx.globalWinH);
  ctx.appCfg.pane5Rotation = p5;
  ctx.appCfg.pane6Rotation = p6;
  ctx.cfgMgr.save(ctx.appCfg);
}

void DashboardContext::update(AppContext &ctx) {
  auto &appCfg = ctx.appCfg;

  ctx.updateLayoutMetrics();

  Uint32 now = SDL_GetTicks();

  // If display is off (software sleep), skip updates and logic
  bool isPowerOn = ctx.displayPower->getPower();

  // Background refresh every 15 minutes, but only if power is on
  auto isWidgetActive = [&](WidgetType type) {
    for (auto &p : panes) {
      if (p->getActiveType() == type)
        return true;
    }
    return false;
  };
  auto isWidgetConfigured = [&](WidgetType type) -> bool {
    if (type == WidgetType::AURORA_GRAPH)
      return true;
    for (auto t : appCfg.pane1Rotation)
      if (t == type)
        return true;
    for (auto t : appCfg.pane2Rotation)
      if (t == type)
        return true;
    for (auto t : appCfg.pane3Rotation)
      if (t == type)
        return true;
    for (auto t : appCfg.pane4Rotation)
      if (t == type)
        return true;
    for (auto t : appCfg.pane5Rotation)
      if (t == type)
        return true;
    for (auto t : appCfg.pane6Rotation)
      if (t == type)
        return true;
    return false;
  };
  const bool isMaster = (appCfg.hubMode == HubMode::Master);

  if (isPowerOn && (now - lastFetchMs > 15 * 60 * 1000)) {
    // Map overlay helper: true if the MUF/RT propagation overlay is active
    // (mufRtProvider and ionosondeProvider only feed PropOverlayType::Muf)
    const bool mufOverlayActive = appCfg.propOverlay == PropOverlayType::Muf;

    // --- NOAA space-weather data ---
    // Gate per consumer group to avoid unnecessary sub-feed fetches.
    // Solar/SpaceWX panel consumers: KIndex, SFI, SN, Plasma, Mag, DST,
    //   XRay, ProtonFlux
    // Aurora/AuroraGraph consumers: Aurora sub-feed
    // DRAP panel consumers: DRAP sub-feed
    const bool needsNoaa = isMaster || isWidgetActive(WidgetType::SOLAR) ||
                           isWidgetActive(WidgetType::AURORA) ||
                           isWidgetConfigured(WidgetType::AURORA_GRAPH) ||
                           isWidgetActive(WidgetType::DRAP) ||
                           appCfg.propOverlay == PropOverlayType::Drap;
    if (needsNoaa)
      noaaProvider->fetch();

    // --- RSS news banner ---
    if (appCfg.rssEnabled)
      rssProvider->fetch();

    // --- Satellite manager ---
    // Feeds: SatPanel (in DXSatPane), EME planning tool, map track overlay
    if (isMaster || isWidgetActive(WidgetType::EME_TOOL) || appCfg.showSatTrack)
      satMgr->fetch();

    // --- Weather providers ---
    if (isMaster || isWidgetActive(WidgetType::DE_WEATHER))
      deWeatherProvider->fetch(ctx.state->deLocation.lat,
                               ctx.state->deLocation.lon);
    if (isMaster || isWidgetActive(WidgetType::DX_WEATHER))
      dxWeatherProvider->fetch(ctx.state->dxLocation.lat,
                               ctx.state->dxLocation.lon);

    // --- Context-sensitive fetches (existing gating preserved/unchanged) ---
    if (isMaster || isWidgetActive(WidgetType::LIVE_SPOTS) ||
        appCfg.propOverlay != PropOverlayType::None)
      spotProvider->fetch();

    if (isMaster || isWidgetActive(WidgetType::ON_THE_AIR) ||
        isWidgetActive(WidgetType::DX_PEDITIONS) || appCfg.ontaFilter != "Off")
      activityProvider->fetch();

    if (isMaster || isWidgetActive(WidgetType::BAND_CONDITIONS))
      bandProvider->update();

    if (isMaster || isWidgetActive(WidgetType::CONTESTS))
      contestProvider->fetch();

    if (isMaster || isWidgetActive(WidgetType::MOON))
      moonProvider->update(appCfg.lat, appCfg.lon);

    if (isWidgetActive(WidgetType::EME_TOOL)) {
      auto it = widgetPool.find(WidgetType::EME_TOOL);
      if (it != widgetPool.end()) {
        auto *eme = static_cast<EMEToolPanel *>(it->second.get());
        eme->setDeLocation(appCfg.lat, appCfg.lon);
        eme->setDxLocation(ctx.state->dxLocation.lat,
                           ctx.state->dxLocation.lon);
      }
    }

    if (isMaster || isWidgetActive(WidgetType::HISTORY_FLUX))
      historyProvider->fetchFlux();
    if (isMaster || isWidgetActive(WidgetType::HISTORY_SSN))
      historyProvider->fetchSSN();
    if (isMaster || isWidgetActive(WidgetType::HISTORY_KP))
      historyProvider->fetchKp();

    if (dstProvider && (isMaster || isWidgetActive(WidgetType::DST_INDEX)))
      dstProvider->fetch();

    // --- ADIF log viewer ---
    if (isMaster || isWidgetActive(WidgetType::ADIF))
      adifProvider->fetch(ctx.cfgMgr.configDir() / "logs.adif");

    // --- Propagation map overlays ---
    if (mufOverlayActive)
      mufRtProvider->update();
    if (mufOverlayActive)
      ionosondeProvider->update();

    // --- Asteroid widget + map pin ---
    if (isMaster || isWidgetActive(WidgetType::ASTEROID))
      asteroidProvider->update();

#ifndef __EMSCRIPTEN__
    if (ctx.updateChecker)
      ctx.updateChecker->fetch();
#endif
    lastFetchMs = now;
  }

  // --- DRAP fetch: immediate when overlay active and store empty (60s
  // cooldown) ---
  if (isPowerOn && appCfg.propOverlay == PropOverlayType::Drap &&
      !ctx.drapDataStore->get().valid &&
      (lastDrapFetchMs == 0 || now - lastDrapFetchMs > 60000u)) {
    noaaProvider->fetchDRAP();
    lastDrapFetchMs = now;
  }

  // --- Tropo fetch (immediate upon widget activation, internal cache 1hr) ---
  if (isWidgetActive(WidgetType::TROPO)) {
    tropoProvider->fetch(appCfg.lat, appCfg.lon);
  }

  // --- Lightning fetch (immediate upon widget activation, internal cache 2m)
  // ---
  if (isWidgetActive(WidgetType::LIGHTNING)) {
    lightningProvider->fetch(appCfg.lat, appCfg.lon);
  }

  // --- Meteor fetch (immediate upon widget activation, internal cache 10m) ---
  if (isWidgetActive(WidgetType::METEOR)) {
    meteorProvider->update(appCfg.lat, appCfg.lon);
  }

  // --- Ionosonde fetch (immediate upon widget activation, internal cache 15m)
  // ---
  if (isWidgetActive(WidgetType::IONOSONDE)) {
    ionosondeProvider->fetch(appCfg.lat, appCfg.lon);
  }

  // --- Solar Storm fetch (immediate upon widget activation, internal cache
  // 1m/5m) ---
  if (isWidgetActive(WidgetType::SOLAR_STORM)) {
    solarStormProvider->update();
  }

  if (isWidgetActive(WidgetType::GREYLINE_DX) &&
      now - lastGreylineFetchMs > 60000) {
    greylineDXProvider->update();
    lastGreylineFetchMs = now;
  }

  if (appCfg.propOverlay == PropOverlayType::Heatmap &&
      now - lastReachFetchMs > 5 * 60 * 1000) {
    reachProvider->fetch(appCfg.propBand, appCfg.propMode);
    lastReachFetchMs = now;
  }

#ifndef __EMSCRIPTEN__
  // Propagate update-available state to TimePanel.
  // Respect user's choice to skip a specific version.
  if (ctx.updateChecker && timePanel) {
    bool available = ctx.updateChecker->updateAvailable();
    if (available &&
        ctx.updateChecker->latestVersion() == appCfg.skippedVersion) {
      available = false;
    }
    timePanel->setUpdateInfo(available, ctx.updateChecker->latestVersion());
  }
#endif

#ifndef __EMSCRIPTEN__
  // Handle UpdateOverlay trigger from TimePanel version click
  if (timePanel && timePanel->isUpdateRequested()) {
    timePanel->clearUpdateRequest();
    if (!updateOverlay) {
      int w = 760;
      int h = 440;
      int x = (LOGICAL_WIDTH - w) / 2;
      int y = (LOGICAL_HEIGHT - h) / 2;
      updateOverlay = std::make_unique<UpdateOverlay>(x, y, w, h, fontMgr,
                                                      *ctx.updateChecker);
    }
  }

  if (updateOverlay) {
    updateOverlay->update();
  }
#endif

  SDL_Event event;
  while (SDL_PollEvent(&event)) {
    // Single modal/focus scan shared across all event-type branches below
    Widget *focusedWidget = nullptr;
    for (auto *w : eventWidgets) {
      if (w->isModalActive()) {
        focusedWidget = w;
        break;
      }
    }
    // Also check inline-configuring widgets (e.g. SatelliteSetup,
    // ReminderPanel)
    if (!focusedWidget) {
      for (auto *w : eventWidgets) {
        if (w->isConfiguring()) {
          focusedWidget = w;
          break;
        }
      }
    }

    if (event.type == SDL_MOUSEMOTION || event.type == SDL_MOUSEBUTTONDOWN ||
        event.type == SDL_MOUSEBUTTONUP || event.type == SDL_FINGERDOWN ||
        event.type == SDL_FINGERMOTION) {
      lastMouseMotionMs = SDL_GetTicks();
      if (!cursorVisible) {
        SDL_ShowCursor(SDL_ENABLE);
        cursorVisible = true;
      }
      // Wake up if screen is off
      if (!isPowerOn) {
        ctx.displayPower->setPower(true);
        isPowerOn = true;
      }
    }

    switch (event.type) {
    case SDL_QUIT:
      ctx.appRunning = false;
      return;
    case SDL_KEYDOWN: {
      bool consumed = false;
      if (focusedWidget) {
        consumed = focusedWidget->onKeyDown(event.key.keysym.sym,
                                            event.key.keysym.mod);
      } else {
        if (event.key.keysym.sym == SDLK_k) {
          ctx.showActionHighlights = !ctx.showActionHighlights;
          consumed = true;
        }
        if (!consumed) {
          for (auto *w : eventWidgets) {
            if (w->onKeyDown(event.key.keysym.sym, event.key.keysym.mod)) {
              consumed = true;
              break;
            }
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
    case SDL_TEXTINPUT: {
      if (focusedWidget) {
        focusedWidget->onTextInput(event.text.text);
      } else {
        for (auto *w : eventWidgets) {
          if (w->onTextInput(event.text.text)) {
            break;
          }
        }
      }
      break;
    }
    case SDL_FINGERDOWN:
      if (appCfg.preventSleep)
        preventRPiSleep(true, ctx.displayPower.get());
      [[fallthrough]];
    case SDL_MOUSEBUTTONDOWN: {
      int smx = event.button.x, smy = event.button.y;
      if (FIDELITY_MODE) {
        float pixX = event.button.x * static_cast<float>(ctx.globalDrawW) /
                     ctx.globalWinW;
        float pixY = event.button.y * static_cast<float>(ctx.globalDrawH) /
                     ctx.globalWinH;
        smx = static_cast<int>(pixX / ctx.layScale);
        smy = static_cast<int>(pixY / ctx.layScale);
      }
      if (focusedWidget)
        focusedWidget->onMouseDown(smx, smy, SDL_GetModState(),
                                   event.button.clicks);
      else
        for (auto *w : eventWidgets)
          if (w->onMouseDown(smx, smy, SDL_GetModState(), event.button.clicks))
            break;
    } break;
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
#ifndef __EMSCRIPTEN__
    case SDL_MOUSEWHEEL:
      if (updateOverlay) {
        updateOverlay->onMouseWheel(event.wheel.y);
      }
      break;
#endif
#ifndef __EMSCRIPTEN__
    case SDL_MOUSEBUTTONUP:
      if (updateOverlay) {
        int smx = event.button.x, smy = event.button.y;
        if (FIDELITY_MODE) {
          float pixX = event.button.x * static_cast<float>(ctx.globalDrawW) /
                       ctx.globalWinW;
          float pixY = event.button.y * static_cast<float>(ctx.globalDrawH) /
                       ctx.globalWinH;
          smx = static_cast<int>(pixX / ctx.layScale);
          smy = static_cast<int>(pixY / ctx.layScale);
        }
        if (updateOverlay->onMouseUp(smx, smy, SDL_GetModState(),
                                     event.button.clicks)) {
          auto res = updateOverlay->getResult();
          if (res == UpdateOverlay::Result::Skip) {
            ctx.appCfg.skippedVersion = ctx.updateChecker->latestVersion();
            ctx.cfgMgr.save(ctx.appCfg);
            updateOverlay.reset();
          } else if (res == UpdateOverlay::Result::NotNow) {
            updateOverlay.reset();
          } else if (res == UpdateOverlay::Result::Update) {
            updateOverlay.reset();
            ctx.activeSetup = AppContext::SetupMode::Main;
            ctx.startOnUpdateTab = true;
          }
          continue; // Event consumed, don't pass to other widgets
        }
      }
      break;
#endif
    default:
      // Handle custom application events
      if (event.type >= AE_BASE_EVENT) {
        switch (event.type - AE_BASE_EVENT) {
        case AE_SATELLITE_TRACK_READY: {
          auto *track =
              static_cast<std::vector<GroundTrackPoint> *>(event.user.data1);
          if (track && ctx.dashboard && ctx.dashboard->mapArea) {
            ctx.dashboard->mapArea->onSatTrackReady(*track);
          }
          delete track; // Free the memory allocated by the worker thread
          break;
        }
        case AE_RSS_DATA_READY: {
          int feed_idx = event.user.code;
          auto *headlines =
              static_cast<std::vector<std::string> *>(event.user.data1);
          if (headlines && feed_idx >= 0 && feed_idx < 3) {
            rssHeadlines[feed_idx] = std::move(*headlines);
            rssDataDirty = true;
          }
          delete headlines;
          break;
        }
        case AE_SOLAR_DATA_READY: {
          auto *update = static_cast<SolarData *>(event.user.data1);
          if (update && ctx.solarStore) {
            auto data = ctx.solarStore->get();
            switch (static_cast<NOAAProvider::UpdateType>(event.user.code)) {
            case NOAAProvider::UpdateType::KIndex:
              data.k_index = update->k_index;
              data.a_index = update->a_index;
              data.noaa_g_scale = update->noaa_g_scale;
              data.last_updated = update->last_updated;
              data.valid = true;
              break;
            case NOAAProvider::UpdateType::SFI:
              data.sfi = update->sfi;
              data.valid = true;
              break;
            case NOAAProvider::UpdateType::SN:
              data.sunspot_number = update->sunspot_number;
              data.valid = true;
              break;
            case NOAAProvider::UpdateType::Plasma:
              data.solar_wind_speed = update->solar_wind_speed;
              data.solar_wind_density = update->solar_wind_density;
              break;
            case NOAAProvider::UpdateType::Mag:
              data.bt = update->bt;
              data.bz = update->bz;
              break;
            case NOAAProvider::UpdateType::DST:
              data.dst = update->dst;
              break;
            case NOAAProvider::UpdateType::Aurora:
              data.aurora = update->aurora;
              break;
            case NOAAProvider::UpdateType::DRAP:
              data.drap = update->drap;
              break;
            case NOAAProvider::UpdateType::XRay:
              data.xray_flux = update->xray_flux;
              data.noaa_r_scale = update->noaa_r_scale;
              break;
            case NOAAProvider::UpdateType::ProtonFlux:
              data.proton_flux = update->proton_flux;
              data.noaa_s_scale = update->noaa_s_scale;
              break;
            }
            ctx.solarStore->set(data);
          }
          delete update;
          break;
        }
        case AE_AURORA_DATA_READY: {
          float percent = *(static_cast<float *>(event.user.data1));
          if (ctx.auroraHistoryStore) {
            ctx.auroraHistoryStore->addPoint(percent);
          }
          delete static_cast<float *>(event.user.data1);
          break;
        }
        case AE_ACTIVITY_DATA_READY: {
          auto *update = static_cast<ActivityData *>(event.user.data1);
          if (update && ctx.activityStore) {
            auto data = ctx.activityStore->get();
            switch (
                static_cast<ActivityProvider::UpdateType>(event.user.code)) {
            case ActivityProvider::UpdateType::DXPeds:
              data.dxpeds = std::move(update->dxpeds);
              break;
            case ActivityProvider::UpdateType::POTA: {
              auto it = std::remove_if(
                  data.ontaSpots.begin(), data.ontaSpots.end(),
                  [](const ONTASpot &s) { return s.program == "POTA"; });
              data.ontaSpots.erase(it, data.ontaSpots.end());
              data.ontaSpots.insert(data.ontaSpots.end(),
                                    update->ontaSpots.begin(),
                                    update->ontaSpots.end());
              break;
            }
            case ActivityProvider::UpdateType::SOTA: {
              auto it = std::remove_if(
                  data.ontaSpots.begin(), data.ontaSpots.end(),
                  [](const ONTASpot &s) { return s.program == "SOTA"; });
              data.ontaSpots.erase(it, data.ontaSpots.end());
              data.ontaSpots.insert(data.ontaSpots.end(),
                                    update->ontaSpots.begin(),
                                    update->ontaSpots.end());
              break;
            }
            }
            data.lastUpdated = std::chrono::system_clock::now();
            data.valid = true;
            ctx.activityStore->set(data);
          }
          delete update;
          break;
        }
        case AE_WEATHER_DATA_READY: {
          auto *update = static_cast<WeatherData *>(event.user.data1);
          int id = event.user.code;
          if (update) {
            if (id == 0 && ctx.deWeatherStore) {
              ctx.deWeatherStore->update(*update);
            } else if (id == 1 && ctx.dxWeatherStore) {
              ctx.dxWeatherStore->update(*update);
            }
          }
          delete update;
          break;
        }
        case AE_CONTEST_DATA_READY: {
          auto *update = static_cast<ContestData *>(event.user.data1);
          if (update && ctx.contestStore) {
            ctx.contestStore->update(*update);
          }
          delete update;
          break;
        }
        case AE_HISTORY_DATA_READY: {
          auto *update = static_cast<HistorySeries *>(event.user.data1);
          if (update && ctx.historyStore) {
            ctx.historyStore->update(update->name, *update);
          }
          delete update;
          break;
        }
        case AE_PROP_DATA_READY: {
          auto *grid = static_cast<std::vector<float> *>(event.user.data1);
          if (grid && ctx.dashboard && ctx.dashboard->mapArea) {
            ctx.dashboard->mapArea->onPropDataReady(
                static_cast<PropOverlayType>(event.user.code), *grid);
          }
          delete grid;
          break;
        }
        case AE_ASTEROID_ELEMENTS_READY: {
          auto *payload =
              static_cast<std::pair<std::string, OrbitalElements> *>(
                  event.user.data1);
          if (payload && ctx.dashboard && ctx.dashboard->mapArea) {
            ctx.dashboard->mapArea->onAsteroidElementsReady(payload->first,
                                                            payload->second);
          }
          delete payload;
          break;
        }
        case AE_ALERTS_DATA_READY: {
          auto *update = static_cast<AlertsData *>(event.user.data1);
          if (update && ctx.alertsStore)
            ctx.alertsStore->update(*update);
          delete update;
          break;
        }
        case AE_FORECAST_DATA_READY: {
          auto *update = static_cast<ForecastData *>(event.user.data1);
          if (update && ctx.forecastStore)
            ctx.forecastStore->update(*update);
          delete update;
          break;
        }
        case AE_REPEATER_DATA_READY: {
          auto *update = static_cast<RepeaterData *>(event.user.data1);
          if (update && ctx.repeaterStore)
            ctx.repeaterStore->update(*update);
          delete update;
          break;
        }
        case AE_HURRICANE_DATA_READY: {
          auto *update = static_cast<HurricaneData *>(event.user.data1);
          if (update && ctx.hurricaneStore)
            ctx.hurricaneStore->update(*update);
          delete update;
          break;
        }
        case AE_MARINE_DATA_READY: {
          auto *update = static_cast<MarineData *>(event.user.data1);
          if (update && ctx.marineStore)
            ctx.marineStore->update(*update);
          delete update;
          break;
        }
        case AE_WINLINK_DATA_READY: {
          auto *update = static_cast<WinlinkData *>(event.user.data1);
          if (update && ctx.winlinkStore)
            ctx.winlinkStore->update(*update);
          delete update;
          break;
        }
        case AE_TOUCH: {
          int mx =
              static_cast<int>(reinterpret_cast<intptr_t>(event.user.data1));
          int my =
              static_cast<int>(reinterpret_cast<intptr_t>(event.user.data2));
          // If setup active, send to setup widget
          if (ctx.activeSetup != AppContext::SetupMode::None &&
              ctx.setupWidget) {
            ctx.setupWidget->onMouseDown(mx, my, 0, 1);
            ctx.setupWidget->onMouseUp(mx, my, 0, 1);
          } else {
            // Check Map Modal first (MapViewMenu) via mapArea
            if (ctx.dashboard->mapArea->isModalActive()) {
              ctx.dashboard->mapArea->onMouseUp(mx, my, 0, 1);
            } else {
              // Dashboard widgets (normal mode)
              for (auto *w : ctx.dashboard->eventWidgets)
                if (w->onMouseUp(mx, my, 0, 1))
                  break;
            }
          }
          break;
        }
        case AE_WHEEL: {
          int dy =
              static_cast<int>(reinterpret_cast<intptr_t>(event.user.data1));
          if (ctx.activeSetup != AppContext::SetupMode::None &&
              ctx.setupWidget) {
            ctx.setupWidget->onMouseWheel(dy);
          } else {
            // Check Map Modal first (MapViewMenu) via mapArea
            if (ctx.dashboard->mapArea->isModalActive()) {
              ctx.dashboard->mapArea->onMouseWheel(dy);
            } else {
              // Dashboard widgets (e.g. scrollable top bar panes)
              for (auto *w : ctx.dashboard->eventWidgets)
                if (w->onMouseWheel(dy))
                  break;
            }
          }
          break;
        }
        }
      }
      break;
    }

    // Dispatch other events
    if (!updateOverlay &&
        (event.type == SDL_MOUSEMOTION || event.type == SDL_MOUSEBUTTONUP ||
         event.type == SDL_MOUSEWHEEL)) {
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
        if (focusedWidget)
          focusedWidget->onMouseMove(mx, my);
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
          if (focusedWidget)
            focusedWidget->onMouseUp(mx, my, SDL_GetModState(),
                                     event.button.clicks);
          else
            for (auto *w : eventWidgets)
              if (w->onMouseUp(mx, my, SDL_GetModState(), event.button.clicks))
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

  // After event loop, process any aggregated data
  if (rssDataDirty) {
    RSSData data;
    for (int i = 0; i < 3; ++i) {
      data.headlines.insert(data.headlines.end(), rssHeadlines[i].begin(),
                            rssHeadlines[i].end());
    }
    if (data.headlines.empty()) {
      data.headlines = {
          "HamClock-Next: A modern amateur radio dashboard",
          "Welcome to HamClock -- real-time propagation and space weather",
      };
    }
    data.lastUpdated = std::chrono::system_clock::now();
    data.valid = true;
    ctx.rssStore->set(data);
    rssDataDirty = false;
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

  // Sync predictor from DXSatPane if it exists in the pool
  auto *dxSatWidget =
      dynamic_cast<DXSatPane *>(widgetPool[WidgetType::DX_INFO].get());
  mapArea->setPredictor(dxSatWidget ? dxSatWidget->activePredictor() : nullptr);
  auto *gimbal =
      dynamic_cast<GimbalPanel *>(widgetPool[WidgetType::GIMBAL].get());
  if (gimbal) {
    gimbal->setPredictor(dxSatWidget ? dxSatWidget->activePredictor()
                                     : nullptr);
    gimbal->setObserver(appCfg.lat, appCfg.lon);
  }

  mapArea->setAsteroidProvider(asteroidProvider.get());

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

  // 60-second RSS/VRAM heartbeat — helps diagnose memory growth on RPi
  if (now - lastMemLogMs > 60000) {
    MemoryMonitor::getInstance().logStats("heartbeat");
    lastMemLogMs = now;
  }

  if (isPowerOn) {
    for (auto *w : widgets)
      w->update();
    // satMgr->update(); // Deprecated: Auto-tracking handled by RotatorService
    ctx.brightnessMgr->update();
  }
}

void DashboardContext::render(AppContext &ctx) {
  SDL_SetRenderDrawColor(ctx.renderer, 0, 0, 0, 255);
  SDL_RenderClear(ctx.renderer);

  if (FIDELITY_MODE) {
    SDL_RenderSetViewport(ctx.renderer, nullptr);
    SDL_RenderSetScale(ctx.renderer, ctx.layScale, ctx.layScale);
  }

  if (!ctx.displayPower->getPower()) {
    return; // Stay black
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
    if (FIDELITY_MODE)
      SDL_RenderSetScale(ctx.renderer, 1.0f, 1.0f);

    SDL_SetRenderDrawBlendMode(ctx.renderer, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(ctx.renderer, 0, 0, 0, 150);
    // Draw over entire window by bypassing logical scaling
    int dw, dh;
    SDL_GetRendererOutputSize(ctx.renderer, &dw, &dh);
    SDL_Rect full = {0, 0, dw, dh};
    SDL_RenderFillRect(ctx.renderer, &full);

    if (FIDELITY_MODE)
      SDL_RenderSetScale(ctx.renderer, ctx.layScale, ctx.layScale);

    activeModal->renderModal(ctx.renderer);
  }

  if (updateOverlay) {
    updateOverlay->render(ctx.renderer);
  }

  if (ctx.showActionHighlights) {
    int mx, my;
    SDL_GetMouseState(&mx, &my);
    if (FIDELITY_MODE) {
      float pixX = mx * static_cast<float>(ctx.globalDrawW) / ctx.globalWinW;
      float pixY = my * static_cast<float>(ctx.globalDrawH) / ctx.globalWinH;
      mx = static_cast<int>(pixX / ctx.layScale);
      my = static_cast<int>(pixY / ctx.layScale);
    }

    SDL_SetRenderDrawBlendMode(ctx.renderer, SDL_BLENDMODE_BLEND);
    std::string hoverTooltip;

    for (auto *w : widgets) {
      auto actions = w->getActions();
      for (const auto &action : actions) {
        SDL_Rect r = w->getActionRect(action);
        if (r.w > 0 && r.h > 0) {
          bool hovered =
              (mx >= r.x && mx < r.x + r.w && my >= r.y && my < r.y + r.h);
          if (hovered) {
            SDL_SetRenderDrawColor(ctx.renderer, 255, 255, 0,
                                   120); // yellow highlight
            hoverTooltip = action;
          } else {
            SDL_SetRenderDrawColor(ctx.renderer, 0, 255, 255,
                                   60); // cyan highlight
          }
          SDL_RenderFillRect(ctx.renderer, &r);
          SDL_SetRenderDrawColor(ctx.renderer, 255, 255, 255, 100);
          SDL_RenderDrawRect(ctx.renderer, &r);
        }
      }
    }

    if (!hoverTooltip.empty()) {
      auto *cat = &fontCatalog;
      if (cat) {
        int tw, th;
        cat->renderText(ctx.renderer, hoverTooltip, {255, 255, 255, 255},
                        FontStyle::Micro, &tw, &th);
        int pad = 4;
        SDL_Rect box = {mx + 12, my + 12, tw + pad * 2, th + pad * 2};
        // Keep tooltip on screen
        if (box.x + box.w > LOGICAL_WIDTH)
          box.x = mx - box.w - 4;
        if (box.y + box.h > LOGICAL_HEIGHT)
          box.y = my - box.h - 4;

        SDL_SetRenderDrawColor(ctx.renderer, 20, 20, 20, 220);
        SDL_RenderFillRect(ctx.renderer, &box);
        SDL_SetRenderDrawColor(ctx.renderer, 255, 255, 255, 180);
        SDL_RenderDrawRect(ctx.renderer, &box);
        cat->drawText(ctx.renderer, hoverTooltip, box.x + pad, box.y + pad,
                      {255, 255, 255, 255}, FontStyle::Micro);
      }
    }
  }

#ifndef __EMSCRIPTEN__
  if (ctx.frameCapture)
    ctx.frameCapture->capture(ctx.renderer);
#endif
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
                                               *ctx.brightnessMgr);
        s->setConfig(ctx.appCfg);
        if (ctx.startOnUpdateTab) {
          s->setStartTab(SetupScreen::Tab::Update);
          ctx.startOnUpdateTab = false;
        } else if (ctx.startOnServicesTab) {
          s->setStartTab(SetupScreen::Tab::Services);
          ctx.startOnServicesTab = false;
        }
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
        else if (event.type == SDL_MOUSEBUTTONDOWN) {
          int smx = event.button.x, smy = event.button.y;
          if (FIDELITY_MODE) {
            float pixX = event.button.x * static_cast<float>(ctx.globalDrawW) /
                         ctx.globalWinW;
            float pixY = event.button.y * static_cast<float>(ctx.globalDrawH) /
                         ctx.globalWinH;
            smx = static_cast<int>(pixX / ctx.layScale);
            smy = static_cast<int>(pixY / ctx.layScale);
          }
          ctx.setupWidget->onMouseDown(smx, smy, SDL_GetModState(),
                                       event.button.clicks);
        } else if (event.type == SDL_MOUSEBUTTONUP) {
          int smx = event.button.x, smy = event.button.y;
          if (FIDELITY_MODE) {
            float pixX = event.button.x * static_cast<float>(ctx.globalDrawW) /
                         ctx.globalWinW;
            float pixY = event.button.y * static_cast<float>(ctx.globalDrawH) /
                         ctx.globalWinH;
            smx = static_cast<int>(pixX / ctx.layScale);
            smy = static_cast<int>(pixY / ctx.layScale);
          }
          ctx.setupWidget->onMouseUp(smx, smy, SDL_GetModState(),
                                     event.button.clicks);
        } else if (event.type == SDL_MOUSEMOTION) {
          int smx = event.motion.x, smy = event.motion.y;
          if (FIDELITY_MODE) {
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
    } else if (ctx.activeSetup == AppContext::SetupMode::DXCluster) {
      if (static_cast<DXClusterSetup *>(ctx.setupWidget.get())->isComplete())
        setupDone = true;
    }

    if (setupDone) {
      SDL_StopTextInput();
      // Save logic
      if (ctx.activeSetup == AppContext::SetupMode::Main) {
        auto *s = static_cast<SetupScreen *>(ctx.setupWidget.get());
        if (!s->wasCancelled()) {
          ctx.appCfg = s->getConfig();
          // Sync watchlist store from updated config
          auto oldW = ctx.watchlistStore->getAll();
          for (const auto &c : oldW)
            ctx.watchlistStore->remove(c);
          for (const auto &c : ctx.appCfg.watchlist)
            ctx.watchlistStore->add(c);
        }
      } else if (ctx.activeSetup == AppContext::SetupMode::DXCluster) {
        if (static_cast<DXClusterSetup *>(ctx.setupWidget.get())->isSaved())
          ctx.appCfg = static_cast<DXClusterSetup *>(ctx.setupWidget.get())
                           ->updateConfig(ctx.appCfg);
      }
      ctx.cfgMgr.save(ctx.appCfg);
      ctx.setupWidget.reset();
      ctx.setupFontMgr.reset();
      ctx.setupCatalog.reset();
      ctx.activeSetup = AppContext::SetupMode::None;
      // Update state
      ctx.state->deCallsign = ctx.appCfg.callsign;
      ctx.state->deGrid = ctx.appCfg.grid;
      ctx.state->deLocation = {ctx.appCfg.lat, ctx.appCfg.lon};

      // Re-apply side-panel pane rotations and layout immediately
      if (ctx.dashboard) {
        ctx.dashboard->applySidePanelMode(ctx.appCfg.pane5Rotation.empty()
                                              ? WidgetType::DE_INFO
                                              : ctx.appCfg.pane5Rotation[0],
                                          ctx);
      }
    }

  } else {
    // Dashboard
    if (!ctx.dashboard) {
      ctx.dashboard = std::make_unique<DashboardContext>(ctx);
      if (ctx.webServer) {
        ctx.webServer->setSatelliteManager(ctx.dashboard->satMgr.get());
        ctx.webServer->setRotatorService(ctx.dashboard->rotatorService.get());
        ctx.webServer->setPanes(&ctx.dashboard->panes);
        ctx.webServer->setWeatherStore(ctx.deWeatherStore);
        ctx.webServer->setBrightnessManager(ctx.brightnessMgr);
      }
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
      ctx.netManager->setHubConfig(ctx.appCfg.hubMode, ctx.appCfg.hubIp,
                                   ctx.appCfg.hubPort);
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
        ctx.dashboard->widgetSelector->setTheme(ctx.appCfg.theme);
      }
      LOG_I("Main", "Config reloaded from remote API: callsign={}",
            ctx.appCfg.callsign);
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
          p.jumpToType(static_cast<WidgetType>(rwidget));
      };
      if (rpane >= 0 && rpane < (int)ctx.dashboard->panes.size()) {
        applyPane(*ctx.dashboard->panes[rpane]);
      } else {
        for (auto &p : ctx.dashboard->panes)
          applyPane(*p);
      }
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
