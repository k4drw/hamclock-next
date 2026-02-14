#include "core/AuroraHistoryStore.h"
#include "core/CitiesManager.h"
#include "core/ConfigManager.h"
#include "core/DXClusterData.h"
#include "core/DatabaseManager.h"
#include "core/HamClockState.h"
#include "core/LiveSpotData.h"
#include "core/PrefixManager.h"
#include "core/RSSData.h"
#include "core/SatelliteManager.h"
#include "core/SolarData.h"
#ifdef ENABLE_DEBUG_API
#include "core/UIRegistry.h"
#endif
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
#include "services/HistoryProvider.h"
#include "services/LiveSpotProvider.h"
#include "services/MoonProvider.h"
#include "services/NOAAProvider.h"
#include "services/RSSProvider.h"
#include "services/SDOProvider.h"
#include "services/SantaProvider.h"
#include "services/WeatherProvider.h"
#include "ui/ADIFPanel.h"
#include "ui/ActivityPanels.h"
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

#include "core/Logger.h"
#include <SDL.h>
#include <SDL_image.h>
#include <SDL_ttf.h>
#include <curl/curl.h>
#include <nlohmann/json.hpp>

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <memory>
#ifdef __linux__
#include <unistd.h>
#endif
#include <vector>

static constexpr int INITIAL_WIDTH = 800;
static constexpr int INITIAL_HEIGHT = 480;
static constexpr int LOGICAL_WIDTH = 800;
static constexpr int LOGICAL_HEIGHT = 480;
static constexpr int FRAME_DELAY_MS =
    33; // ~30 FPS is plenty and saves CPU on Pi
static constexpr bool FIDELITY_MODE = true;

static constexpr int FONT_SIZE = 24;

int main(int argc, char *argv[]) {
#ifndef _WIN32
  SDL_SetMainReady();
#endif
  curl_global_init(CURL_GLOBAL_ALL);

  // --- Load config via ConfigManager ---
  ConfigManager cfgMgr;
  cfgMgr.init(); // Resolve paths for logging and config

  Log::init(cfgMgr.configDir().string());
  if (!DatabaseManager::instance().init(cfgMgr.configDir() / "hamclock.db")) {
    LOG_E("Main", "Failed to initialize database");
  }
  LOG_INFO("Starting HamClock-Next v{}...", HAMCLOCK_VERSION);

  bool forceFullscreen = false;
  bool forceSoftware = false;
  for (int i = 1; i < argc; ++i) {
    std::string arg = argv[i];
    if (arg == "-f" || arg == "--fullscreen") {
      forceFullscreen = true;
    } else if (arg == "-s" || arg == "--software") {
      forceSoftware = true;
    } else if (arg == "-h" || arg == "--help") {
      std::printf("Usage: hamclock-next [options]\n");
      std::printf("Options:\n");
      std::printf("  -f, --fullscreen  Force fullscreen mode\n");
      std::printf(
          "  -s, --software    Force software rendering (no OpenGL/MSAA)\n");
      std::printf("  -h, --help        Show this help message\n");
      return EXIT_SUCCESS;
    }
  }

  AppConfig appCfg;
  enum class SetupMode { None, Main, DXCluster };
  SetupMode activeSetup = SetupMode::None;

  if (cfgMgr.configPath().empty()) {
    std::fprintf(stderr, "Warning: could not resolve config path\n");
    activeSetup = SetupMode::Main;
  } else if (!cfgMgr.load(appCfg)) {
    // No config at ~/.config/hamclock/ — needs setup
    activeSetup = SetupMode::Main;
  }

  // Handle screen blanking prevention (persistent setting)
  bool preventSleep = appCfg.preventSleep;

  // --- Init SDL2 ---
  int numDrivers = SDL_GetNumVideoDrivers();
  std::fprintf(stderr, "SDL Video Drivers available: ");
  for (int i = 0; i < numDrivers; ++i) {
    std::fprintf(stderr, "%s%s", SDL_GetVideoDriver(i),
                 (i == numDrivers - 1) ? "" : ", ");
  }
  std::fprintf(stderr, "\n");

  const char *envDriver = std::getenv("SDL_VIDEODRIVER");
  if (envDriver) {
    std::fprintf(stderr, "Requested SDL_VIDEODRIVER via env: %s\n", envDriver);
  }

  if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER) != 0) {
    LOG_ERROR("SDL_Init failed: {}", SDL_GetError());
    if (numDrivers == 0) {
      LOG_ERROR("Error: No video drivers compiled into SDL2.");
    } else {
#ifdef __linux__
      if (access("/dev/dri/card0", F_OK) != 0) {
        LOG_ERROR("Error: /dev/dri/card0 not found. KMSDRM requires the modern "
                  "DRM/KMS driver.");
        LOG_ERROR("Hint: Enable 'dtoverlay=vc4-kms-v3d' in /boot/config.txt "
                  "and reboot.");
      } else {
        LOG_ERROR("Hint: If running from console, ensure you have permission "
                  "to /dev/dri/card0");
        LOG_ERROR("      Try: sudo usermod -aG video,render $USER");
      }
#else
      LOG_ERROR("Hint: Check graphics drivers installation.");
#endif
    }
    return EXIT_FAILURE;
  }

  // Init SDL_image
  int imgFlags = IMG_INIT_PNG | IMG_INIT_JPG;
  if (!(IMG_Init(imgFlags) & imgFlags)) {
    LOG_ERROR("IMG_Init failed: {}", IMG_GetError());
    // Continue anyway, basic BMP support is built-in
  }

  const char *activeDriver = SDL_GetCurrentVideoDriver();
  if (activeDriver) {
    std::fprintf(stderr, "SDL Video Driver in use: %s\n", activeDriver);
  }

  if (preventSleep) {
    SDL_DisableScreenSaver();
    LOG_I("Main", "Screen saver disabled (kiosk mode)");
  } else {
    SDL_EnableScreenSaver();
    LOG_I("Main", "Screen saver enabled");
  }

  if (forceSoftware) {
    SDL_SetHint(SDL_HINT_RENDER_DRIVER, "software");
  } else {
    SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "best");
    // MSAA can cause "Couldn't find matching EGL config" on RPi and some other
    // platforms. Disabling by default for maximum compatibility.
    SDL_GL_SetAttribute(SDL_GL_MULTISAMPLEBUFFERS, 0);
    SDL_GL_SetAttribute(SDL_GL_MULTISAMPLESAMPLES, 0);
#if defined(__arm__) || defined(__aarch64__)
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_ES);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 2);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 0);
#endif
    // Disable Depth/Stencil to help EGL config finding on limited VRAM/Drivers
    SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 0);
    SDL_GL_SetAttribute(SDL_GL_STENCIL_SIZE, 0);

    // Explicitly request 8-bit color depth to help Mali drivers match a config
    SDL_GL_SetAttribute(SDL_GL_RED_SIZE, 8);
    SDL_GL_SetAttribute(SDL_GL_GREEN_SIZE, 8);
    SDL_GL_SetAttribute(SDL_GL_BLUE_SIZE, 8);
    SDL_GL_SetAttribute(SDL_GL_ALPHA_SIZE, 0);
  }

  Uint32 windowFlags = SDL_WINDOW_SHOWN | SDL_WINDOW_RESIZABLE;
  if (!forceSoftware) {
    windowFlags |= SDL_WINDOW_OPENGL;
  }
  int winW = INITIAL_WIDTH;
  int winH = INITIAL_HEIGHT;

  if (forceFullscreen) {
    bool isKMS = (activeDriver && std::string(activeDriver).find("KMSDRM") !=
                                      std::string::npos);
    windowFlags |= (isKMS || forceSoftware) ? SDL_WINDOW_FULLSCREEN
                                            : SDL_WINDOW_FULLSCREEN_DESKTOP;

    // Try to get native display resolution for KMSDRM/Fullscreen
    SDL_DisplayMode dm;
    if (SDL_GetCurrentDisplayMode(0, &dm) == 0) {
      winW = dm.w;
      winH = dm.h;
      std::fprintf(stderr, "Native Display Mode: %dx%d\n", winW, winH);
    }
  }

  SDL_Window *window =
      SDL_CreateWindow("HamClock-Next", SDL_WINDOWPOS_CENTERED,
                       SDL_WINDOWPOS_CENTERED, winW, winH, windowFlags);

  if (!window) {
    if (!forceSoftware) {
      std::fprintf(stderr,
                   "SDL_CreateWindow failed with HW accel: %s. Retrying with "
                   "Software Renderer...\n",
                   SDL_GetError());

      // Fallback: Clear OpenGL flag, force software hint
      windowFlags &= ~SDL_WINDOW_OPENGL;
      SDL_SetHint(SDL_HINT_RENDER_DRIVER, "software");

      window =
          SDL_CreateWindow("HamClock-Next", SDL_WINDOWPOS_CENTERED,
                           SDL_WINDOWPOS_CENTERED, winW, winH, windowFlags);
      if (window) {
        forceSoftware = true; // Update state so renderer creation knows
        std::fprintf(stderr,
                     "Success: Fallback to software rendering worked.\n");
      }
    }
  }

  if (!window) {
    std::fprintf(stderr, "SDL_CreateWindow failed: %s\n", SDL_GetError());
    if (activeDriver &&
        std::string(activeDriver).find("KMSDRM") != std::string::npos) {
      std::fprintf(stderr,
                   "Warning: KMSDRM failed to create a window. Check if "
                   "another process (X11/Wayland) is already using the GPU.\n");
    }
    SDL_Quit();
    return EXIT_FAILURE;
  }

  // Set window icon
  {
    SDL_RWops *rw = SDL_RWFromMem((void *)icon_png, sizeof(icon_png));
    SDL_Surface *iconSurface = IMG_Load_RW(rw, 1);
    if (iconSurface) {
      SDL_SetWindowIcon(window, iconSurface);
      SDL_FreeSurface(iconSurface);
    }
  }

  Uint32 rendererFlags = SDL_RENDERER_PRESENTVSYNC;
  if (!forceSoftware) {
    rendererFlags |= SDL_RENDERER_ACCELERATED;
  } else {
    rendererFlags |= SDL_RENDERER_SOFTWARE;
  }

  SDL_Renderer *renderer = SDL_CreateRenderer(window, -1, rendererFlags);
  if (!renderer) {
    std::fprintf(stderr, "SDL_CreateRenderer failed: %s\n", SDL_GetError());
    SDL_DestroyWindow(window);
    SDL_Quit();
    return EXIT_FAILURE;
  }

  // Debug Output: Resolution Info
  int debugWinW, debugWinH;
  SDL_GetWindowSize(window, &debugWinW, &debugWinH);
  int renW, renH;
  SDL_GetRendererOutputSize(renderer, &renW, &renH);
  std::fprintf(stderr, "Display Info:\n");
  std::fprintf(stderr, "  Window Size: %dx%d\n", debugWinW, debugWinH);
  std::fprintf(stderr, "  Renderer Output: %dx%d\n", renW, renH);

  SDL_RendererInfo info;
  if (SDL_GetRendererInfo(renderer, &info) == 0) {
    std::fprintf(stderr, "  Renderer Name: %s\n", info.name);
    std::fprintf(stderr, "  Max Texture Size: %dx%d\n", info.max_texture_width,
                 info.max_texture_height);
  }

  // --- Dashboard Loop ---
  Uint32 lastFetchMs = SDL_GetTicks();
  enum class AlignMode { Center, Left, Right };
  AlignMode alignMode = AlignMode::Center;

  // --- Layout Metrics State ---
  // We calculate these manually to ensure Rendering and Input are perfectly
  // aligned. Instead of using SDL Viewports (which are buggy/flaky on some
  // remote X11/GL setups), we effectively shift the "Logical Origin" of the
  // layout itself.
  float layScale = 1.0f;

  // These are now LOGICAL offsets (0..800 coordinate space additions)
  // e.g. if we have extra space, we add to every widget's X.
  int layLogicalOffX = 0;
  int layLogicalOffY = 0;

  int globalDrawW = INITIAL_WIDTH;
  int globalDrawH = INITIAL_HEIGHT;
  int globalWinW = INITIAL_WIDTH;
  int globalWinH = INITIAL_HEIGHT;

  auto updateLayoutMetrics = [&](SDL_Window *win, SDL_Renderer *ren) {
    SDL_GetWindowSize(win, &globalWinW, &globalWinH);
    SDL_GetRendererOutputSize(ren, &globalDrawW, &globalDrawH);

    if (FIDELITY_MODE) {
      float sw = static_cast<float>(globalDrawW) / LOGICAL_WIDTH;
      float sh = static_cast<float>(globalDrawH) / LOGICAL_HEIGHT;
      layScale = std::min(sw, sh);

      // Calculate available LOGICAL space
      // If DrawW=3840, Scale=4.415, then LogicalWidth = 869.7 units.
      // Content is 800 units wide.
      // Surplus = 69.7 units.
      // Offset = 34 units.
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
  };

  // Initial calculation
  updateLayoutMetrics(window, renderer);

  if (TTF_Init() != 0) {
    std::fprintf(stderr, "TTF_Init failed: %s\n", TTF_GetError());
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();
    return EXIT_FAILURE;
  }

  curl_global_init(CURL_GLOBAL_DEFAULT);

  // --- Data layer persistent globals ---
  NetworkManager netManager(cfgMgr.configDir() / "cache");
  PrefixManager prefixMgr;
  prefixMgr.init();
  CitiesManager::getInstance().init();

  auto solarStore = std::make_shared<SolarDataStore>();
  auto watchlistStore = std::make_shared<WatchlistStore>();
  auto rssStore = std::make_shared<RSSDataStore>();
  auto watchlistHitStore = std::make_shared<WatchlistHitStore>();
  auto spotStore = std::make_shared<LiveSpotDataStore>();
  spotStore->setSelectedBandsMask(appCfg.pskBands);
  auto activityStore = std::make_shared<ActivityDataStore>();
  auto dxcStore = std::make_shared<DXClusterDataStore>();
  auto bandStore = std::make_shared<BandConditionsStore>();
  auto contestStore = std::make_shared<ContestStore>();
  auto moonStore = std::make_shared<MoonStore>();
  auto historyStore = std::make_shared<HistoryStore>();
  auto deWeatherStore = std::make_shared<WeatherStore>();
  auto dxWeatherStore = std::make_shared<WeatherStore>();
  auto callbookStore = std::make_shared<CallbookStore>();
  auto dstStore = std::make_shared<DstStore>();
  auto adifStore = std::make_shared<ADIFStore>();
  auto santaStore = std::make_shared<SantaStore>();
  auto state = std::make_shared<HamClockState>();

  // Pre-populate watchlist with defaults if empty
  if (watchlistStore->getAll().empty()) {
    watchlistStore->add("K1ABC");
    watchlistStore->add("W1AW");
  }

  // --- Web Server (Persistent) ---
  WebServer webServer(renderer, appCfg, *state, cfgMgr, watchlistStore,
                      solarStore, 8080);
  webServer.start();

  bool appRunning = true;
  while (appRunning) {
    // --- Run setup screen if needed (first run or gear icon click) ---
    if (activeSetup != SetupMode::None) {
      FontManager setupFontMgr;
      setupFontMgr.loadFromMemory(assets_font_ttf, assets_font_ttf_len,
                                  FONT_SIZE);

      int setupW = LOGICAL_WIDTH;
      int setupH = LOGICAL_HEIGHT;

      // Setup starts centered in logical space if fidelity mode is on
      int setupX = layLogicalOffX;
      int setupY = layLogicalOffY;

      std::unique_ptr<Widget> setupWidget;
      if (activeSetup == SetupMode::Main) {
        auto s = std::make_unique<SetupScreen>(setupX, setupY, setupW, setupH,
                                               setupFontMgr);
        s->setConfig(appCfg);
        setupWidget = std::move(s);
      } else if (activeSetup == SetupMode::DXCluster) {
        auto s = std::make_unique<DXClusterSetup>(setupX, setupY, setupW,
                                                  setupH, setupFontMgr);
        s->setConfig(appCfg);
        setupWidget = std::move(s);
      }

      SDL_StartTextInput();

      auto renderSetup = [&]() {
        SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
        SDL_RenderClear(renderer);

        if (FIDELITY_MODE) {
          SDL_RenderSetViewport(renderer, nullptr);
          SDL_RenderSetScale(renderer, layScale, layScale);
        }

        setupWidget->render(renderer);
        SDL_RenderPresent(renderer);

        if (FIDELITY_MODE) {
          SDL_RenderSetScale(renderer, 1.0f, 1.0f);
        }
      };

      bool setupRunning = true;
      while (setupRunning) {
        if (activeSetup == SetupMode::Main) {
          if (static_cast<SetupScreen *>(setupWidget.get())->isComplete())
            break;
        } else if (activeSetup == SetupMode::DXCluster) {
          if (static_cast<DXClusterSetup *>(setupWidget.get())->isComplete())
            break;
        }

        SDL_Event event;
        while (SDL_PollEvent(&event)) {
          switch (event.type) {
          case SDL_QUIT:
            setupRunning = false;
            appRunning = false;
            break;
          case SDL_KEYDOWN:
            setupWidget->onKeyDown(event.key.keysym.sym, event.key.keysym.mod);
            break;
          case SDL_TEXTINPUT:
            setupWidget->onTextInput(event.text.text);
            break;
          case SDL_WINDOWEVENT:
            if (event.window.event == SDL_WINDOWEVENT_SIZE_CHANGED) {
              updateLayoutMetrics(window, renderer);
              setupWidget->onResize(layLogicalOffX, layLogicalOffY,
                                    LOGICAL_WIDTH, LOGICAL_HEIGHT);
              renderSetup();
            } else if (event.window.event == SDL_WINDOWEVENT_EXPOSED) {
              renderSetup();
            }
            break;
          case SDL_MOUSEBUTTONUP: {
            int smx = event.button.x, smy = event.button.y;
            if (FIDELITY_MODE) {
              float pixX =
                  event.button.x * static_cast<float>(globalDrawW) / globalWinW;
              float pixY =
                  event.button.y * static_cast<float>(globalDrawH) / globalWinH;
              smx = static_cast<int>(pixX / layScale);
              smy = static_cast<int>(pixY / layScale);
            }
            setupWidget->onMouseUp(smx, smy, SDL_GetModState());
            break;
          }
          default:
            break;
          }
        }

        setupWidget->update();
        renderSetup();
        SDL_Delay(FRAME_DELAY_MS);
      }

      SDL_StopTextInput();

      if (!appRunning)
        break;

      // Extract config back and save if not cancelled
      bool shouldSave = true;
      if (activeSetup == SetupMode::Main && setupWidget) {
        auto *s = dynamic_cast<SetupScreen *>(setupWidget.get());
        if (s && s->wasCancelled()) {
          shouldSave = false;
        } else if (s) {
          appCfg = s->getConfig();
        }
      } else if (activeSetup == SetupMode::DXCluster && setupWidget) {
        auto *s = dynamic_cast<DXClusterSetup *>(setupWidget.get());
        if (s && !s->isSaved()) {
          shouldSave = false;
        } else if (s) {
          appCfg = s->updateConfig(appCfg);
        }
      }

      if (shouldSave) {
        cfgMgr.save(appCfg);
      }
      activeSetup = SetupMode::None;
    }

    // --- Update global state from config (in case changed in setup) ---
    state->deCallsign = appCfg.callsign;
    state->deGrid = appCfg.grid;
    state->deLocation = {appCfg.lat, appCfg.lon};

    // Scope block: all managers/widgets must be destroyed before re-entering
    // setup.
    {
      FontManager fontMgr;
      if (!fontMgr.loadFromMemory(assets_font_ttf, assets_font_ttf_len,
                                  FONT_SIZE)) {
        std::fprintf(stderr, "Warning: text rendering disabled\n");
      }

      TextureManager texMgr;
      // Load global assets

      FontCatalog fontCatalog(fontMgr);
      fontMgr.setCatalog(&fontCatalog);

      // Compute render scale for hi-DPI text super-sampling
      {
        int drawW, drawH;
        SDL_GetRendererOutputSize(renderer, &drawW, &drawH);
        float rs = static_cast<float>(drawH) / LOGICAL_HEIGHT;
        fontMgr.setRenderScale(rs);
      }

      DebugOverlay debugOverlay(fontMgr);

      // --- Data providers ---
      auto auroraHistoryStore = std::make_shared<AuroraHistoryStore>();
      NOAAProvider noaaProvider(netManager, solarStore, auroraHistoryStore,
                                state.get());
      noaaProvider.fetch();

      RSSProvider rssProvider(netManager, rssStore);
      rssProvider.fetch();

      LiveSpotProvider spotProvider(netManager, spotStore, appCfg, state.get());
      spotProvider.fetch();

      SatelliteManager satMgr(netManager);
      satMgr.fetch();

      ActivityProvider activityProvider(netManager, activityStore);
      activityProvider.fetch();

      DXClusterProvider dxcProvider(dxcStore, prefixMgr, watchlistStore,
                                    watchlistHitStore, state.get());
      dxcProvider.start(appCfg);

      BandConditionsProvider bandProvider(solarStore, bandStore);
      bandProvider.update();

      ContestProvider contestProvider(netManager, contestStore);
      contestProvider.fetch();

      MoonProvider moonProvider(netManager, moonStore);
      moonProvider.update(appCfg.lat, appCfg.lon);

      HistoryProvider historyProvider(netManager, historyStore);
      historyProvider.fetchFlux();
      historyProvider.fetchSSN();
      historyProvider.fetchKp();

      WeatherProvider deWeatherProvider(netManager, deWeatherStore);
      deWeatherProvider.fetch(state->deLocation.lat, state->deLocation.lon);

      WeatherProvider dxWeatherProvider(netManager, dxWeatherStore);
      dxWeatherProvider.fetch(state->dxLocation.lat, state->dxLocation.lon);

      SDOProvider sdoProvider(netManager);
      DRAPProvider drapProvider(netManager);
      auto auroraProvider = std::make_shared<AuroraProvider>(netManager);

      auto callbookProvider =
          std::make_shared<CallbookProvider>(netManager, callbookStore);
      callbookProvider->lookup("K1ABC");

      DstProvider dstProvider(netManager, dstStore);
      dstProvider.fetch();

      ADIFProvider adifProvider(adifStore);
      adifProvider.fetch(cfgMgr.configDir() / "logs.adif");

      SantaProvider santaProvider(santaStore);
      santaProvider.update();

      // --- Top Bar widgets ---
      SDL_Color cyan = {0, 200, 255, 255};

      TimePanel timePanel(0, 0, 0, 0, fontMgr, texMgr, appCfg.callsign);
      timePanel.setCallColor(appCfg.callsignColor);
      timePanel.setOnConfigChanged(
          [&appCfg, &cfgMgr](const std::string &call, SDL_Color color) {
            appCfg.callsign = call;
            appCfg.callsignColor = color;
            cfgMgr.save(appCfg);
          });

      WidgetSelector widgetSelector(fontMgr);

      // Create all possible widgets
      std::map<WidgetType, std::unique_ptr<Widget>> widgetPool;
      auto addToPool = [&](WidgetType type) {
        switch (type) {
        case WidgetType::SOLAR:
          widgetPool[type] = std::make_unique<SpaceWeatherPanel>(
              0, 0, 0, 0, fontMgr, solarStore);
          break;
        case WidgetType::DX_CLUSTER:
          widgetPool[type] =
              std::make_unique<DXClusterPanel>(0, 0, 0, 0, fontMgr, dxcStore);
          break;
        case WidgetType::LIVE_SPOTS:
          widgetPool[type] = std::make_unique<LiveSpotPanel>(
              0, 0, 0, 0, fontMgr, spotProvider, spotStore, appCfg, cfgMgr);
          break;
        case WidgetType::BAND_CONDITIONS:
          widgetPool[type] = std::make_unique<BandConditionsPanel>(
              0, 0, 0, 0, fontMgr, bandStore);
          break;
        case WidgetType::CONTESTS:
          widgetPool[type] =
              std::make_unique<ContestPanel>(0, 0, 0, 0, fontMgr, contestStore);
          break;
        case WidgetType::CALLBOOK:
          widgetPool[type] = std::make_unique<CallbookPanel>(
              0, 0, 0, 0, fontMgr, callbookStore);
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
        case WidgetType::ON_THE_AIR:
          widgetPool[type] = std::make_unique<ONTAPanel>(
              0, 0, 0, 0, fontMgr, activityProvider, activityStore);
          break;
        case WidgetType::DX_PEDITIONS:
          widgetPool[type] = std::make_unique<DXPedPanel>(
              0, 0, 0, 0, fontMgr, activityProvider, activityStore);
          break;
        case WidgetType::GIMBAL:
          widgetPool[type] = std::make_unique<GimbalPanel>(0, 0, 0, 0, fontMgr);
          break;
        case WidgetType::MOON:
          widgetPool[type] = std::make_unique<MoonPanel>(
              0, 0, 0, 0, fontMgr, texMgr, netManager, moonStore);
          break;
        case WidgetType::CLOCK_AUX:
          widgetPool[type] =
              std::make_unique<ClockAuxPanel>(0, 0, 0, 0, fontMgr);
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
                                                         texMgr, drapProvider);
          break;
        case WidgetType::AURORA:
          widgetPool[type] = std::make_unique<AuroraPanel>(
              0, 0, 0, 0, fontMgr, texMgr, *auroraProvider);
          break;
        case WidgetType::AURORA_GRAPH:
          widgetPool[type] = std::make_unique<AuroraGraphPanel>(
              0, 0, 0, 0, fontMgr, auroraHistoryStore);
          break;
        case WidgetType::ADIF:
          widgetPool[type] =
              std::make_unique<ADIFPanel>(0, 0, 0, 0, fontMgr, adifStore);
          break;
        case WidgetType::COUNTDOWN:
          widgetPool[type] =
              std::make_unique<CountdownPanel>(0, 0, 0, 0, fontMgr);
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
          widgetPool[type] = std::make_unique<SDOPanel>(0, 0, 0, 0, fontMgr,
                                                        texMgr, sdoProvider);
          break;
        default:
          widgetPool[type] = std::make_unique<PlaceholderWidget>(
              0, 0, 0, 0, fontMgr, widgetTypeDisplayName(type), cyan);
          break;
        }
      };

      // Populate pool with all types
      std::vector<WidgetType> allTypes = {
          WidgetType::SOLAR,        WidgetType::DX_CLUSTER,
          WidgetType::LIVE_SPOTS,   WidgetType::BAND_CONDITIONS,
          WidgetType::CONTESTS,     WidgetType::ON_THE_AIR,
          WidgetType::GIMBAL,       WidgetType::MOON,
          WidgetType::CLOCK_AUX,    WidgetType::DX_PEDITIONS,
          WidgetType::DE_WEATHER,   WidgetType::DX_WEATHER,
          WidgetType::NCDXF,        WidgetType::SDO,
          WidgetType::HISTORY_FLUX, WidgetType::HISTORY_KP,
          WidgetType::HISTORY_SSN,  WidgetType::DRAP,
          WidgetType::AURORA,       WidgetType::AURORA_GRAPH,
          WidgetType::ADIF,         WidgetType::COUNTDOWN};
      for (auto t : allTypes)
        addToPool(t);

      // Create 4 pane containers
      std::vector<std::unique_ptr<PaneContainer>> panes;
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

      // Selection callback
      auto onPaneSelectionRequested = [&](int paneIdx, int mx, int my) {
        (void)mx;
        (void)my;
        std::vector<WidgetType> available = allTypes;
        if (paneIdx == 3) {
          available = {WidgetType::NCDXF, WidgetType::SOLAR,
                       WidgetType::DX_WEATHER, WidgetType::DE_WEATHER};
        }

        std::vector<WidgetType> current = panes[paneIdx]->getRotation();
        std::vector<WidgetType> forbidden;
        for (int i = 0; i < 4; ++i) {
          if (i == paneIdx)
            continue;
          auto rot = panes[i]->getRotation();
          forbidden.insert(forbidden.end(), rot.begin(), rot.end());
        }

        widgetSelector.show(
            paneIdx, available, current, forbidden,
            [&](int idx, const std::vector<WidgetType> &finalSelection) {
              panes[idx]->setRotation(finalSelection, appCfg.rotationIntervalS);

              // Save to config
              appCfg.pane1Rotation = panes[0]->getRotation();
              appCfg.pane2Rotation = panes[1]->getRotation();
              appCfg.pane3Rotation = panes[2]->getRotation();
              appCfg.pane4Rotation = panes[3]->getRotation();
              cfgMgr.save(appCfg);
            });
      };

      for (int i = 0; i < 4; ++i) {
        panes[i]->setOnSelectionRequested(onPaneSelectionRequested, i);
      }

      // --- Side Panel widgets (2 panes) ---
      LocalPanel localPanel(0, 0, 0, 0, fontMgr, state, deWeatherStore);
      DXSatPane dxSatPane(0, 0, 0, 0, fontMgr, texMgr, state, satMgr,
                          dxWeatherStore);
      dxSatPane.setObserver(appCfg.lat, appCfg.lon);
      dxSatPane.restoreState(appCfg.panelMode, appCfg.selectedSatellite);
      dxSatPane.setOnModeChanged(
          [&appCfg, &cfgMgr](const std::string &mode,
                             const std::string &satName) {
            appCfg.panelMode = mode;
            appCfg.selectedSatellite = satName;
            cfgMgr.save(appCfg);
          });

      // --- Main Stage ---
      MapWidget mapArea(0, 0, 0, 0, texMgr, fontMgr, netManager, state, appCfg);
      mapArea.setSpotStore(spotStore);
      mapArea.setDXClusterStore(dxcStore);
      mapArea.setAuroraStore(auroraHistoryStore);
      RSSBanner rssBanner(139, 412, 660, 68, fontMgr, rssStore);

      // --- Apply Theme ---
      for (auto const &[type, widget] : widgetPool) {
        if (widget) {
          widget->setTheme(appCfg.theme);
          widget->setMetric(appCfg.useMetric);
        }
      }
      timePanel.setTheme(appCfg.theme);
      timePanel.setMetric(appCfg.useMetric);
      localPanel.setTheme(appCfg.theme);
      localPanel.setMetric(appCfg.useMetric);
      dxSatPane.setTheme(appCfg.theme);
      dxSatPane.setMetric(appCfg.useMetric);
      mapArea.setTheme(appCfg.theme);
      mapArea.setMetric(appCfg.useMetric);
      rssBanner.setTheme(appCfg.theme);
      rssBanner.setMetric(appCfg.useMetric);
      widgetSelector.setTheme(appCfg.theme);
      widgetSelector.setMetric(appCfg.useMetric);
      for (auto &p : panes) {
        p->setTheme(appCfg.theme);
        p->setMetric(appCfg.useMetric);
      }

      // --- Layout ---
      LayoutManager layout;
      if (FIDELITY_MODE)
        layout.setFidelityMode(true);

      layout.addWidget(Zone::TopBar, &timePanel, 2.0f);
      layout.addWidget(Zone::TopBar, panes[0].get(), 1.5f);
      layout.addWidget(Zone::TopBar, panes[1].get(), 1.5f);
      layout.addWidget(Zone::TopBar, panes[2].get(), 1.5f);
      layout.addWidget(Zone::TopBar, panes[3].get(), 0.6f);

      layout.addWidget(Zone::SidePanel, &localPanel);
      layout.addWidget(Zone::SidePanel, &dxSatPane);

      layout.addWidget(Zone::MainStage, &mapArea);

      // Helper to update all UI positions with current logical offsets
      auto recalculateUI = [&]() {
        fontCatalog.recalculate(LOGICAL_WIDTH, LOGICAL_HEIGHT);
        layout.recalculate(LOGICAL_WIDTH, LOGICAL_HEIGHT, layLogicalOffX,
                           layLogicalOffY);
        // Manually update RSSBanner which is not in LayoutManager
        // Canonical pos: 139, 412, 660, 68
        rssBanner.onResize(139 + layLogicalOffX, 412 + layLogicalOffY, 660, 68);
      };

      // In fidelity mode, always use logical 800x480 for layout and fonts
      recalculateUI();

      std::vector<Widget *> widgets = {
          &timePanel,     panes[0].get(), panes[1].get(), panes[2].get(),
          panes[3].get(), &localPanel,    &dxSatPane,     &mapArea,
          &rssBanner,     &widgetSelector};

      std::vector<Widget *> eventWidgets = {
          &widgetSelector, &timePanel,     panes[0].get(), panes[1].get(),
          panes[2].get(),  panes[3].get(), &localPanel,    &dxSatPane,
          &mapArea,        &rssBanner};

      // Build the overlay's actual-rect list from live widget positions.
      auto buildActuals = [&]() -> std::vector<WidgetRect> {
        return {
            {"TimePanel", timePanel.getRect()},
            {widgetTypeDisplayName(panes[0]->getActiveType()),
             panes[0]->getRect()},
            {widgetTypeDisplayName(panes[1]->getActiveType()),
             panes[1]->getRect()},
            {widgetTypeDisplayName(panes[2]->getActiveType()),
             panes[2]->getRect()},
            {widgetTypeDisplayName(panes[3]->getActiveType()),
             panes[3]->getRect()},
            {"LocalPanel", localPanel.getRect()},
            {"DXSatPane", dxSatPane.getRect()},
            {"MapWidget", mapArea.getRect()},
            {"RSSBanner", rssBanner.getRect()},
        };
      };

      // Helper: force an immediate frame (used after resize/expose to
      // prevent blank areas while the user is still dragging).
      auto renderFrame = [&]() {
        SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
        SDL_RenderClear(renderer);

        if (FIDELITY_MODE) {
          // Reset viewport logic to "Full Window + Scale"
          // We do NOT use SDL viewports for offsetting anymore, as they are
          // flaky.
          SDL_RenderSetViewport(renderer, nullptr);
          SDL_RenderSetScale(renderer, layScale, layScale);
        }

        Widget *activeModal = nullptr;
        for (auto *w : widgets) {
          if (w->isModalActive())
            activeModal = w;
          SDL_Rect clip = w->getRect();
          SDL_RenderSetClipRect(renderer, &clip);
          w->render(renderer);
        }
        SDL_RenderSetClipRect(renderer, nullptr);

        if (activeModal) {
          // Semi-transparent dimming overlay
          SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
          SDL_SetRenderDrawColor(renderer, 0, 0, 0, 150);
          SDL_Rect full = {0, 0, LOGICAL_WIDTH, LOGICAL_HEIGHT};
          SDL_RenderFillRect(renderer, &full);
          // Render modal content (unclipped)
          activeModal->renderModal(renderer);
        }

        if (debugOverlay.isVisible()) {
          debugOverlay.render(renderer, LOGICAL_WIDTH, LOGICAL_HEIGHT,
                              buildActuals());
          for (size_t i = 0; i < panes.size(); ++i) {
            SDL_Rect r = panes[i]->getRect();
            fontMgr.drawText(renderer,
                             widgetTypeToString(panes[i]->getActiveType()),
                             r.x + 2, r.y + 2, {255, 128, 0, 255}, 10);
          }
        }
        SDL_RenderPresent(renderer);

        if (FIDELITY_MODE) {
          SDL_RenderSetScale(renderer, 1.0f, 1.0f);
        }
      };

      // --- Dashboard Loop ---
      Uint32 lastFetchMs = SDL_GetTicks();
      Uint32 lastResizeMs = 0; // debounce timer for font re-rasterization
      bool running = true;
      Uint32 lastFpsUpdate = SDL_GetTicks();
      int frames = 0;
      while (running) {
        Uint32 now = SDL_GetTicks();

        // Background refresh every 15 minutes
        if (now - lastFetchMs > 15 * 60 * 1000) {
          noaaProvider.fetch();
          rssProvider.fetch();
          spotProvider.fetch();
          satMgr.fetch();
          activityProvider.fetch();
          bandProvider.update();
          contestProvider.fetch();
          moonProvider.update(appCfg.lat, appCfg.lon);
          deWeatherProvider.fetch(state->deLocation.lat, state->deLocation.lon);
          dxWeatherProvider.fetch(state->dxLocation.lat, state->dxLocation.lon);
          historyProvider.fetchFlux();
          historyProvider.fetchSSN();
          historyProvider.fetchKp();
          adifProvider.fetch(cfgMgr.configDir() / "logs.adif");
          lastFetchMs = now;
        }

        // Ensure layout metrics are always up to date with actual window state
        // This fixes issues where Resize events might report stale or
        // intermediate sizes, or where the renderer output size lags behind the
        // window size.
        updateLayoutMetrics(window, renderer);

        static Uint32 lastMouseMotionMs = SDL_GetTicks();
        static bool cursorVisible = true;

        SDL_Event event;
        while (SDL_PollEvent(&event)) {
          // Any mouse activity resets the timer
          if (event.type == SDL_MOUSEMOTION ||
              event.type == SDL_MOUSEBUTTONDOWN ||
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
            running = false;
            appRunning = false;
            break;
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
              consumed = activeModal->onKeyDown(event.key.keysym.sym,
                                                event.key.keysym.mod);
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
                running = false;
                appRunning = false;
              } else if (event.key.keysym.sym == SDLK_F11) {
                Uint32 flags = SDL_GetWindowFlags(window);
                SDL_SetWindowFullscreen(window,
                                        (flags & SDL_WINDOW_FULLSCREEN_DESKTOP)
                                            ? 0
                                            : SDL_WINDOW_FULLSCREEN_DESKTOP);
              } else if (event.key.keysym.sym == SDLK_o) {
                debugOverlay.toggle();
                if (debugOverlay.isVisible()) {
                  debugOverlay.dumpReport(LOGICAL_WIDTH, LOGICAL_HEIGHT,
                                          buildActuals());
                }
              } else if (event.key.keysym.sym == SDLK_k) {
                // Cycle alignment mode
                int m = static_cast<int>(alignMode);
                m = (m + 1) % 3;
                alignMode = static_cast<AlignMode>(m);
                updateLayoutMetrics(window, renderer);
                recalculateUI();
              }
            }
            break;
          }
          case SDL_WINDOWEVENT:
            if (event.window.event == SDL_WINDOWEVENT_SIZE_CHANGED) {
              updateLayoutMetrics(window, renderer);
              // Update render scale for hi-DPI text super-sampling.
              // Only invalidate caches on significant scale changes
              // (e.g., fullscreen toggle), not every drag pixel.
              {
                float ns = static_cast<float>(globalDrawH) / LOGICAL_HEIGHT;
                float old = fontMgr.renderScale();
                if (ns > 0.5f && std::fabs(ns - old) / old > 0.05f) {
                  fontMgr.setRenderScale(ns);
                  recalculateUI();
                }
                lastResizeMs = SDL_GetTicks();
              }
              if (!FIDELITY_MODE) {
                fontCatalog.recalculate(event.window.data1, event.window.data2);
                layout.recalculate(event.window.data1, event.window.data2);
              }
              renderFrame();
            } else if (event.window.event == SDL_WINDOWEVENT_EXPOSED) {
              renderFrame();
            }
            break;
          case SDL_TEXTINPUT: {
            Widget *activeModal = nullptr;
            for (auto *w : eventWidgets) {
              if (w->isModalActive()) {
                activeModal = w;
                break;
              }
            }
            if (activeModal) {
              activeModal->onTextInput(event.text.text);
            } else {
              for (auto *w : widgets) {
                if (w->onTextInput(event.text.text))
                  break;
              }
            }
            break;
          }
          case SDL_MOUSEMOTION: {
            int mx = event.motion.x, my = event.motion.y;
            if (FIDELITY_MODE) {
              float pixX =
                  event.motion.x * static_cast<float>(globalDrawW) / globalWinW;
              float pixY =
                  event.motion.y * static_cast<float>(globalDrawH) / globalWinH;
              mx = static_cast<int>(pixX / layScale);
              my = static_cast<int>(pixY / layScale);
            }
            // Dispatch to modal if active
            Widget *activeModal = nullptr;
            for (auto *w : eventWidgets) {
              if (w->isModalActive()) {
                activeModal = w;
                break;
              }
            }
            if (activeModal) {
              activeModal->onMouseMove(mx, my);
            } else {
              for (auto *w : eventWidgets) {
                w->onMouseMove(mx, my);
              }
            }
            break;
          }
          case SDL_MOUSEBUTTONUP:
            if (event.button.button == SDL_BUTTON_LEFT) {
              int mx = event.button.x, my = event.button.y;
              if (FIDELITY_MODE) {
                // Window coords → Scaled Renderer Coords
                float pixX = event.button.x * static_cast<float>(globalDrawW) /
                             globalWinW;
                float pixY = event.button.y * static_cast<float>(globalDrawH) /
                             globalWinH;

                // Scaled Pixels -> Logical Coords
                // NO subtracting layOffX, because LAYOUT applies offset to
                // WIDGETS now. We just need to convert screen space to logical
                // space.
                mx = static_cast<int>(pixX / layScale);
                my = static_cast<int>(pixY / layScale);
              }
              Widget *activeModal = nullptr;
              for (auto *w : eventWidgets) {
                if (w->isModalActive()) {
                  activeModal = w;
                  break;
                }
              }

              if (activeModal) {
                activeModal->onMouseUp(mx, my, SDL_GetModState());
              } else {
                for (auto *w : eventWidgets) {
                  if (w->onMouseUp(mx, my, SDL_GetModState()))
                    break;
                }
              }
            }
            break;
          case SDL_MOUSEWHEEL:
            for (auto *w : eventWidgets) {
              if (w->onMouseWheel(event.wheel.y))
                break;
            }
            break;
          default:
            break;
          }
        }

        // Check if gear icon was clicked → re-enter setup
        if (timePanel.isSetupRequested()) {
          timePanel.clearSetupRequest();
          activeSetup = SetupMode::Main;
          running = false;
          break;
        }

        // Check if DX Cluster requested setup (clicking bottom half)
        auto *dxcPanel = dynamic_cast<DXClusterPanel *>(
            widgetPool[WidgetType::DX_CLUSTER].get());
        if (dxcPanel && dxcPanel->isSetupRequested()) {
          dxcPanel->clearSetupRequest();
          activeSetup = SetupMode::DXCluster;
          running = false;
          break;
        }

        // Wire satellite predictor to map (nullptr when in DX mode)
        mapArea.setPredictor(dxSatPane.activePredictor());

        // Update Gimbal with active satellite and observer
        auto *gimbal =
            dynamic_cast<GimbalPanel *>(widgetPool[WidgetType::GIMBAL].get());
        if (gimbal) {
          gimbal->setPredictor(dxSatPane.activePredictor());
          gimbal->setObserver(appCfg.lat, appCfg.lon);
        }

        // Debounced font re-rasterization after resize settles
        if (lastResizeMs && (SDL_GetTicks() - lastResizeMs > 200)) {
          lastResizeMs = 0;
          int dw, dh;
          SDL_GetRendererOutputSize(renderer, &dw, &dh);
          float ns = static_cast<float>(dh) / LOGICAL_HEIGHT;
          if (ns > 0.5f && std::fabs(ns - fontMgr.renderScale()) > 0.01f) {
            fontMgr.setRenderScale(ns);
            recalculateUI();
          }
        }

        // Hide cursor if inactive for 10 seconds
        if (cursorVisible && (SDL_GetTicks() - lastMouseMotionMs > 10000)) {
          SDL_ShowCursor(SDL_DISABLE);
          cursorVisible = false;
        }

        for (auto *w : widgets)
          w->update();

#ifdef ENABLE_DEBUG_API
        // Update Semantic Debug Registry
        {
          auto &reg = UIRegistry::getInstance();
          reg.setScale(layScale, layLogicalOffX, layLogicalOffY);

          std::map<std::string, WidgetInfo> newSnapshot;
          for (auto *w : eventWidgets) {
            WidgetInfo info;
            info.name = w->getName();
            info.rect = w->getRect();
            for (const auto &action : w->getActions()) {
              info.actions.push_back({action, w->getActionRect(action)});
            }
            info.data = w->getDebugData();
            newSnapshot[info.name] = info;
          }
          reg.replaceAll(newSnapshot);
        }
#endif

        renderFrame();
        webServer.updateFrame();

        // Update FPS telemetry
        frames++;
        Uint32 nowMs = SDL_GetTicks();
        if (nowMs - lastFpsUpdate >= 1000) {
          state->fps = frames * 1000.0f / (nowMs - lastFpsUpdate);
          frames = 0;
          lastFpsUpdate = nowMs;
        }

        SDL_Delay(FRAME_DELAY_MS);
      }
    } // widgets/managers destroyed here
  }

  curl_global_cleanup();
  TTF_Quit();
  SDL_DestroyRenderer(renderer);
  SDL_DestroyWindow(window);
  SDL_Quit();

  return EXIT_SUCCESS;
}
#ifdef _WIN32
#include <windows.h>
int WINAPI WinMain(HINSTANCE hInst, HINSTANCE hPrev, LPSTR szCmdLine, int sw) {
  return main(__argc, __argv);
}
#endif
