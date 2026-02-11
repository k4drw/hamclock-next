#include "core/ConfigManager.h"
#include "core/HamClockState.h"
#include "core/LiveSpotData.h"
#include "core/RSSData.h"
#include "core/SatelliteManager.h"
#include "core/SolarData.h"
#include "core/WidgetType.h"
#include "network/NetworkManager.h"
#include "services/LiveSpotProvider.h"
#include "services/NOAAProvider.h"
#include "services/RSSProvider.h"
#include "ui/DXSatPane.h"
#include "ui/DebugOverlay.h"
#include "ui/FontCatalog.h"
#include "ui/FontManager.h"
#include "ui/LayoutManager.h"
#include "ui/ListPanel.h"
#include "ui/LiveSpotPanel.h"
#include "ui/LocalPanel.h"
#include "ui/MapWidget.h"
#include "ui/PlaceholderWidget.h"
#include "ui/RSSBanner.h"
#include "ui/SetupScreen.h"
#include "ui/SpaceWeatherPanel.h"
#include "ui/TextureManager.h"
#include "ui/TimePanel.h"

#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>
#include <curl/curl.h>
#include <nlohmann/json.hpp>

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <memory>
#include <vector>

static constexpr int INITIAL_WIDTH = 800;
static constexpr int INITIAL_HEIGHT = 480;
static constexpr int LOGICAL_WIDTH = 800;
static constexpr int LOGICAL_HEIGHT = 480;
static constexpr int FRAME_DELAY_MS = 10;
static constexpr bool FIDELITY_MODE = true;

static constexpr const char *FONT_PATH = "assets/font.ttf";
static constexpr const char *LEGACY_CONFIG_PATH = "assets/config.json";
static constexpr int FONT_SIZE = 24;

int main(int /*argc*/, char * /*argv*/[]) {
  // --- Load config via ConfigManager ---
  ConfigManager cfgMgr;
  AppConfig appCfg;
  bool needSetup = false;

  if (!cfgMgr.init()) {
    std::fprintf(stderr, "Warning: could not resolve config path\n");
    needSetup = true;
  } else if (!cfgMgr.load(appCfg)) {
    // No config at ~/.config/hamclock/ — try migrating from legacy
    // assets/config.json
    std::ifstream ifs(LEGACY_CONFIG_PATH);
    if (ifs) {
      auto legacy = nlohmann::json::parse(ifs, nullptr, false);
      if (!legacy.is_discarded()) {
        appCfg.callsign = legacy.value("callsign", "");
        appCfg.grid = legacy.value("grid", "");
        if (!appCfg.callsign.empty() && !appCfg.grid.empty()) {
          Astronomy::gridToLatLon(appCfg.grid, appCfg.lat, appCfg.lon);
          cfgMgr.save(appCfg);
          std::fprintf(stderr, "Migrated config to %s\n",
                       cfgMgr.configPath().c_str());
        } else {
          needSetup = true;
        }
      } else {
        needSetup = true;
      }
    } else {
      needSetup = true;
    }
  }

  // --- Init SDL2 (needed before setup screen) ---
  if (SDL_Init(SDL_INIT_VIDEO) != 0) {
    std::fprintf(stderr, "SDL_Init failed: %s\n", SDL_GetError());
    return EXIT_FAILURE;
  }

  SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "best");

  // Set MSAA attributes BEFORE creating the window
  SDL_GL_SetAttribute(SDL_GL_MULTISAMPLEBUFFERS, 1);
  SDL_GL_SetAttribute(SDL_GL_MULTISAMPLESAMPLES, 4);

  SDL_Window *window = SDL_CreateWindow(
      "HamClock-Next", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
      INITIAL_WIDTH, INITIAL_HEIGHT,
      SDL_WINDOW_SHOWN | SDL_WINDOW_RESIZABLE | SDL_WINDOW_OPENGL);
  if (!window) {
    std::fprintf(stderr, "SDL_CreateWindow failed: %s\n", SDL_GetError());
    SDL_Quit();
    return EXIT_FAILURE;
  }

  SDL_Renderer *renderer = SDL_CreateRenderer(
      window, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
  if (!renderer) {
    std::fprintf(stderr, "SDL_CreateRenderer failed: %s\n", SDL_GetError());
    SDL_DestroyWindow(window);
    SDL_Quit();
    return EXIT_FAILURE;
  }

  // if (FIDELITY_MODE) {
  //     SDL_RenderSetLogicalSize(renderer, LOGICAL_WIDTH, LOGICAL_HEIGHT);
  // }

  // --- Layout Alignment State ---
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

  bool appRunning = true;
  while (appRunning) {
    // --- Run setup screen if needed (first run or gear icon click) ---
    if (needSetup) {
      FontManager setupFontMgr;
      setupFontMgr.load(FONT_PATH, FONT_SIZE);

      int setupW = FIDELITY_MODE ? LOGICAL_WIDTH : 0;
      int setupH = FIDELITY_MODE ? LOGICAL_HEIGHT : 0;
      if (!FIDELITY_MODE) {
        SDL_GetWindowSize(window, &setupW, &setupH);
      }

      // Setup starts centered in logical space if fidelity mode is on
      int setupX = FIDELITY_MODE ? layLogicalOffX : 0;
      int setupY = FIDELITY_MODE ? layLogicalOffY : 0;

      SetupScreen setup(setupX, setupY, setupW, setupH, setupFontMgr);
      setup.setConfig(appCfg);
      SDL_StartTextInput();

      auto renderSetup = [&]() {
        SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
        SDL_RenderClear(renderer);

        if (FIDELITY_MODE) {
          // Viewport matches full window, we handle offset via layout placement
          SDL_RenderSetViewport(renderer, nullptr);
          SDL_RenderSetScale(renderer, layScale, layScale);
        }

        setup.render(renderer);
        SDL_RenderPresent(renderer);

        // Cleanup render state if needed, though we clear next frame anyway.
        if (FIDELITY_MODE) {
          SDL_RenderSetScale(renderer, 1.0f, 1.0f);
        }
      };

      bool running = true;
      while (running && !setup.isComplete()) {
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
          switch (event.type) {
          case SDL_QUIT:
            running = false;
            appRunning = false;
            break;
          case SDL_KEYDOWN:
            setup.onKeyDown(event.key.keysym.sym, event.key.keysym.mod);
            break;
          case SDL_TEXTINPUT:
            setup.onTextInput(event.text.text);
            break;
          case SDL_WINDOWEVENT:
            if (event.window.event == SDL_WINDOWEVENT_SIZE_CHANGED) {
              updateLayoutMetrics(window, renderer);
              if (!FIDELITY_MODE) {
                setup.onResize(0, 0, event.window.data1, event.window.data2);
              } else {
                // Keep setup centered in logical space
                setup.onResize(layLogicalOffX, layLogicalOffY, LOGICAL_WIDTH,
                               LOGICAL_HEIGHT);
              }
              renderSetup();
            } else if (event.window.event == SDL_WINDOWEVENT_EXPOSED) {
              renderSetup();
            }
            break;
          case SDL_MOUSEBUTTONUP: {
            int smx = event.button.x, smy = event.button.y;
            if (FIDELITY_MODE) {
              // Window -> Render(Scaled)
              float pixX =
                  event.button.x * static_cast<float>(globalDrawW) / globalWinW;
              float pixY =
                  event.button.y * static_cast<float>(globalDrawH) / globalWinH;

              // Scaled Pixel -> Logical
              smx = static_cast<int>(pixX / layScale);
              smy = static_cast<int>(pixY / layScale);

              // Since widgets are physically shifted by layLogicalOffX, they
              // will match this coordinate.
            }
            setup.onMouseUp(smx, smy, SDL_GetModState());
            break;
          }
          default:
            break;
          }
        }

        setup.update();

        renderSetup();
        SDL_Delay(FRAME_DELAY_MS);
      }

      SDL_StopTextInput();

      if (!appRunning || !setup.isComplete())
        break;

      appCfg = setup.getConfig();
      cfgMgr.save(appCfg);
      needSetup = false;
    }

    // --- Init global state from config ---
    auto state = std::make_shared<HamClockState>();
    state->deCallsign = appCfg.callsign;
    state->deGrid = appCfg.grid;
    state->deLocation = {appCfg.lat, appCfg.lon};

    // Scope block: all managers/widgets must be destroyed before re-entering
    // setup.
    {
      FontManager fontMgr;
      if (!fontMgr.load(FONT_PATH, FONT_SIZE)) {
        std::fprintf(stderr, "Warning: text rendering disabled\n");
      }

      TextureManager texMgr;
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

      // --- Data layer ---
      auto solarStore = std::make_shared<SolarDataStore>();
      NetworkManager netManager(cfgMgr.configDir() / "cache");
      NOAAProvider noaaProvider(netManager, solarStore);
      noaaProvider.fetch();

      auto rssStore = std::make_shared<RSSDataStore>();
      RSSProvider rssProvider(netManager, rssStore);
      rssProvider.fetch();

      auto spotStore = std::make_shared<LiveSpotDataStore>();
      LiveSpotProvider spotProvider(netManager, spotStore, appCfg.callsign,
                                    appCfg.grid);
      spotProvider.fetch();

      SatelliteManager satMgr(netManager);
      satMgr.fetch();

      // --- Top Bar widgets ---
      SDL_Color cyan = {0, 200, 255, 255};

      TimePanel timePanel(0, 0, 0, 0, fontMgr, appCfg.callsign);
      timePanel.setCallColor(appCfg.callsignColor);
      timePanel.setOnConfigChanged(
          [&appCfg, &cfgMgr](const std::string &call, SDL_Color color) {
            appCfg.callsign = call;
            appCfg.callsignColor = color;
            cfgMgr.save(appCfg);
          });

      // Pane widget factory: creates a widget for a given WidgetType.
      auto createPaneWidget = [&](WidgetType type) -> std::unique_ptr<Widget> {
        switch (type) {
        case WidgetType::SOLAR:
          return std::make_unique<SpaceWeatherPanel>(0, 0, 0, 0, fontMgr,
                                                     solarStore);
        case WidgetType::DX_CLUSTER:
          return std::make_unique<ListPanel>(0, 0, 0, 0, fontMgr, "DX Cluster",
                                             std::vector<std::string>{
                                                 "14.074  FT8  K1ABC",
                                                 "7.074   FT8  W1AW",
                                                 "21.074  FT8  VE3ABC",
                                                 "10.136  FT8  JA1XYZ",
                                             });
        case WidgetType::LIVE_SPOTS:
          return std::make_unique<LiveSpotPanel>(0, 0, 0, 0, fontMgr,
                                                 spotStore);
        case WidgetType::BAND_CONDITIONS:
          return std::make_unique<PlaceholderWidget>(0, 0, 0, 0, fontMgr,
                                                     "Band Cond", cyan);
        case WidgetType::CONTESTS:
          return std::make_unique<PlaceholderWidget>(0, 0, 0, 0, fontMgr,
                                                     "Contests", cyan);
        case WidgetType::ON_THE_AIR:
          return std::make_unique<PlaceholderWidget>(0, 0, 0, 0, fontMgr,
                                                     "On The Air", cyan);
        }
        return std::make_unique<PlaceholderWidget>(0, 0, 0, 0, fontMgr,
                                                   "Unknown", cyan);
      };

      // Create pane widgets from config (Panes 1–3)
      WidgetType pane1Type = appCfg.pane1Widget;
      WidgetType pane2Type = appCfg.pane2Widget;
      WidgetType pane3Type = appCfg.pane3Widget;
      auto pane1 = createPaneWidget(pane1Type);
      auto pane2 = createPaneWidget(pane2Type);
      auto pane3 = createPaneWidget(pane3Type);

      PlaceholderWidget bandCond(0, 0, 0, 0, fontMgr, "Band Cond", cyan);

      // --- Side Panel widgets (2 panes) ---
      LocalPanel localPanel(0, 0, 0, 0, fontMgr, state);
      DXSatPane dxSatPane(0, 0, 0, 0, fontMgr, state, satMgr);
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
      MapWidget mapArea(0, 0, 0, 0, texMgr, state);
      mapArea.setSpotStore(spotStore);
      RSSBanner rssBanner(139, 412, 660, 68, fontMgr, rssStore);

      // --- Layout ---
      LayoutManager layout;
      if (FIDELITY_MODE)
        layout.setFidelityMode(true);

      layout.addWidget(Zone::TopBar, &timePanel, 2.0f);
      layout.addWidget(Zone::TopBar, pane1.get(), 1.5f);
      layout.addWidget(Zone::TopBar, pane2.get(), 1.5f);
      layout.addWidget(Zone::TopBar, pane3.get(), 1.0f);
      layout.addWidget(Zone::TopBar, &bandCond, 1.0f);

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

      std::vector<Widget *> widgets = {&timePanel,  pane1.get(), pane2.get(),
                                       pane3.get(), &bandCond,   &localPanel,
                                       &dxSatPane,  &mapArea,    &rssBanner};

      // Build the overlay's actual-rect list from live widget positions.
      auto buildActuals = [&]() -> std::vector<WidgetRect> {
        return {
            {"TimePanel", timePanel.getRect()},
            {widgetTypeDisplayName(pane1Type), pane1->getRect()},
            {widgetTypeDisplayName(pane2Type), pane2->getRect()},
            {widgetTypeDisplayName(pane3Type), pane3->getRect()},
            {"Band Cond", bandCond.getRect()},
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

        for (auto *w : widgets) {
          SDL_Rect clip = w->getRect();
          SDL_RenderSetClipRect(renderer, &clip);
          w->render(renderer);
        }
        SDL_RenderSetClipRect(renderer, nullptr);

        if (debugOverlay.isVisible()) {
          debugOverlay.render(renderer, LOGICAL_WIDTH, LOGICAL_HEIGHT,
                              buildActuals());
          // Pane type indicators
          struct PaneLabel {
            Widget *w;
            WidgetType t;
          };
          PaneLabel paneLabels[] = {
              {pane1.get(), pane1Type},
              {pane2.get(), pane2Type},
              {pane3.get(), pane3Type},
          };
          for (const auto &pl : paneLabels) {
            SDL_Rect r = pl.w->getRect();
            fontMgr.drawText(renderer, widgetTypeToString(pl.t), r.x + 2,
                             r.y + 2, {255, 128, 0, 255}, 10);
          }
        }
        SDL_RenderPresent(renderer);

        if (FIDELITY_MODE) {
          SDL_RenderSetScale(renderer, 1.0f, 1.0f);
        }
      };

      // --- Dashboard Loop ---
      Uint32 lastResizeMs = 0; // debounce timer for font re-rasterization
      bool running = true;
      while (running) {
        // Ensure layout metrics are always up to date with actual window state
        // This fixes issues where Resize events might report stale or
        // intermediate sizes, or where the renderer output size lags behind the
        // window size.
        updateLayoutMetrics(window, renderer);

        SDL_Event event;
        while (SDL_PollEvent(&event)) {
          switch (event.type) {
          case SDL_QUIT:
            running = false;
            appRunning = false;
            break;
          case SDL_KEYDOWN: {
            bool consumed = false;
            for (auto *w : widgets) {
              if (w->onKeyDown(event.key.keysym.sym, event.key.keysym.mod)) {
                consumed = true;
                break;
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
          case SDL_TEXTINPUT:
            for (auto *w : widgets) {
              if (w->onTextInput(event.text.text))
                break;
            }
            break;
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
              for (auto *w : widgets) {
                if (w->onMouseUp(mx, my, SDL_GetModState()))
                  break;
              }
            }
            break;
          case SDL_MOUSEWHEEL:
            for (auto *w : widgets) {
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
          needSetup = true;
          running = false;
          break;
        }

        // Wire satellite predictor to map (nullptr when in DX mode)
        mapArea.setPredictor(dxSatPane.activePredictor());

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

        for (auto *w : widgets)
          w->update();

        renderFrame();
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
