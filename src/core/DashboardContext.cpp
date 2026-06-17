#include "DashboardContext.h"
#include "PaneRestrictions.h"
#include "../ui/RenderUtils.h"

#include "core/ActivityLocationManager.h"
#include "core/AuroraHistoryStore.h"
#include "core/BrightnessManager.h"
#include "core/CPUMonitor.h"
#include "core/CitiesManager.h"
#include "core/ConfigManager.h"
#include "core/DXClusterData.h"
#include "core/DatabaseManager.h"
#include "core/DisplayPower.h"
#include "core/FlareData.h"
#include "core/HamClockState.h"
#include "core/LiveSpotData.h"
#include "core/PrefixManager.h"
#include "core/RSSData.h"
#include "core/RigData.h"
#include "core/RotatorData.h"
#include "core/SatelliteManager.h"
#include "core/SolarData.h"
#include "core/SoundManager.h"
#include "core/WorkerService.h"
#include "ui/WidgetRegistry.h"

// Forward declaration — implemented in ui/WidgetDescriptions.cpp
void populateWidgetDescriptions();

#include "network/FrameCapture.h"
#include "network/NetworkManager.h"
#include "network/WebServer.h"
#include "services/ADIFProvider.h"
#include "services/ActivityProvider.h"
#include "services/AlertsProvider.h"
#include "services/AsteroidProvider.h"
#include "services/AuroraProvider.h"
#include "services/BME280Provider.h"
#include "services/RemoteEnvProvider.h"
#include "services/LTR329Provider.h"
#include "services/BandConditionsProvider.h"
#include "services/BeaconProvider.h"
#include "services/CallbookProvider.h"
#include "services/ContestProvider.h"
#include "services/DRAPProvider.h"
#include "services/DXClusterProvider.h"
#include "services/DstProvider.h"
#include "services/FlareProvider.h"
#include "services/FccProvider.h"
#include "services/OpenMeteoForecastProvider.h"
#include "services/GPSProvider.h"
#include "services/HistoryProvider.h"
#include "services/HurricaneProvider.h"
#include "services/IonosondeProvider.h"
#include "services/LightningProvider.h"
#include "services/LiveSpotProvider.h"
#include "services/LoTWActivityProvider.h"
#include "services/ClublogProvider.h"
#include "services/LoTWProvider.h"
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
#include "services/MQTTService.h"
#include "ui/ColorPicker.h"
#include "ui/ADIFPanel.h"
#include "ui/ActivityPanels.h"
#include "ui/AlertsPanel.h"
#include "ui/AsteroidPanel.h"
#include "ui/AuroraGraphPanel.h"
#include "ui/AuroraPanel.h"
#include "ui/BandConditionsPanel.h"
#include "ui/BeaconPanel.h"
#include "ui/CallbookPanel.h"
#include "ui/ClublogWantedPanel.h"
#include "ui/ClockAuxPanel.h"
#include "ui/ContestPanel.h"
#include "ui/CountdownPanel.h"
#include "ui/DRAPPanel.h"
#include "ui/DXClusterPanel.h"
#include "ui/DXInfo.h"
#include "ui/SatWidget.h"
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
#include "ui/LoTWSyncPanel.h"
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
#include "ui/WorldClockPanel.h"
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
#include "core/SpaceWeatherAlertData.h"
#include "services/SpaceWeatherAlertProvider.h"
#include "ui/SolarCyclePanel.h"
#include "services/LaunchProvider.h"
#include "services/NetLoggerProvider.h"
#include "core/NetLoggerStore.h"
#include "services/PowerwallProvider.h"
#include "core/PowerwallStore.h"
#include "core/KIndexHistoryData.h"
#include "core/SFIHistoryData.h"
#include "ui/KIndexAlertPanel.h"
#include "ui/SFITrendPanel.h"
#include "ui/GreylineWindowsPanel.h"
#include "ui/DXCCProgressPanel.h"
#include "ui/SpaceWeatherAlertsPanel.h"
#include "ui/NOAASpaceWxPanel.h"
#include "ui/BigClockPanel.h"
#include "ui/CallsignClock.h"
#include "ui/VoacapDeDxPanel.h"
#include "ui/WASPanel.h"
#include "ui/WACRadarPanel.h"
#include "ui/ZoneHeatmapPanel.h"
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

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#ifdef _WIN32
#include <winsock2.h>
#endif
#include <memory>
#if defined(__linux__) && !defined(__ANDROID__)
#include <malloc.h>
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

#include "DashboardContext.h"

using namespace HamClock;

// ── In-app help panel ──────────────────────────────────────────────────────────
namespace {

// Panel dimensions (logical pixels, 800×480 space)
static constexpr int kHelpPanelW  = 700;
static constexpr int kHelpPanelH  = 370;
static constexpr int kHelpPadX    = 8;
static constexpr int kHelpPadY    = 6;
static constexpr int kHelpViewH   = kHelpPanelH - kHelpPadY * 2 - 18; // minus footer
static constexpr int kHelpLineH   = 13; // logical pixel line height (Fast font ~12pt)

struct HelpPanelState {
  bool visible  = false;
  int  scrollY  = 0;
  int  contentH = 0; // total rendered height; computed on first render pass
};
HelpPanelState g_help;

} // namespace

// Forward declaration — defined in main.cpp (non-static, shared across TUs)
class DisplayPower;
void preventRPiSleep(bool prevent, DisplayPower *dp = nullptr);

AppContext::~AppContext() {
#ifndef __EMSCRIPTEN__
  // Stop the WebServer thread before any member destructors run.
  // dashboard (DashboardContext) is declared after webServer in AppContext, so it
  // would normally be destroyed first by reverse member-destruction order — leaving
  // the HTTP thread holding dangling raw pointers to timePanel_, rssBanner_,
  // adifProvider_, satMgr_, etc. that live inside DashboardContext.
  // Explicitly stopping here ensures the thread is joined before anything it
  // references can be freed.
  if (webServer)
    webServer->stop();
#endif
}

void AppContext::updateLayoutMetrics() {
  SDL_GetWindowSize(window, &globalWinW, &globalWinH);
  SDL_GetRendererOutputSize(renderer, &globalDrawW, &globalDrawH);

  if (FIDELITY_MODE) {
    float sw = static_cast<float>(globalDrawW) / LOGICAL_WIDTH;
    float sh = static_cast<float>(globalDrawH) / LOGICAL_HEIGHT;
    if (appCfg.scaleToFullScreen) {
      layScaleX = sw;
      layScaleY = sh;
      layLogicalOffX = 0;
      layLogicalOffY = 0;
    } else {
      float layScale = std::min(sw, sh);
      layScaleX = layScale;
      layScaleY = layScale;
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
    }
  } else {
    layScaleX = 1.0f;
    layScaleY = 1.0f;
    layLogicalOffX = 0;
    layLogicalOffY = 0;
  }
}

DashboardContext::DashboardContext(AppContext &ctx)
    : fontMgr(), texMgr(), fontCatalog(fontMgr), debugOverlay(fontMgr),
      satMgr(std::make_unique<SatelliteManager>(*ctx.netManager)),
      appContext_(ctx) {
  // Reset idle timer to now so the cursor-hide logic doesn't fire immediately
  lastMouseMotionMs = SDL_GetTicks();
  // Load font
  if (!fontMgr.loadFromMemory(assets_font_ttf, assets_font_ttf_len,
                              DEFAULT_FONT_SIZE)) {
    std::fprintf(stderr, "Warning: text rendering disabled\n");
  }
  fontMgr.setCatalog(&fontCatalog);

  // Populate per-widget help descriptions (safe: all REGISTER_WIDGET static
  // initializers have already run before the constructor executes).
  populateWidgetDescriptions();

  // Compute render scale
  int drawW, drawH;
  SDL_GetRendererOutputSize(ctx.renderer, &drawW, &drawH);
  float rs = static_cast<float>(drawH) / LOGICAL_HEIGHT;
  fontMgr.setRenderScale(rs);

  // In software mode textures live in system RAM; shrink the cache to avoid
  // accumulating 200MB of pixel buffers (default 50 × 4MB on ARM).
  {
    SDL_RendererInfo rinfo;
    if (SDL_GetRendererInfo(ctx.renderer, &rinfo) == 0 &&
        (rinfo.flags & SDL_RENDERER_SOFTWARE)) {
      texMgr.setMaxCacheSize(20);
    }
  }

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

  currentDxcSnapshot_ = dxcStore->snapshot();
  currentSpotSnapshot_ = spotStore->snapshot();
  lastDxcVer_ = dxcStore->getVersion();
  lastSpotVer_ = spotStore->getVersion();

  // Returns true if the widget appears in any pane rotation.
  // aurora_graph is always treated as configured: its history store
  // needs continuous sampling even when the pane is off-screen.
  auto isWidgetConfigured = [&](const std::string &typeId) -> bool {
    if (typeId == "aurora_graph")
      return true;
    for (const auto &t : appCfg.pane1Rotation) if (t == typeId) return true;
    for (const auto &t : appCfg.pane2Rotation) if (t == typeId) return true;
    for (const auto &t : appCfg.pane3Rotation) if (t == typeId) return true;
    for (const auto &t : appCfg.pane4Rotation) if (t == typeId) return true;
    for (const auto &t : appCfg.pane5Rotation) if (t == typeId) return true;
    for (const auto &t : appCfg.pane6Rotation) if (t == typeId) return true;
    return false;
  };
  const bool isMasterMode = (appCfg.hubMode == HubMode::Master);

  ctx.xrayHistoryStore = std::make_shared<XRayHistoryStore>();
  ctx.kIndexHistoryStore = std::make_shared<KIndexHistoryStore>();
  ctx.sfiHistoryStore = std::make_shared<SFIHistoryStore>();
  ctx.drapDataStore = std::make_shared<DRAPDataStore>();
  ctx.flareStore = std::make_shared<FlareDataStore>();
  ctx.heardMeStore = std::make_shared<HeardMeStore>();
  noaaProvider =
      std::make_unique<NOAAProvider>(netManager, solarStore, auroraHistoryStore,
                                     ctx.xrayHistoryStore, state.get());
  noaaProvider->setDrapStore(ctx.drapDataStore);
  noaaProvider->setAuroraMapStore(ctx.auroraMapStore);
  noaaProvider->setKIndexHistoryStore(ctx.kIndexHistoryStore);
  noaaProvider->setSFIHistoryStore(ctx.sfiHistoryStore);
  if (isMasterMode || isWidgetConfigured("solar") ||
      isWidgetConfigured("aurora") ||
      isWidgetConfigured("aurora_graph") ||
      isWidgetConfigured("drap") ||
      isWidgetConfigured("band_conditions") ||
      isWidgetConfigured("kindex_trend") ||
      isWidgetConfigured("sfi_trend") ||
      appCfg.propOverlay == PropOverlayType::Aurora)
    noaaProvider->fetch();
  if (appCfg.propOverlay == PropOverlayType::Drap)
    noaaProvider->fetchDRAP();
  if (appCfg.propOverlay == PropOverlayType::Aurora)
    noaaProvider->fetchAuroraMap();

  rssProvider = std::make_unique<RSSProvider>(netManager, rssStore);
  rssProvider->setCustomUrl(appCfg.rssUrl);
  rssProvider->fetch();

  spotProvider = std::make_unique<LiveSpotProvider>(
      netManager, spotStore, appCfg, ctx.prefixMgr, state.get(), dxcStore);
  if (isMasterMode || isWidgetConfigured("live_spots") ||
      appCfg.propOverlay != PropOverlayType::None)
    spotProvider->fetch();

#ifndef __EMSCRIPTEN__
  rotatorService =
      std::make_unique<RotatorService>(rotatorStore, appCfg, state.get());
  rotatorService->start();
  rigService = std::make_unique<RigService>(rigStore, appCfg, state);
  rigService->start();
#endif

  satMgr->fetch();

  launchProvider = std::make_shared<LaunchProvider>(netManager);
  launchProvider->fetch();

#ifndef __EMSCRIPTEN__
  satMgr->setRotatorService(rotatorService.get());
#endif
  satMgr->setObserver(appCfg.lat, appCfg.lon);

  activityProvider = std::make_unique<ActivityProvider>(
      netManager, activityStore, ctx.prefixMgr);
  activityProvider->fetch();

  // Re-fetch POTA spots once the parks CSV is parsed so spots get coordinates.
  // The CSV is read/parsed in a background thread; spots often arrive first.
  ActivityLocationManager::getInstance().setOnParksReady(
      [&ctx, self = this, liveFlag = parksReadyLive_]() {
        // Check the atomic flag first (set in ~DashboardContext) to avoid
        // racing on ctx.dashboard.get() from this background thread.
        if (!liveFlag->load(std::memory_order_acquire))
          return;
        if (ctx.dashboard.get() == self && self->activityProvider)
          self->activityProvider->fetch();
      });

  dxcProvider = std::make_unique<DXClusterProvider>(
      dxcStore, ctx.prefixMgr, watchlistStore, watchlistHitStore, state.get(),
      ctx.adifStore);
#ifndef __EMSCRIPTEN__
  if (isMasterMode || isWidgetConfigured("dx_cluster") || isWidgetConfigured("watchlist"))
    dxcProvider->start(appCfg);
#endif

  rbnProvider =
      std::make_unique<RBNProvider>(dxcStore, ctx.prefixMgr, watchlistStore,
                                    watchlistHitStore, ctx.heardMeStore,
                                    state.get());
#ifndef __EMSCRIPTEN__
  if ((isMasterMode || isWidgetConfigured("dx_cluster") ||
       isWidgetConfigured("watchlist")) &&
      appCfg.rbnEnabled)
    rbnProvider->start(appCfg);
#endif

  bandProvider =
      std::make_unique<BandConditionsProvider>(solarStore, bandStore);
  bandProvider->update();

  contestProvider = std::make_unique<ContestProvider>(netManager, contestStore);
  contestProvider->fetch();

  flareProvider = std::make_unique<FlareProvider>(netManager, ctx.flareStore);
  if (isMasterMode || isWidgetConfigured("flare_log"))
    flareProvider->fetch();

  moonProvider = std::make_unique<MoonProvider>(netManager, moonStore);
  moonProvider->update(appCfg.lat, appCfg.lon);

  historyProvider = std::make_unique<HistoryProvider>(netManager, historyStore);
  if (isMasterMode || isWidgetConfigured("history_flux"))
    historyProvider->fetchFlux();
  if (isMasterMode || isWidgetConfigured("history_ssn") ||
      isWidgetConfigured("solar_cycle"))
    historyProvider->fetchSSN();
  if (isMasterMode || isWidgetConfigured("history_kp"))
    historyProvider->fetchKp();

  deWeatherProvider =
      std::make_unique<WeatherProvider>(netManager, deWeatherStore, 0);
  if (state->deLocation.lat != 0.0 || state->deLocation.lon != 0.0) {
    deWeatherProvider->fetch(state->deLocation.lat, state->deLocation.lon);
  }

  dxWeatherProvider =
      std::make_unique<WeatherProvider>(netManager, dxWeatherStore, 1);
  if (state->dxLocation.lat != 0.0 || state->dxLocation.lon != 0.0) {
    dxWeatherProvider->fetch(state->dxLocation.lat, state->dxLocation.lon);
  }

  sdoProvider = std::make_shared<SDOProvider>(netManager);
  drapProvider = std::make_unique<DRAPProvider>(netManager, ctx.drapDataStore);
  auroraProvider = std::make_shared<AuroraProvider>(netManager);

  callbookProvider =
      std::make_shared<CallbookProvider>(netManager, callbookStore);

  dstProvider = std::make_shared<DstProvider>(netManager, dstStore);
  if (isMasterMode || isWidgetConfigured("dst_index"))
    dstProvider->fetch();

  adifProvider = std::make_unique<ADIFProvider>(adifStore, ctx.prefixMgr);
  adifProvider->fetch(ctx.cfgMgr.configDir() / "logs.adif");
#ifndef __EMSCRIPTEN__
  if (ctx.webServer) {
    ctx.webServer->setADIFProvider(adifProvider.get());
    ctx.webServer->setRssProvider(rssProvider.get());
    ctx.webServer->setRssBanner(rssBanner.get());
  }
#endif

  lotwActivityProvider = std::make_shared<LoTWActivityProvider>(
      netManager, ctx.lotwActivityStore);
  lotwActivityProvider->fetch();

  clublogProvider = std::make_shared<ClublogProvider>(
      netManager, ctx.clublogStore, appCfg.clublogApiKey);
  if (!appCfg.clublogApiKey.empty())
    clublogProvider->fetch();

  lotwProvider = std::make_shared<LoTWProvider>(
      netManager, ctx.adifStore, ctx.prefixMgr, appCfg.lotwCall, appCfg.lotwPassword);
  if (!appCfg.lotwCall.empty() && !appCfg.lotwPassword.empty())
    lotwProvider->fetch();

  mufRtProvider = std::make_shared<MufRtProvider>(netManager);
  mufRtProvider->update();



  appContext_.asteroidProvider = std::make_shared<AsteroidProvider>(netManager);
  appContext_.asteroidProvider->update();

  beaconProvider = std::make_unique<BeaconProvider>();

  alertsProvider =
      std::make_unique<AlertsProvider>(netManager, ctx.alertsStore);
  if (isMasterMode || isWidgetConfigured("alerts"))
    alertsProvider->fetch(appCfg.lat, appCfg.lon);

  forecastProvider =
      std::make_shared<OpenMeteoForecastProvider>(netManager, ctx.forecastStore);
  if (isMasterMode || isWidgetConfigured("forecast"))
    forecastProvider->fetch(appCfg.lat, appCfg.lon);

  repeaterProvider =
      std::make_unique<RepeaterProvider>(netManager, ctx.repeaterStore);
  // RepeaterBook API requires auth key — fetch disabled until configured.
  // repeaterProvider->fetch(appCfg.lat, appCfg.lon);

  hurricaneProvider =
      std::make_unique<HurricaneProvider>(netManager, ctx.hurricaneStore);
  if (isMasterMode || isWidgetConfigured("hurricane"))
    hurricaneProvider->fetch();

  marineProvider =
      std::make_unique<MarineProvider>(netManager, ctx.marineStore, state.get());
  if (isMasterMode || isWidgetConfigured("marine"))
    marineProvider->fetch(appCfg.marineStation, appCfg.marineBuoy);

  winlinkProvider =
      std::make_unique<WinlinkProvider>(netManager, ctx.winlinkStore);
  // Winlink API requires access key — fetch disabled until configured.
  // winlinkProvider->fetch(appCfg.lat, appCfg.lon);

  greylineDXProvider =
      std::make_unique<GreylineDXProvider>(ctx.prefixMgr, ctx.greylineDXStore);
  greylineDXProvider->update();

  appContext_.spaceWxAlertStore = std::make_shared<SpaceWeatherAlertStore>();
  spaceWeatherAlertProvider =
      std::make_unique<SpaceWeatherAlertProvider>(netManager, appContext_.spaceWxAlertStore);
  if (isMasterMode || isWidgetConfigured("spacewx_alerts"))
    spaceWeatherAlertProvider->fetch();
  ctx.netLoggerStore = std::make_shared<NetLoggerStore>();
  netLoggerProvider = std::make_unique<NetLoggerProvider>(netManager, ctx.netLoggerStore);
  if (isMasterMode || isWidgetConfigured("netlogger"))
    netLoggerProvider->fetch();

  ctx.powerwallStore = std::make_shared<PowerwallStore>();
  powerwallProvider = std::make_unique<PowerwallProvider>(netManager, ctx.powerwallStore, ctx.appCfg);
  if (isMasterMode || isWidgetConfigured("powerwall"))
    powerwallProvider->fetch();

  santaProvider = std::make_unique<SantaProvider>(santaStore);
  santaProvider->update();

  tropoProvider = std::make_unique<TropoProvider>(netManager);
  tropoProvider->setCallback([&ctx, self = this, live = dashboardLive_](const TropoData &d) {
    if (live->load(std::memory_order_acquire) &&
        ctx.dashboard.get() == self &&
        self->widgetPool.count("tropo")) {
      static_cast<TropoPanel *>(self->widgetPool["tropo"].get())
          ->updateData(d);
    }
  });

  lightningProvider = std::make_shared<LightningProvider>(netManager);
  lightningProvider->setCallback([&ctx, self = this, live = dashboardLive_](const LightningData &d) {
    if (live->load(std::memory_order_acquire) &&
        ctx.dashboard.get() == self &&
        self->widgetPool.count("lightning")) {
      static_cast<LightningPanel *>(
          self->widgetPool["lightning"].get())
          ->updateData(d);
    }
  });

  meteorProvider = std::make_unique<MeteorProvider>();
  meteorProvider->setCallback([&ctx, self = this, live = dashboardLive_](const MeteorData &d) {
    if (live->load(std::memory_order_acquire) &&
        ctx.dashboard.get() == self &&
        self->widgetPool.count("meteor")) {
      static_cast<MeteorPanel *>(self->widgetPool["meteor"].get())
          ->updateData(d);
    }
  });

  solarStormProvider = std::make_shared<SolarStormProvider>(netManager);
  solarStormProvider->setCallback(
      [&ctx, self = this, live = dashboardLive_](const SolarStormData &d) {
        if (live->load(std::memory_order_acquire) &&
            ctx.dashboard.get() == self &&
            self->widgetPool.count("solar_storm")) {
          static_cast<SolarStormPanel *>(
              self->widgetPool["solar_storm"].get())
              ->updateData(d);
        }
      });

  ionosondeProvider = std::make_shared<IonosondeProvider>(netManager);
  ionosondeProvider->setCallback([&ctx, self = this, live = dashboardLive_](const IonosondeData &d) {
    if (live->load(std::memory_order_acquire) &&
        ctx.dashboard.get() == self &&
        self->widgetPool.count("ionosonde")) {
      static_cast<IonosondePanel *>(
          self->widgetPool["ionosonde"].get())
          ->updateData(d);
    }
  });

  reachProvider = std::make_shared<ReachProvider>(netManager, state, spotStore);
  reachProvider->setCallback([&ctx, self = this, live = dashboardLive_](const ReachData &d) {
    // Push SDL event so onPropDataReady runs on the main/render thread,
    // not the network worker thread where SDL_GL_GetCurrentWindow() is NULL.
    if (live->load(std::memory_order_acquire) &&
        ctx.dashboard.get() == self && self->mapArea) {
      auto *result = new std::vector<float>(d.grid);
      SDL_Event event;
      SDL_zero(event);
      event.type = HamClock::AE_BASE_EVENT + HamClock::AE_PROP_DATA_READY;
      event.user.code = static_cast<int>(PropOverlayType::Heatmap);
      event.user.data1 = result;
      if (SDL_PushEvent(&event) < 0)
        delete result;
    }
  });

  timePanel =
      std::make_unique<TimePanel>(0, 0, 0, 0, fontMgr, texMgr, appCfg);
  timePanel->setCallColor(appCfg.callsignColor);
  timePanel->setCallBgColor(appCfg.callsignBgColor);
  timePanel->setRigDataStore(ctx.rigStore.get());
  timePanel->setOnConfigChanged(
      [&ctx](const std::string &call, SDL_Color fgColor, SDL_Color bgColor) {
        ctx.appCfg.callsign = call;
        ctx.appCfg.callsignColor = fgColor;
        ctx.appCfg.callsignBgColor = bgColor;
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
  widgetFactory_ = [this, &ctx](const std::string &type) -> Widget * {
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
    auto kIndexHistoryStore = ctx.kIndexHistoryStore;
    auto sfiHistoryStore = ctx.sfiHistoryStore;
    if (widgetPool.count(type) && widgetPool[type])
      return widgetPool[type].get();

    WidgetDeps deps{
        fontMgr,
        texMgr,
        appCfg,
        ctx.cfgMgr,
        netManager,
        ctx.prefixMgr,
        state,
        ctx.cpuMonitor,
        solarStore,
        auroraHistoryStore,
        watchlistStore,
        watchlistHitStore,
        spotStore,
        activityStore,
        dxcStore,
        bandStore,
        contestStore,
        moonStore,
        historyStore,
        deWeatherStore,
        dxWeatherStore,
        callbookStore,
        dstStore,
        adifStore,
        santaStore,
        rotatorStore,
        ctx.rigStore,
        ctx.alertsStore,
        ctx.forecastStore,
        ctx.repeaterStore,
        ctx.hurricaneStore,
        ctx.marineStore,
        ctx.winlinkStore,
        ctx.powerwallStore,
        ctx.drapDataStore,
        ctx.xrayHistoryStore,
        kIndexHistoryStore,
        sfiHistoryStore,
        ctx.greylineDXStore,
        ctx.auroraMapStore,
        ctx.calendarStore,
        appContext_.spaceWxAlertStore,
        ctx.lotwActivityStore,
        ctx.clublogStore,
        ctx.flareStore,
        ctx.heardMeStore,
        ctx.netLoggerStore,
        spotProvider.get(),
        activityProvider.get(),
        drapProvider.get(),
        auroraProvider.get(),
        sdoProvider.get(),
        beaconProvider.get(),
        satMgr.get(),
        appContext_.asteroidProvider.get(),
        callbookProvider,
        ionosondeProvider,
        &fccProvider,
#ifndef __EMSCRIPTEN__
        rigService.get(),
#else
        nullptr,
#endif
        marineProvider.get(),
        dxWeatherProvider.get(),
        launchProvider.get(),
    };
    widgetPool[type] = WidgetRegistry::instance().create(type, deps);

    // Wire callbacks for newly created widgets
    if (type == "dx_cluster") {
      auto *dxcPanel = dynamic_cast<DXClusterPanel *>(widgetPool[type].get());
      if (dxcPanel) {
        dxcPanel->setOnSpotActivated(
            [state, activityStore, &appCfg](const DXClusterSpot &spot) {
              state->dxCallsign = spot.txCall;
              state->dxLocation = {spot.txLat, spot.txLon};
              state->dxGrid = spot.txGrid;
              state->dxFreqKhz = spot.freqKhz;
              state->dxActive = (spot.txLat != 0.0 || spot.txLon != 0.0);
              ActivityData ad = *activityStore->get();
              ad.hasSelection = false;
              activityStore->set(ad);
              if (appCfg.propOverlay != PropOverlayType::None) {
                int bi = freqToBandIndex(spot.freqKhz);
                if (bi >= 0) {
                  static constexpr const char *kPropBands[] = {
                      "80m","60m","40m","30m","20m","17m","15m","12m","10m","6m"
                  };
                  const std::string &name = kBands[bi].name;
                  for (auto *b : kPropBands)
                    if (name == b) { appCfg.propBand = name; break; }
                }
              }
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
          state->dxFreqKhz = 0.0;
        });
      }
    } else if (type == "on_the_air") {
      auto *ontaPanel = dynamic_cast<ONTAPanel *>(widgetPool[type].get());
      if (ontaPanel) {
        ontaPanel->setOnSpotActivated([state, dxcStore, &appCfg](const ONTASpot &spot) {
          state->dxCallsign = spot.call;
          state->dxLocation = {spot.lat, spot.lon};
          state->dxGrid = (spot.lat != 0.0 || spot.lon != 0.0)
                              ? Astronomy::latLonToGrid(spot.lat, spot.lon)
                              : "";
          state->dxFreqKhz = spot.freqKhz;
          state->dxActive = (spot.lat != 0.0 || spot.lon != 0.0);
          dxcStore->clearSelection();
          if (appCfg.propOverlay != PropOverlayType::None) {
            int bi = freqToBandIndex(spot.freqKhz);
            if (bi >= 0) {
              static constexpr const char *kPropBands[] = {
                  "80m","60m","40m","30m","20m","17m","15m","12m","10m","6m"
              };
              const std::string &name = kBands[bi].name;
              for (auto *b : kPropBands)
                if (name == b) { appCfg.propBand = name; break; }
            }
          }
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
          state->dxFreqKhz = 0.0;
        });
      }
    } else if (type == "dx_peditions") {
      auto *dxpPanel = dynamic_cast<DXPedPanel *>(widgetPool[type].get());
      if (dxpPanel) {
        dxpPanel->setOnPeditionActivated(
            [state, dxcStore](const DXPedition &p) {
              state->dxCallsign = p.call;
              state->dxLocation = {p.lat, p.lon};
              state->dxGrid = Astronomy::latLonToGrid(p.lat, p.lon);
              state->dxStartTime = p.startTime;
              state->dxEndTime = p.endTime;
              state->dxActive = true;
              dxcStore->clearSelection();
            });
        dxpPanel->setOnPeditionDeactivated([state]() {
          if (state->mapDxActive) {
            state->dxLocation = state->mapDxLocation;
            state->dxGrid = state->mapDxGrid;
            state->dxActive = true;
          } else {
            state->dxActive = false;
          }
          state->dxCallsign.clear();
          state->dxStartTime = {};
          state->dxEndTime = {};
        });
      }
    } else if (type == "live_spots") {
      auto *lsPanel = dynamic_cast<LiveSpotPanel *>(widgetPool[type].get());
      if (lsPanel) {
        lsPanel->setOnBandSelected([state, &appCfg](int bandIdx) {
          if (bandIdx < 0 || bandIdx >= kNumBands)
            return;
          state->dxFreqKhz =
              (kBands[bandIdx].minKhz + kBands[bandIdx].maxKhz) / 2.0;
          if (appCfg.propOverlay != PropOverlayType::None) {
            static constexpr const char *kPropBands[] = {
                "80m","60m","40m","30m","20m","17m","15m","12m","10m","6m"
            };
            const std::string &name = kBands[bandIdx].name;
            for (auto *b : kPropBands)
              if (name == b) { appCfg.propBand = name; break; }
          }
        });
      }
    } else if (type == "lotw_sync") {
      auto *syncPanel = dynamic_cast<LoTWSyncPanel *>(widgetPool[type].get());
      if (syncPanel && lotwProvider) {
        syncPanel->setupProvider(lotwProvider.get());
      }
    }

    if (widgetPool[type]) {
      widgetPool[type]->setTheme(appCfg.theme);
      widgetPool[type]->setMetric(appCfg.useMetric);
    }

    return widgetPool[type].get();
  };

  std::vector<std::string> allTypes;
  for (auto *d : WidgetRegistry::instance().getAll(false))
    allTypes.push_back(d->typeId);

  if (!appCfg.repeaterBookKey.empty())
    allTypes.push_back("repeater_dir");
  if (!appCfg.winlinkKey.empty())
    allTypes.push_back("winlink");
  if (!appCfg.rigHost.empty() && appCfg.rigPort != 0)
    allTypes.push_back("rig_control");

  // Callback wiring moved to getOrAddWidget for lazy compatibility

  for (int i = 0; i < 6; ++i) {
    panes.push_back(std::make_unique<PaneContainer>(
        0, 0, 0, 0, "solar", fontMgr));
    // Capture [this] — widgetFactory_ is a member, so this is safe post-ctor.
    panes.back()->setWidgetFactory(
        [this](const std::string &t) { return widgetFactory_(t); });
  }

  // Scrub configuration to ensure restricted panes (like Pane 4) don't have prohibited widgets.
  auto scrubRotation = [](int paneIdx, std::vector<std::string> &rot) {
    auto allowed = PaneRestrictions::getAllowedWidgets(paneIdx);
    if (allowed.empty())
      return;
    auto it = std::remove_if(rot.begin(), rot.end(), [&](const std::string &t) {
      return std::find(allowed.begin(), allowed.end(), t) == allowed.end();
    });
    if (it != rot.end()) {
      rot.erase(it, rot.end());
      if (rot.empty())
        rot.push_back("solar");
    }
  };
  scrubRotation(0, appCfg.pane1Rotation);
  scrubRotation(1, appCfg.pane2Rotation);
  scrubRotation(2, appCfg.pane3Rotation);
  scrubRotation(3, appCfg.pane4Rotation);
  scrubRotation(4, appCfg.pane5Rotation);
  scrubRotation(5, appCfg.pane6Rotation);

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
    std::vector<std::string> available = allTypes;
    if (!ctx.bmeProvider->isAvailable()) {
      available.erase(std::remove_if(available.begin(), available.end(),
                                     [](const std::string &t) {
                                       return t == "env_temp" ||
                                              t == "env_pressure" ||
                                              t == "env_humidity" ||
                                              t == "env_dewpoint";
                                     }),
                      available.end());
    }
    auto allowedForPane = PaneRestrictions::getAllowedWidgets(paneIdx);
    if (!allowedForPane.empty()) {
      available = allowedForPane;
    }
    std::vector<std::string> current = panes[paneIdx]->getRotation();
    std::vector<std::string> forbidden;
    for (int i = 0; i < 6; ++i) {
      if (i == paneIdx)
        continue;
      for (const auto &t : panes[i]->getRotation()) {
        forbidden.push_back(t);
      }
    }
    bool isFullHeight = false;
    if (paneIdx == 4 || paneIdx == 5) {
      isFullHeight = panes[5]->getRotation().empty();
      if (isFullHeight && !panes[4]->getRotation().empty()) {
        for (const auto &t : panes[4]->getRotation()) {
          auto *d = WidgetRegistry::instance().find(t);
          if (!d || !d->isScrollable) {
            isFullHeight = false;
            break;
          }
        }
      }
    }

    widgetSelector->show(
        paneIdx, available, current, forbidden,
        [&ctx, this, isFullHeight](int idx, const std::vector<std::string> &finalSelection, bool fullHeightSelected) {
          int targetIdx = idx;
          if (fullHeightSelected) {
            targetIdx = 4; // Always use pane 5 for full height widgets
            panes[5]->setRotation({}, ctx.appCfg.rotationIntervalS, ctx.appCfg.syncRotation);
          } else if (isFullHeight) {
            // Transitioning out of full-height: restore pane 6 with a default
            // widget so it becomes visible and clickable in the split layout
            panes[5]->setRotation({"solar"}, ctx.appCfg.rotationIntervalS, ctx.appCfg.syncRotation);
          }
          panes[targetIdx]->setRotation(finalSelection, ctx.appCfg.rotationIntervalS,
                                  ctx.appCfg.syncRotation);
                                  
          // When pane 6 occupancy changes, update side-panel layout
          bool pane6Empty = panes[5]->getRotation().empty();
          layout.removeWidget(panes[5].get());
          if (!pane6Empty) {
            layout.addWidget(Zone::SidePanel, panes[5].get());
          } else {
            panes[5]->onResize(0, 0, 0, 0);
          }
          if (FIDELITY_MODE)
            layout.recalculate(LOGICAL_WIDTH, LOGICAL_HEIGHT,
                                ctx.layLogicalOffX, ctx.layLogicalOffY);
          else
            layout.recalculate(ctx.globalWinW, ctx.globalWinH);

          ctx.appCfg.pane1Rotation = panes[0]->getRotation();
          ctx.appCfg.pane2Rotation = panes[1]->getRotation();
          ctx.appCfg.pane3Rotation = panes[2]->getRotation();
          ctx.appCfg.pane4Rotation = panes[3]->getRotation();
          ctx.appCfg.pane5Rotation = panes[4]->getRotation();
          ctx.appCfg.pane6Rotation = panes[5]->getRotation();
          ctx.cfgMgr.save(ctx.appCfg);
        }, false, isFullHeight);
  };
  for (int i = 0; i < 6; ++i) {
    panes[i]->setOnSelectionRequested(onPaneSelectionRequested, i);
    panes[i]->setOnMaximizeRequested([this, &ctx](int idx) {
      if (expandedPaneIdx_ == idx)
        restorePane(ctx);
      else
        expandPane(idx, ctx);
    });
    panes[i]->setLineAATexture(texMgr.get("line_aa"));
  }

  mapArea = std::make_unique<MapWidget>(0, 0, 0, 0, texMgr, fontMgr, netManager,
                                        state, appCfg);
  // Share ownership of WxMbProvider so its async callbacks are safe if
  // dashboard resets.
  wxMbProvider = mapArea->getWxMbProvider();

  qrzProvider = std::make_shared<QRZProvider>(netManager);
  qrzProvider->setCredentials(appCfg.qrzUsername, appCfg.qrzPassword);
  mapArea->setOnConfigChanged([&ctx] { ctx.cfgMgr.save(ctx.appCfg); });
  mapArea->setSpotStore(spotStore);
  mapArea->setDXClusterStore(dxcStore);
  mapArea->setHeardMeStore(ctx.heardMeStore);
  mapArea->setADIFStore(adifStore);
  mapArea->setMufRtProvider(mufRtProvider.get());
  mapArea->setBeaconProvider(beaconProvider.get());
  mapArea->setAuroraStore(auroraHistoryStore);
  mapArea->setAuroraMapStore(auroraMapStore);
  mapArea->setDrapStore(ctx.drapDataStore);
  mapArea->setIonosondeProvider(ionosondeProvider);
  mapArea->setSolarDataStore(ctx.solarStore.get());
  mapArea->setActivityStore(ctx.activityStore);
  mapArea->setNetLoggerStore(ctx.netLoggerStore);
  mapArea->setLaunchProvider(launchProvider.get());

  std::vector<PaneContainer *> panePtrs;
  for (const auto &p : panes)
    panePtrs.push_back(p.get());
  mapArea->setPanes(panePtrs);

  // Wire predictor from standalone satellite widget to map
  auto *satWidget = dynamic_cast<SatWidget *>(widgetFactory_("satellite"));
  if (satWidget) {
    mapArea->setPredictor(satWidget->activePredictor());
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
  if (layout.fidelityMode() != FIDELITY_MODE)
    layout.setFidelityMode(FIDELITY_MODE);
  layout.setGlassHudMode(ctx.appCfg.glassHudMode);
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
  widgetSelector->setLineAATexture(texMgr.get("line_aa"));
  for (auto &p : panes) {
    p->setTheme(appCfg.theme);
    p->setMetric(appCfg.useMetric);
    p->setLineAATexture(texMgr.get("line_aa"));
  }
  timePanel->setLineAATexture(texMgr.get("line_aa"));
  mapArea->setLineAATexture(texMgr.get("line_aa"));
  rssBanner->setLineAATexture(texMgr.get("line_aa"));

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
      fontMgr.setVolatileCacheLimit(100);
      fontMgr.setTextCacheLimit(100);
      LOG_I("Main", "Low-memory device: capping texture cache to 15, font cache to 100");
    } else {
      texMgr.setMaxCacheSize(50);
    }

    if (isMasterMode && memMon.isLowMemoryDevice()) {
      LOG_W("Hub", "Hub master on low-memory device ({:.0f} MB) — recommend 2+ GB",
            memMon.getTotalRAM() / 1024.0 / 1024.0);
      mapArea->setStartupBanner(
          "WARNING: Hub master on low-memory device. Stability not guaranteed.",
          30000);
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

  // Propagate theme and metric to all dashboard widgets
  std::string startTheme = appCfg.glassHudMode ? "glass" : appCfg.theme;
  for (auto *w : widgets) {
    if (w) {
      w->setTheme(startTheme);
      w->setMetric(appCfg.useMetric);
      w->setLineAATexture(texMgr.get("line_aa"));
    }
  }

  // Initial layout calculation
  fontCatalog.recalculate(LOGICAL_WIDTH, LOGICAL_HEIGHT);
  layout.recalculate(LOGICAL_WIDTH, LOGICAL_HEIGHT, ctx.layLogicalOffX,
                     ctx.layLogicalOffY);
  rssBanner->onResize(139 + ctx.layLogicalOffX, 412 + ctx.layLogicalOffY, 660,
                      68);
}

DashboardContext::~DashboardContext() {
  // Signal background callbacks to exit early rather than race on
  // ctx.dashboard.get() after this object is freed.
  parksReadyLive_->store(false, std::memory_order_release);
  dashboardLive_->store(false, std::memory_order_release);
#ifndef __EMSCRIPTEN__
  // Null out pointers in WebServer to prevent UAF from HTTP route handlers
  if (appContext_.webServer) {
    appContext_.webServer->setPanes(nullptr);
    appContext_.webServer->setTimePanel(nullptr);
    appContext_.webServer->setRssBanner(nullptr);
    appContext_.webServer->setSatelliteManager(nullptr);
    appContext_.webServer->setRotatorService(nullptr);
    appContext_.webServer->setADIFProvider(nullptr);
    appContext_.webServer->setRssProvider(nullptr);
    appContext_.webServer->setBMEProvider(nullptr);
    appContext_.webServer->setFrameCapture(nullptr);
  }

  if (dxcProvider)
    dxcProvider->stop();
  if (rbnProvider)
    rbnProvider->stop();
  if (rigService)
    rigService->stop();
  if (satMgr)
    satMgr->setRotatorService(nullptr); // prevent UAF: satMgr callbacks must not call rotator_ after stop
  if (rotatorService)
    rotatorService->stop();
#endif

  // Null out observer pointers in UI components to prevent dangling pointers
  // if components are re-created or dashboard is partially torn down.
  if (mapArea) {
    mapArea->setPredictor(nullptr);
    mapArea->setAsteroidProvider(nullptr);
    mapArea->setMufRtProvider(nullptr);
    mapArea->setBeaconProvider(nullptr);
    mapArea->setSolarDataStore(nullptr);
    mapArea->setPanes({});
  }
  if (timePanel) {
    timePanel->setRigDataStore(nullptr);
  }
  for (auto &p : panes) {
    if (p) {
      p->setLineAATexture(nullptr);
    }
  }
}

void DashboardContext::applySidePanelMode(const std::string &chosen, AppContext &ctx) {
  std::vector<std::string> p5 = {chosen};
  std::vector<std::string> p6 =
      (chosen == "de_info")
          ? std::vector<std::string>{"dx_info"}
          : std::vector<std::string>{};
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

void DashboardContext::expandPane(int idx, AppContext &ctx) {
  if (expandedPaneIdx_ >= 0 || idx < 0 || idx >= (int)panes.size())
    return;
  expandedPaneIdx_ = idx;
  SDL_Rect targetRect = mapArea->getRect();
  if (panes[idx]->getActiveType() == "big_clock") {
    targetRect = {ctx.layLogicalOffX, ctx.layLogicalOffY, LOGICAL_WIDTH, LOGICAL_HEIGHT};
  }
  panes[idx]->onResize(targetRect.x, targetRect.y, targetRect.w, targetRect.h);
  panes[idx]->setExpanded(true);
  panes[idx]->setPaused(true);
}

void DashboardContext::restorePane(AppContext &ctx) {
  if (expandedPaneIdx_ < 0)
    return;
  panes[expandedPaneIdx_]->setExpanded(false);
  panes[expandedPaneIdx_]->setPaused(false);
  expandedPaneIdx_ = -1;
  if (FIDELITY_MODE)
    layout.recalculate(LOGICAL_WIDTH, LOGICAL_HEIGHT, ctx.layLogicalOffX,
                       ctx.layLogicalOffY);
  else
    layout.recalculate(ctx.globalWinW, ctx.globalWinH);
}

// Compute integer UTC offset for an IANA timezone ID.
// Linux/macOS: uses POSIX TZ env-var + localtime (DST-aware), serialised by a
// static mutex so it is safe to call from the NetworkManager callback thread.
// WASM / Windows: falls back to longitude-based solar time (every 15° ≈ 1 hr).
static int tzIdToOffset(const std::string &tzId, double fallbackLon) {
#if defined(__EMSCRIPTEN__) || defined(_WIN32)
  int off = static_cast<int>(std::round(fallbackLon / 15.0));
  return std::clamp(off, -12, 14);
#else
  static std::mutex gTzMutex;
  std::lock_guard<std::mutex> lock(gTzMutex);
  const char *saved = getenv("TZ");
  std::string savedStr = saved ? saved : "";
  setenv("TZ", tzId.c_str(), 1);
  tzset();
  std::time_t now = std::time(nullptr);
  struct tm local{};
  Astronomy::portable_localtime(&now, &local);
  int off = static_cast<int>(Astronomy::portable_utcoffset(&now, &local) / 3600);
  if (!savedStr.empty())
    setenv("TZ", savedStr.c_str(), 1);
  else
    unsetenv("TZ");
  tzset();
  return off;
#endif
}

void DashboardContext::update(AppContext &ctx) {
  // Publish MQTT state
  MQTTService::getInstance().publishState(ctx);

  auto &appCfg = ctx.appCfg;

  ctx.updateLayoutMetrics();

  Uint32 now = SDL_GetTicks();

  // If display is off (software sleep), skip updates and logic
  bool isPowerOn = ctx.displayPower->getPower();

  // 1. Throttle data store snapshots to avoid per-frame deep copies
  if (isPowerOn) {
    uint32_t dVer = ctx.dxcStore->getVersion();
    if (dVer != lastDxcVer_ || !currentDxcSnapshot_) {
      currentDxcSnapshot_ = ctx.dxcStore->snapshot();
      lastDxcVer_ = dVer;
    }
    uint32_t sVer = ctx.spotStore->getVersion();
    if (sVer != lastSpotVer_ || !currentSpotSnapshot_) {
      currentSpotSnapshot_ = ctx.spotStore->snapshot();
      lastSpotVer_ = sVer;
    }
  }

  // Background refresh every 15 minutes, but only if power is on
  auto isWidgetActive = [&](const std::string &typeId) {
    for (auto &p : panes) {
      if (p->getActiveType() == typeId)
        return true;
    }
    return false;
  };
  auto isWidgetConfigured = [&](const std::string &typeId) -> bool {
    if (typeId == "aurora_graph")
      return true;
    for (const auto &t : appCfg.pane1Rotation) if (t == typeId) return true;
    for (const auto &t : appCfg.pane2Rotation) if (t == typeId) return true;
    for (const auto &t : appCfg.pane3Rotation) if (t == typeId) return true;
    for (const auto &t : appCfg.pane4Rotation) if (t == typeId) return true;
    for (const auto &t : appCfg.pane5Rotation) if (t == typeId) return true;
    for (const auto &t : appCfg.pane6Rotation) if (t == typeId) return true;
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
    const bool needsNoaa = isMaster || isWidgetActive("solar") ||
                           isWidgetActive("aurora") ||
                           isWidgetConfigured("aurora_graph") ||
                           isWidgetActive("drap") ||
                           isWidgetActive("noaa_spacewx") ||
                           isWidgetActive("band_conditions") ||
                           isWidgetActive("kindex_trend") ||
                           isWidgetActive("sfi_trend") ||
                           appCfg.propOverlay == PropOverlayType::Drap;
    if (needsNoaa)
      noaaProvider->fetch();

    // --- RSS news banner ---
    if (appCfg.rssEnabled)
      rssProvider->fetch();

    // --- Satellite manager ---
    // Feeds: SatPanel (in DXSatPane), EME planning tool, map track overlay
    if (isMaster || isWidgetActive("eme_tool") || appCfg.showSatTrack)
      satMgr->fetch();

    // --- Weather providers ---
    if ((isMaster || isWidgetActive("de_weather")) &&
        (ctx.state->deLocation.lat != 0.0 || ctx.state->deLocation.lon != 0.0))
      deWeatherProvider->fetch(ctx.state->deLocation.lat,
                               ctx.state->deLocation.lon);
    if ((isMaster || isWidgetActive("dx_weather")) &&
        (ctx.state->dxLocation.lat != 0.0 || ctx.state->dxLocation.lon != 0.0))
      dxWeatherProvider->fetch(ctx.state->dxLocation.lat,
                               ctx.state->dxLocation.lon);

    // --- Context-sensitive fetches (existing gating preserved/unchanged) ---
    if (isMaster || isWidgetActive("live_spots") ||
        appCfg.propOverlay != PropOverlayType::None)
      spotProvider->fetch();

    if (isMaster || isWidgetActive("on_the_air") ||
        isWidgetActive("dx_peditions") || appCfg.ontaFilter != "Off")
      activityProvider->fetch();

    if (isMaster || isWidgetActive("band_conditions"))
      bandProvider->update();

    if (isMaster || isWidgetActive("contests"))
      contestProvider->fetch();

    if (isMaster || isWidgetActive("moon"))
      moonProvider->update(appCfg.lat, appCfg.lon);

    if (isWidgetActive("eme_tool")) {
      auto it = widgetPool.find("eme_tool");
      if (it != widgetPool.end()) {
        auto *eme = static_cast<EMEToolPanel *>(it->second.get());
        eme->setDeLocation(appCfg.lat, appCfg.lon);
        eme->setDxLocation(ctx.state->dxLocation.lat,
                           ctx.state->dxLocation.lon);
      }
    }

    if (isMaster || isWidgetActive("history_flux"))
      historyProvider->fetchFlux();
    if (isMaster || isWidgetActive("history_ssn") ||
        isWidgetActive("solar_cycle"))
      historyProvider->fetchSSN();
    if (isMaster || isWidgetActive("history_kp"))
      historyProvider->fetchKp();

    if (isMaster || isWidgetActive("spacewx_alerts"))
      spaceWeatherAlertProvider->fetch();

    if (dstProvider && (isMaster || isWidgetActive("dst_index")))
      dstProvider->fetch();

    // --- ADIF log viewer ---
    if (isMaster || isWidgetActive("adif"))
      adifProvider->fetch(ctx.cfgMgr.configDir() / "logs.adif");

    // --- Propagation map overlays ---
    if (mufOverlayActive)
      mufRtProvider->update();
    if (mufOverlayActive)
      ionosondeProvider->update();

    // --- Asteroid widget + map pin ---
    if (isMaster || isWidgetActive("asteroid"))
      appContext_.asteroidProvider->update();

#ifndef __EMSCRIPTEN__
    if (ctx.updateChecker)
      ctx.updateChecker->fetch();
#endif
    lastFetchMs = now;
#if defined(__linux__) && !defined(__EMSCRIPTEN__) && !defined(__ANDROID__)
    // Return heap pages freed during provider parse burst back to the OS.
    // 20+ concurrent fetches leave glibc malloc arenas with freed-but-retained
    // pages that inflate RSS by ~50MB per screen wake if not trimmed.
    malloc_trim(0);
#endif
  }

  // --- Immediate Fetches for Active Widgets (Data missing or stale) ---
  const uint32_t kCooldown = 60000u;
  if (isPowerOn) {
    // NOAA / SpaceWeather
    const bool needsNoaa = isMaster || isWidgetActive("solar") ||
                           isWidgetActive("aurora") ||
                           isWidgetConfigured("aurora_graph") ||
                           isWidgetActive("drap") ||
                           isWidgetActive("noaa_spacewx") ||
                           isWidgetActive("band_conditions") ||
                           isWidgetActive("kindex_trend") ||
                           isWidgetActive("sfi_trend") ||
                           appCfg.propOverlay == PropOverlayType::Drap ||
                           appCfg.propOverlay == PropOverlayType::Aurora;
    if (needsNoaa && (!ctx.solarStore->get().valid || noaaProvider->isStale(now, 15 * 60 * 1000)) &&
        noaaProvider->isStale(now, kCooldown)) {
      noaaProvider->fetch();
    }

    // RSS
    if (appCfg.rssEnabled && rssProvider->isStale(now, 15 * 60 * 1000) &&
        rssProvider->isStale(now, kCooldown)) {
      rssProvider->fetch();
    }

    // Satellite
    if ((isMaster || isWidgetActive("eme_tool") || appCfg.showSatTrack) &&
        satMgr->isStale(now, 2 * 60 * 60 * 1000) &&
        satMgr->isStale(now, kCooldown)) {
      satMgr->fetch();
    }

    // Weather
    if ((isMaster || isWidgetActive("de_weather")) &&
        (ctx.state->deLocation.lat != 0.0 || ctx.state->deLocation.lon != 0.0) &&
        (!ctx.deWeatherStore->get().valid || deWeatherProvider->isStale(now, 15 * 60 * 1000)) &&
        deWeatherProvider->isStale(now, kCooldown)) {
      deWeatherProvider->fetch(ctx.state->deLocation.lat, ctx.state->deLocation.lon);
    }
    if ((isMaster || isWidgetActive("dx_weather")) &&
        (ctx.state->dxLocation.lat != 0.0 || ctx.state->dxLocation.lon != 0.0) &&
        (!ctx.dxWeatherStore->get().valid || dxWeatherProvider->isStale(now, 15 * 60 * 1000)) &&
        dxWeatherProvider->isStale(now, kCooldown)) {
      dxWeatherProvider->fetch(ctx.state->dxLocation.lat, ctx.state->dxLocation.lon);
    }

    // Live Spots
    if ((isMaster || isWidgetActive("live_spots") || appCfg.propOverlay != PropOverlayType::None) &&
        (!currentSpotSnapshot_->valid || spotProvider->isStale(now, 5 * 60 * 1000)) &&
        spotProvider->isStale(now, kCooldown)) {

      spotProvider->fetch();
    }

    // Activity (POTA/SOTA/DXPeds)
    if ((isMaster || isWidgetActive("on_the_air") || isWidgetActive("dx_peditions") || appCfg.ontaFilter != "Off") &&
        (!ctx.activityStore->get()->valid || activityProvider->isStale(now, 15 * 60 * 1000)) &&
        activityProvider->isStale(now, kCooldown)) {
      activityProvider->fetch();
    }

    // Band Conditions
    if ((isMaster || isWidgetActive("band_conditions")) &&
        (!ctx.bandStore->get().valid || bandProvider->isStale(now, 15 * 60 * 1000)) &&
        bandProvider->isStale(now, kCooldown)) {
      bandProvider->update();
    }

    // Contests
    if ((isMaster || isWidgetActive("contests")) &&
        (!ctx.contestStore->get().valid || contestProvider->isStale(now, 12 * 60 * 60 * 1000)) &&
        contestProvider->isStale(now, kCooldown)) {
      contestProvider->fetch();
    }

    // Forecast
    if (forecastProvider &&
        (isMaster || isWidgetActive("forecast")) &&
        forecastProvider->isStale(now, 4 * 60 * 60 * 1000) &&
        forecastProvider->isStale(now, kCooldown) &&
        (appCfg.lat != 0.0 || appCfg.lon != 0.0)) {
      forecastProvider->fetch(appCfg.lat, appCfg.lon);
    }

    // Solar Flares
    if ((isMaster || isWidgetActive("flare_log")) && flareProvider && flareProvider->isStale(now, 15 * 60 * 1000)) {
      flareProvider->fetch();
    }

    // History
    if ((isMaster || isWidgetActive("history_flux")) && historyProvider->isStale(now, kCooldown)) {
      historyProvider->fetchFlux();
    }
    if ((isMaster || isWidgetActive("history_ssn") || isWidgetActive("solar_cycle")) && historyProvider->isStale(now, kCooldown)) {
      historyProvider->fetchSSN();
    }
    if ((isMaster || isWidgetActive("history_kp")) && historyProvider->isStale(now, kCooldown)) {
      historyProvider->fetchKp();
    }

    if (isMaster || isWidgetActive("netlogger")) {
      netLoggerProvider->fetch();
    }
    
    if (isMaster || isWidgetActive("powerwall")) {
      powerwallProvider->fetch();
    }

    // SpaceWx Alerts
    if ((isMaster || isWidgetActive("spacewx_alerts")) &&
        (!appContext_.spaceWxAlertStore->get().valid || spaceWeatherAlertProvider->isStale(now, 15 * 60 * 1000)) &&
        spaceWeatherAlertProvider->isStale(now, kCooldown)) {
      spaceWeatherAlertProvider->fetch();
    }

    // DST Index
    if (dstProvider && (isMaster || isWidgetActive("dst_index")) &&
        (!ctx.dstStore->get().valid || dstProvider->isStale(now, 12 * 60 * 60 * 1000)) &&
        dstProvider->isStale(now, kCooldown)) {
      dstProvider->fetch();
    }

    // ADIF
    if ((isMaster || isWidgetActive("adif")) &&
        (!ctx.adifStore->get().valid || adifProvider->isStale(now, 15 * 60 * 1000)) &&
        adifProvider->isStale(now, kCooldown)) {
      adifProvider->fetch(ctx.cfgMgr.configDir() / "logs.adif");
    }

    // Asteroid
    if ((isMaster || isWidgetActive("asteroid")) &&
        appContext_.asteroidProvider->isStale(now, 24 * 60 * 60 * 1000) &&
        appContext_.asteroidProvider->isStale(now, kCooldown)) {
      appContext_.asteroidProvider->update();
    }

    // Marine (fetch periodically if widget active)
    if (isWidgetActive("marine") &&
        marineProvider->isStale(now, 15 * 60 * 1000) &&
        marineProvider->isStale(now, kCooldown)) {
      marineProvider->fetch(appCfg.marineStation, appCfg.marineBuoy);
    }

    // DX timezone lookup via ZoneDetect API — fires once per DX location change.
    // locationMutex is held by the main thread (main.cpp) for the entire
    // update+render tick.  The callback MUST NOT re-acquire it: NetworkManager
    // delivers cache-hit callbacks synchronously on the calling thread, so
    // attempting std::lock_guard(locationMutex) here would deadlock.
    // dxTzOffset (int) and dxTzValid (bool) are the only fields the callback
    // writes; DXInfo::update() reads them without holding locationMutex, so
    // omitting the lock in the callback does not change the thread-safety
    // contract for those two fields.
    if (ctx.state->dxActive) {
      bool changed =
          (ctx.state->dxLocation.lat != ctx.state->dxTzQueryLoc.lat ||
           ctx.state->dxLocation.lon != ctx.state->dxTzQueryLoc.lon);
      if (changed) {
        ctx.state->dxTzValid    = false;
        ctx.state->dxTzQueryLoc = ctx.state->dxLocation;
        double dxLat = ctx.state->dxLocation.lat;
        double dxLon = ctx.state->dxLocation.lon;
        char url[128];
        std::snprintf(url, sizeof(url),
            "https://timezone.bertold.org/timezone?lat=%.4f&lon=%.4f&c=1",
            dxLat, dxLon);
        auto dashLive = dashboardLive_;
        auto stateRef = ctx.state;
        ctx.netManager->fetchAsync(url, [dashLive, stateRef, dxLon](std::string body) {
          if (!dashLive->load(std::memory_order_acquire)) return;
          auto pos = body.find("\"TimezoneId\"");
          if (pos == std::string::npos) return;
          auto q1 = body.find('"', pos + 13);
          if (q1 == std::string::npos) return;
          auto q2 = body.find('"', q1 + 1);
          if (q2 == std::string::npos) return;
          std::string tzId = body.substr(q1 + 1, q2 - q1 - 1);
          int offset = tzIdToOffset(tzId, dxLon);
          stateRef->dxTzId     = tzId;
          stateRef->dxTzOffset = offset;
          stateRef->dxTzValid  = true;
        }, 86400, false);
      }
    }
  }

  // --- Late-start DX Cluster / RBN providers ---
  // dxcProvider and rbnProvider are only started at build time when
  // isWidgetConfigured() finds dx_cluster/watchlist in the saved pane rotations.
  // If the widget is added mid-session (e.g. set_pane?action=solo), the provider
  // must be started now so the panel receives data without requiring a restart.
#ifndef __EMSCRIPTEN__
  if (!dxcProvider->isRunning() &&
      (isMaster || isWidgetActive("dx_cluster") || isWidgetActive("watchlist")))
    dxcProvider->start(appCfg);
  if (!rbnProvider->isRunning() && appCfg.rbnEnabled &&
      (isMaster || isWidgetActive("dx_cluster") || isWidgetActive("watchlist")))
    rbnProvider->start(appCfg);
#endif

  // --- DRAP fetch: immediate when overlay active and store empty (60s
  // cooldown) ---
  if (isPowerOn && appCfg.propOverlay == PropOverlayType::Drap &&
      !ctx.drapDataStore->get().valid &&
      (lastDrapFetchMs == 0 || now - lastDrapFetchMs > 60000u)) {
    noaaProvider->fetchDRAP();
    lastDrapFetchMs = now;
  }

  // --- Tropo fetch (immediate upon widget activation, internal cache 1hr) ---
  if (isWidgetActive("tropo")) {
    tropoProvider->fetch(appCfg.lat, appCfg.lon);
  }

  // --- Lightning fetch (immediate upon widget activation, internal cache 2m)
  // ---
  if (isWidgetActive("lightning")) {
    lightningProvider->fetch(appCfg.lat, appCfg.lon);
  }

  // --- Meteor fetch (immediate upon widget activation, internal cache 10m) ---
  if (isWidgetActive("meteor")) {
    meteorProvider->update(appCfg.lat, appCfg.lon);
  }

  // --- Ionosonde fetch (immediate upon widget activation, internal cache 15m)
  // ---
  if (isWidgetActive("ionosonde")) {
    ionosondeProvider->fetch(appCfg.lat, appCfg.lon);
  }

  // --- Solar Storm fetch (immediate upon widget activation, internal cache
  // 1m/5m) ---
  if (isWidgetActive("solar_storm")) {
    solarStormProvider->update();
  }

  if (isWidgetActive("greyline_dx") &&
      now - lastGreylineFetchMs > 60000) {
    greylineDXProvider->update();
    lastGreylineFetchMs = now;
  }

  // Reset fetch timer when switching away from Heatmap so re-selecting it
  // triggers an immediate refresh rather than waiting out the 5-min interval.
  if (prevPropOverlayForReach_ == PropOverlayType::Heatmap &&
      appCfg.propOverlay != PropOverlayType::Heatmap) {
    lastReachFetchMs = 0;
  }
  prevPropOverlayForReach_ = appCfg.propOverlay;

  if (appCfg.propOverlay == PropOverlayType::Heatmap &&
      (lastReachFetchMs == 0 || now - lastReachFetchMs > 5 * 60 * 1000)) {
    reachProvider->fetch(appCfg.propBand, appCfg.propMode);
    lastReachFetchMs = now;
  }

  // Calendar notification check — runs once per minute
  if (ctx.calendarStore && mapArea &&
      (lastCalendarCheckMs == 0 || now - lastCalendarCheckMs >= 60000)) {
    lastCalendarCheckMs = now;
    auto events = ctx.calendarStore->snapshot();
    time_t nowT = std::time(nullptr);
    for (const auto &ev : events) {
      time_t triggerAt;
      if (ev.allDay) {
        // All-day: notify at configured hour on the event day (UTC)
        struct tm evDay{};
        Astronomy::portable_gmtime(&ev.start, &evDay);
        evDay.tm_hour = appCfg.calendarAllDayNotifyHour;
        evDay.tm_min  = 0;
        evDay.tm_sec  = 0;
        triggerAt = Astronomy::portable_timegm(&evDay);
      } else {
        triggerAt = ev.start - (time_t)(appCfg.calendarNotifyMinutes * 60);
      }
      // Fire within a 60-second window around the trigger time
      if (nowT >= triggerAt && nowT < triggerAt + 60) {
        if (ctx.calendarStore->markNotified(ev.start)) {
          mapArea->showCalendarAlert(ev.summary, ev.source, ev.start,
                                     appCfg.calendarDismissMinutes);
          break; // Show one alert at a time
        }
      }
    }
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
      const SDL_Keycode sym = event.key.keysym.sym;
      const SDL_Keymod  mod = static_cast<SDL_Keymod>(event.key.keysym.mod);

      // Help panel: intercept scroll / dismiss keys when visible (before widget dispatch)
      if (g_help.visible && !consumed) {
        if (sym == SDLK_ESCAPE ||
            (sym == SDLK_SLASH && (mod & KMOD_SHIFT))) {
          g_help.visible = false;
          consumed = true;
        } else if (sym == SDLK_UP || sym == SDLK_PAGEUP) {
          g_help.scrollY = std::max(0, g_help.scrollY - kHelpLineH * 3);
          consumed = true;
        } else if (sym == SDLK_DOWN || sym == SDLK_PAGEDOWN) {
          int maxS = std::max(0, g_help.contentH - kHelpViewH);
          g_help.scrollY = std::min(g_help.scrollY + kHelpLineH * 3, maxS);
          consumed = true;
        } else {
          consumed = true; // swallow all other keys while panel is open
        }
      }

      if (!consumed) {
      if (focusedWidget) {
        consumed = focusedWidget->onKeyDown(event.key.keysym.sym,
                                            event.key.keysym.mod);
      } else {
        // ? (Shift+/) toggles the in-app help panel
        if (sym == SDLK_SLASH && (mod & KMOD_SHIFT)) {
          g_help.visible = !g_help.visible;
          if (g_help.visible) g_help.scrollY = 0;
          consumed = true;
        } else if (sym == SDLK_k) {
          ctx.showActionHighlights = !ctx.showActionHighlights;
          consumed = true;
#ifndef NDEBUG
        } else if (sym == SDLK_o && ctx.dashboard) {
          ctx.dashboard->debugOverlay.toggle();
          consumed = true;
#endif
        } else if (sym == SDLK_l) {
          ctx.appCfg.scaleToFullScreen = !ctx.appCfg.scaleToFullScreen;
          ctx.cfgMgr.save(ctx.appCfg);
          SDL_Event ev{};
          ev.type = SDL_WINDOWEVENT;
          ev.window.event = SDL_WINDOWEVENT_SIZE_CHANGED;
          SDL_GetWindowSize(ctx.window, &ev.window.data1, &ev.window.data2);
          SDL_PushEvent(&ev);
          consumed = true;
        } else if (sym == SDLK_h) {
          ctx.appCfg.glassHudMode = !ctx.appCfg.glassHudMode;
          ctx.cfgMgr.save(ctx.appCfg);
          layout.setGlassHudMode(ctx.appCfg.glassHudMode);
          std::string activeTheme = ctx.appCfg.glassHudMode ? "glass" : ctx.appCfg.theme;
          for (auto *w : widgets) {
            w->setTheme(activeTheme);
          }
          SDL_Event ev{};
          ev.type = SDL_WINDOWEVENT;
          ev.window.event = SDL_WINDOWEVENT_SIZE_CHANGED;
          SDL_GetWindowSize(ctx.window, &ev.window.data1, &ev.window.data2);
          SDL_PushEvent(&ev);
          consumed = true;
        } else if (sym == SDLK_F11) {

          Uint32 flags = SDL_GetWindowFlags(ctx.window);
          if (flags & SDL_WINDOW_FULLSCREEN_DESKTOP)
            SDL_SetWindowFullscreen(ctx.window, 0);
          else
            SDL_SetWindowFullscreen(ctx.window, SDL_WINDOW_FULLSCREEN_DESKTOP);
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
      } // end if (!consumed) [help panel guard]
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
      fingerScrollAccum_ = 0.0f;
      fingerWasScrolling_ = false;
      break;
    case SDL_FINGERMOTION: {
      // Convert vertical swipe to discrete scroll steps for dropdowns and lists.
      // tfinger.dy is normalized 0..1 relative to window height.
      // 0.04 ≈ one step per ~40px on a 1080p screen — adjust if too sensitive.
      static constexpr float kScrollStep = 0.04f;
      fingerScrollAccum_ += event.tfinger.dy;
      // Convert normalised finger position to logical coords — same pipeline
      // as the SDL_MOUSEWHEEL handler so hit-testing uses identical geometry.
      int fmx = static_cast<int>(event.tfinger.x * ctx.globalWinW);
      int fmy = static_cast<int>(event.tfinger.y * ctx.globalWinH);
      if (FIDELITY_MODE) {
        float pixX = fmx * static_cast<float>(ctx.globalDrawW) / ctx.globalWinW;
        float pixY = fmy * static_cast<float>(ctx.globalDrawH) / ctx.globalWinH;
        fmx = static_cast<int>(pixX / ctx.layScaleX);
        fmy = static_cast<int>(pixY / ctx.layScaleY);
      }
      auto dispatchScroll = [&](int dy) {
        if (updateOverlay) {
          updateOverlay->onMouseWheel(dy);
        } else if (mapArea->isModalActive()) {
          mapArea->onMouseWheel(dy);
        } else {
          for (auto *w : eventWidgets) {
            SDL_Rect r = w->getRect();
            if (fmx >= r.x && fmx < r.x + r.w && fmy >= r.y && fmy < r.y + r.h) {
              if (w->onMouseWheel(dy)) break;
            }
          }
        }
      };
      while (fingerScrollAccum_ <= -kScrollStep) {
        fingerScrollAccum_ += kScrollStep;
        fingerWasScrolling_ = true;
        dispatchScroll(-1); // finger moved up → list scrolls down
      }
      while (fingerScrollAccum_ >= kScrollStep) {
        fingerScrollAccum_ -= kScrollStep;
        fingerWasScrolling_ = true;
        dispatchScroll(1);  // finger moved down → list scrolls up
      }
      break;
    }
    case SDL_MOUSEBUTTONDOWN: {
      // Suppress touch-emulated click if the touch gesture was a scroll.
      if (event.button.which == SDL_TOUCH_MOUSEID && fingerWasScrolling_)
        break;
      int smx = event.button.x, smy = event.button.y;
      if (FIDELITY_MODE && event.button.windowID != 0) {
        float pixX = event.button.x * static_cast<float>(ctx.globalDrawW) /
                     ctx.globalWinW;
        float pixY = event.button.y * static_cast<float>(ctx.globalDrawH) /
                     ctx.globalWinH;
        smx = static_cast<int>(pixX / ctx.layScaleX);
        smy = static_cast<int>(pixY / ctx.layScaleY);
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
        if (FIDELITY_MODE && event.button.windowID != 0) {
          float pixX = event.button.x * static_cast<float>(ctx.globalDrawW) /
                       ctx.globalWinW;
          float pixY = event.button.y * static_cast<float>(ctx.globalDrawH) /
                       ctx.globalWinH;
          smx = static_cast<int>(pixX / ctx.layScaleX);
          smy = static_cast<int>(pixY / ctx.layScaleY);
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
          std::unique_ptr<std::vector<GroundTrackPoint>> track(
              static_cast<std::vector<GroundTrackPoint> *>(event.user.data1));
          if (track && ctx.dashboard && ctx.dashboard->mapArea) {
            ctx.dashboard->mapArea->onSatTrackReady(*track);
          }
          break;
        }
        case AE_SATELLITE_DATA_READY: {
          std::unique_ptr<std::string> raw(
              static_cast<std::string *>(event.user.data1));
          if (raw && ctx.dashboard && ctx.dashboard->satMgr) {
            ctx.dashboard->satMgr->onDataReady(*raw, event.user.code);
          }
          break;
        }
#ifndef __EMSCRIPTEN__
        case AE_UPDATE_DATA_READY: {
          std::unique_ptr<std::string> raw(
              static_cast<std::string *>(event.user.data1));
          if (raw && ctx.updateChecker) {
            ctx.updateChecker->onDataReady(*raw);
          }
          break;
        }
#endif
        case AE_RSS_DATA_READY: {
          int feed_idx = event.user.code;
          std::unique_ptr<std::vector<std::string>> headlines(
              static_cast<std::vector<std::string> *>(event.user.data1));
          if (headlines && feed_idx >= 0) {
            int target_idx = (feed_idx == 99) ? 3 : feed_idx;
            if (target_idx < 4) {
              rssHeadlines[target_idx] = std::move(*headlines);
              rssDataDirty = true;
            }
          }
          break;
        }
        case HamClock::AE_DX_ALERT: {
          struct DXAlertData {
            std::string call;
            std::string entity;
            double freq;
            std::string mode;
          };
          std::unique_ptr<DXAlertData> data(
              static_cast<DXAlertData *>(event.user.data1));
          if (data && ctx.dashboard && ctx.dashboard->mapArea) {
            ctx.dashboard->mapArea->showDXAlert(data->call, data->entity,
                                               data->freq, data->mode);
          }
          break;
        }
        case AE_SOLAR_DATA_READY: {
          std::unique_ptr<SolarData> update(
              static_cast<SolarData *>(event.user.data1));
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
            case NOAAProvider::UpdateType::NOAAScales:
              for (int c = 0; c < 3; ++c)
                for (int d = 0; d < 4; ++d)
                  data.noaa_scales[c][d] = update->noaa_scales[c][d];
              data.noaa_scales_valid = true;
              break;
            }
            ctx.solarStore->set(data);
            // Recalculate band conditions immediately when SFI or K-index arrive
            auto code = static_cast<NOAAProvider::UpdateType>(event.user.code);
            if (code == NOAAProvider::UpdateType::SFI ||
                code == NOAAProvider::UpdateType::KIndex) {
              bandProvider->update();
            }
          }
          break;
        }
        case AE_AURORA_DATA_READY: {
          std::unique_ptr<float> percentPtr(
              static_cast<float *>(event.user.data1));
          if (ctx.auroraHistoryStore) {
            ctx.auroraHistoryStore->addPoint(*percentPtr);
          }
          break;
        }
        case AE_ACTIVITY_DATA_READY: {
          std::unique_ptr<ActivityData> update(
              static_cast<ActivityData *>(event.user.data1));
          if (update && ctx.activityStore) {
            ActivityData data = *ctx.activityStore->get();
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
          break;
        }
        case AE_WEATHER_DATA_READY: {
          std::unique_ptr<WeatherData> update(
              static_cast<WeatherData *>(event.user.data1));
          int id = event.user.code;
          if (update) {
            if (id == 0 && ctx.deWeatherStore) {
              ctx.deWeatherStore->update(*update);
            } else if (id == 1 && ctx.dxWeatherStore) {
              ctx.dxWeatherStore->update(*update);
            }
          }
          break;
        }
        case AE_CONTEST_DATA_READY: {
          std::unique_ptr<ContestData> update(
              static_cast<ContestData *>(event.user.data1));
          if (update && ctx.contestStore) {
            ctx.contestStore->update(*update);
          }
          break;
        }
        case AE_HISTORY_DATA_READY: {
          std::unique_ptr<HistorySeries> update(
              static_cast<HistorySeries *>(event.user.data1));
          if (update && ctx.historyStore) {
            ctx.historyStore->update(update->name, *update);
          }
          break;
        }
        case AE_PROP_DATA_READY: {
          std::unique_ptr<std::vector<float>> grid(
              static_cast<std::vector<float> *>(event.user.data1));
          if (grid && ctx.dashboard && ctx.dashboard->mapArea) {
            ctx.dashboard->mapArea->onPropDataReady(
                static_cast<PropOverlayType>(event.user.code), *grid);
          }
          break;
        }
        case AE_ASTEROID_ELEMENTS_READY: {
          std::unique_ptr<std::pair<std::string, OrbitalElements>> payload(
              static_cast<std::pair<std::string, OrbitalElements> *>(
                  event.user.data1));
          if (payload && ctx.dashboard && ctx.dashboard->mapArea) {
            ctx.dashboard->mapArea->onAsteroidElementsReady(payload->first,
                                                            payload->second);
          }
          break;
        }
        case AE_ALERTS_DATA_READY: {
          std::unique_ptr<AlertsData> update(
              static_cast<AlertsData *>(event.user.data1));
          if (update && ctx.alertsStore)
            ctx.alertsStore->update(*update);
          break;
        }
        case AE_FORECAST_DATA_READY: {
          std::unique_ptr<ForecastData> update(
              static_cast<ForecastData *>(event.user.data1));
          if (update && ctx.forecastStore)
            ctx.forecastStore->update(*update);
          break;
        }
        case AE_SPACEWX_ALERT_READY: {
          std::unique_ptr<SpaceWxAlertData> update(
              static_cast<SpaceWxAlertData *>(event.user.data1));
          if (update && ctx.dashboard && ctx.dashboard->appContext_.spaceWxAlertStore)
            ctx.dashboard->appContext_.spaceWxAlertStore->update(*update);
          break;
        }
        case AE_REPEATER_DATA_READY: {
          std::unique_ptr<RepeaterData> update(
              static_cast<RepeaterData *>(event.user.data1));
          if (update && ctx.repeaterStore)
            ctx.repeaterStore->update(*update);
          break;
        }
        case AE_HURRICANE_DATA_READY: {
          std::unique_ptr<HurricaneData> update(
              static_cast<HurricaneData *>(event.user.data1));
          if (update && ctx.hurricaneStore)
            ctx.hurricaneStore->update(*update);
          break;
        }
        case AE_MARINE_DATA_READY: {
          std::unique_ptr<MarineData> update(
              static_cast<MarineData *>(event.user.data1));
          if (update && ctx.marineStore) {
            MarineData current = ctx.marineStore->get();
            if (event.user.code == 0) { // Tides
              current.tideStationId = update->tideStationId;
              if (!update->tideStationName.empty()) {
                current.tideStationName = update->tideStationName;
              }
              current.tides = update->tides;
              current.tidesValid = update->tidesValid;
            } else if (event.user.code == 1) { // Buoy
              current.buoy = update->buoy;
              current.buoyValid = update->buoyValid;
            }
            current.lastUpdate = update->lastUpdate;
            ctx.marineStore->update(current);
          }
          break;
        }
        case AE_MARINE_LOOKUP_READY: {
          std::unique_ptr<MarineLookupResult> res(
              static_cast<MarineLookupResult *>(event.user.data1));
          if (res && ctx.dashboard) {
            auto it = ctx.dashboard->widgetPool.find("marine");
            if (it != ctx.dashboard->widgetPool.end()) {
              static_cast<MarinePanel *>(it->second.get())->onLookupReady(*res);
            }
          }
          break;
        }

        case AE_WINLINK_DATA_READY: {
          std::unique_ptr<WinlinkData> update(
              static_cast<WinlinkData *>(event.user.data1));
          if (update && ctx.winlinkStore)
            ctx.winlinkStore->update(*update);
          break;
        }
        case AE_MAP_IMAGE_READY: {
          std::unique_ptr<std::string> data(
              static_cast<std::string *>(event.user.data1));
          bool isNight = (event.user.code == 1);
          if (data && ctx.dashboard && ctx.dashboard->mapArea) {
            ctx.dashboard->mapArea->onMapImageReady(isNight, std::move(*data));
          }
          break;
        }
        case AE_MOON_IMAGE_READY: {
          auto *surf = static_cast<SDL_Surface *>(event.user.data1);
          if (surf && ctx.dashboard) {
            if (ctx.dashboard->widgetPool.count("moon")) {
              static_cast<MoonPanel *>(
                  ctx.dashboard->widgetPool["moon"].get())
                  ->onImageReady(surf);
            } else {
              SDL_FreeSurface(surf);
            }
          } else if (surf) {
            SDL_FreeSurface(surf);
          }
          break;
        }
        case AE_TOUCH: {
          int mx = static_cast<int>(reinterpret_cast<intptr_t>(event.user.data1));
          int my = static_cast<int>(reinterpret_cast<intptr_t>(event.user.data2));

          if (focusedWidget) {
            focusedWidget->onMouseDown(mx, my, 0, 1);
            focusedWidget->onMouseUp(mx, my, 0, 1);
          } else {
            bool isFS = (expandedPaneIdx_ >= 0 &&
                         panes[expandedPaneIdx_]->getActiveType() == "big_clock");
            for (auto *w : eventWidgets) {
              if (expandedPaneIdx_ >= 0) {
                if (isFS) {
                  if (w != panes[expandedPaneIdx_].get() && w != widgetSelector.get())
                    continue;
                } else if (w == mapArea.get()) {
                  continue;
                }
              }
              SDL_Rect r = w->getRect();
              if (mx >= r.x && mx < r.x + r.w && my >= r.y && my < r.y + r.h) {
                w->onMouseDown(mx, my, 0, 1);
                if (w->onMouseUp(mx, my, 0, 1))
                  break;
              }
            }
          }
          break;
        }
        case AE_WHEEL: {
          int dy = static_cast<int>(reinterpret_cast<intptr_t>(event.user.data1));
          if (focusedWidget) {
            focusedWidget->onMouseWheel(dy);
          } else {
            for (auto *w : eventWidgets) {
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
        if (FIDELITY_MODE && event.motion.windowID != 0) {
          float pixX = event.motion.x * static_cast<float>(ctx.globalDrawW) /
                       ctx.globalWinW;
          float pixY = event.motion.y * static_cast<float>(ctx.globalDrawH) /
                       ctx.globalWinH;
          mx = static_cast<int>(pixX / ctx.layScaleX);
          my = static_cast<int>(pixY / ctx.layScaleY);
        }
        if (focusedWidget)
          focusedWidget->onMouseMove(mx, my);
        else {
          bool isFS = (expandedPaneIdx_ >= 0 && panes[expandedPaneIdx_]->getActiveType() == "big_clock");
          for (auto *w : eventWidgets) {
            if (expandedPaneIdx_ >= 0) {
              if (isFS) {
                if (w != panes[expandedPaneIdx_].get() && w != widgetSelector.get()) continue;
              } else if (w == mapArea.get()) {
                continue;
              }
            }
            w->onMouseMove(mx, my);
          }
        }
      }
      // MOUSEBUTTONUP
      else if (event.type == SDL_MOUSEBUTTONUP) {
        bool left = (event.button.button == SDL_BUTTON_LEFT);
        bool right = (event.button.button == SDL_BUTTON_RIGHT);
        
        // Suppress touch-emulated click if the touch gesture was a scroll.
        if ((left || right) &&
            !(event.button.which == SDL_TOUCH_MOUSEID && fingerWasScrolling_)) {
          int mx = event.button.x, my = event.button.y;
          if (FIDELITY_MODE && event.button.windowID != 0) {
            float pixX = event.button.x * static_cast<float>(ctx.globalDrawW) /
                         ctx.globalWinW;
            float pixY = event.button.y * static_cast<float>(ctx.globalDrawH) /
                         ctx.globalWinH;
            mx = static_cast<int>(pixX / ctx.layScaleX);
            my = static_cast<int>(pixY / ctx.layScaleY);
          }
          
          if (left) {
            if (focusedWidget)
              focusedWidget->onMouseUp(mx, my, SDL_GetModState(),
                                       event.button.clicks);
            else {
              bool isFS = (expandedPaneIdx_ >= 0 && panes[expandedPaneIdx_]->getActiveType() == "big_clock");
              for (auto *w : eventWidgets) {
                if (expandedPaneIdx_ >= 0) {
                  if (isFS) {
                    if (w != panes[expandedPaneIdx_].get() && w != widgetSelector.get()) continue;
                  } else if (w == mapArea.get()) {
                    continue;
                  }
                }
                if (w->onMouseUp(mx, my, SDL_GetModState(), event.button.clicks))
                  break;
              }
            }
          } else if (right) {
            if (focusedWidget)
              focusedWidget->onRightClick(mx, my, SDL_GetModState());
            else {
              bool isFS = (expandedPaneIdx_ >= 0 && panes[expandedPaneIdx_]->getActiveType() == "big_clock");
              for (auto *w : eventWidgets) {
                if (expandedPaneIdx_ >= 0) {
                  if (isFS) {
                    if (w != panes[expandedPaneIdx_].get() && w != widgetSelector.get()) continue;
                  } else if (w == mapArea.get()) {
                    continue;
                  }
                }
                if (w->onRightClick(mx, my, SDL_GetModState()))
                  break;
              }
            }
          }
        }
      }
      // MOUSEWHEEL
      else if (event.type == SDL_MOUSEWHEEL) {
        int scrollY = event.wheel.y;
#if SDL_VERSION_ATLEAST(2, 0, 18)
        if (event.wheel.direction == SDL_MOUSEWHEEL_FLIPPED)
          scrollY = -scrollY;
#endif
        // Help panel captures mouse wheel when visible
        if (g_help.visible) {
          int maxS = std::max(0, g_help.contentH - kHelpViewH);
          g_help.scrollY = std::clamp(g_help.scrollY - scrollY * kHelpLineH * 2,
                                      0, maxS);
          // consumed — don't fall through to widget scroll
        } else if (ctx.dashboard->mapArea->isModalActive()) {
          ctx.dashboard->mapArea->onMouseWheel(scrollY);
        } else {
          int mx, my;
          SDL_GetMouseState(&mx, &my);
          if (FIDELITY_MODE) {
            float pixX = mx * static_cast<float>(ctx.globalDrawW) /
                         ctx.globalWinW;
            float pixY = my * static_cast<float>(ctx.globalDrawH) /
                         ctx.globalWinH;
            mx = static_cast<int>(pixX / ctx.layScaleX);
            my = static_cast<int>(pixY / ctx.layScaleY);
          }
          bool isFS = (expandedPaneIdx_ >= 0 && panes[expandedPaneIdx_]->getActiveType() == "big_clock");
          for (auto *w : eventWidgets) {
            if (expandedPaneIdx_ >= 0) {
              if (isFS) {
                if (w != panes[expandedPaneIdx_].get() && w != widgetSelector.get()) continue;
              } else if (w == mapArea.get()) {
                continue;
              }
            }
            SDL_Rect r = w->getRect();
            if (mx >= r.x && mx < r.x + r.w && my >= r.y && my < r.y + r.h) {
              if (w->onMouseWheel(scrollY))
                break;
            }
          }
        }
      }
    }
  }

  // After event loop, process any aggregated data
  if (rssDataDirty) {
    RSSData data;
    for (int i = 0; i < 4; ++i) {
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

  if (timePanel->isHelpRequested()) {
    timePanel->clearHelpRequest();
    g_help.visible = !g_help.visible;
  }

  // Sync predictor from standalone SatWidget if in pool
  auto *satWidget =
      dynamic_cast<SatWidget *>(widgetPool["satellite"].get());
  OrbitPredictor *activePredictor =
      satWidget ? satWidget->activePredictor() : nullptr;
  mapArea->setPredictor(activePredictor);
  auto *gimbal =
      dynamic_cast<GimbalPanel *>(widgetPool["gimbal"].get());
  if (gimbal) {
    gimbal->setPredictor(activePredictor);
    gimbal->setObserver(appCfg.lat, appCfg.lon);
  }
  if (satWidget)
    satWidget->setObserver(appCfg.lat, appCfg.lon);
  auto *sdoWidget =
      dynamic_cast<SDOPanel *>(widgetPool["sdo"].get());
  if (sdoWidget)
    sdoWidget->setObserver(appCfg.lat, appCfg.lon);

  mapArea->setAsteroidProvider(appContext_.asteroidProvider.get());

  // Recalculate UI call logic
  if (lastResizeMs && (SDL_GetTicks() - lastResizeMs > 200)) {
    lastResizeMs = 0;
    int dw, dh;
    SDL_GetRendererOutputSize(ctx.renderer, &dw, &dh);
    float ns = static_cast<float>(dh) / LOGICAL_HEIGHT;
    if (ns > 0.5f) {
      if (std::fabs(ns - fontMgr.renderScale()) > 0.01f) {
        fontMgr.setRenderScale(ns);
        fontMgr.clearCache();
        fontCatalog.recalculate(LOGICAL_WIDTH, LOGICAL_HEIGHT);
      }
      layout.recalculate(LOGICAL_WIDTH, LOGICAL_HEIGHT, ctx.layLogicalOffX,
                         ctx.layLogicalOffY);
      rssBanner->onResize(139 + ctx.layLogicalOffX, 412 + ctx.layLogicalOffY,
                          660, 68);
    }
  }

#ifndef __EMSCRIPTEN__
  {
    Uint32 winFlags = ctx.window ? SDL_GetWindowFlags(ctx.window) : 0;
    bool isFullscreen = (winFlags & (SDL_WINDOW_FULLSCREEN |
                                     SDL_WINDOW_FULLSCREEN_DESKTOP)) != 0;
    if (isFullscreen && cursorVisible &&
        (SDL_GetTicks() - lastMouseMotionMs > 10000)) {
      SDL_ShowCursor(SDL_DISABLE);
      cursorVisible = false;
    }
  }
#endif

  if (appCfg.preventSleep && (now - lastSleepAssert > 30000)) {
    preventRPiSleep(true);
    lastSleepAssert = now;
  }

  // 60-second RSS/VRAM heartbeat — helps diagnose memory growth on RPi
  if (now - lastMemLogMs > 60000) {
    auto &mm = MemoryMonitor::getInstance();
    mm.logStats("heartbeat");
    {
      size_t rss = mm.getRSS();
      // Hard limit at 400 MB.
      // 60-second heartbeat window means threshold must be well below OOM kill.
      size_t threshold = 400ULL * 1024 * 1024;
      if (rss > threshold) {
        LOG_W("Main", "RSS {:.1f} MB exceeds 400MB limit — flushing caches",
              rss / 1024.0 / 1024.0);
        texMgr.setMaxCacheSize(15);
        fontMgr.setTextCacheLimit(100);
        fontMgr.setVolatileCacheLimit(100);
        fontCatalog.clearCache();
        fontMgr.clearCache();
        if (ctx.netManager) {
          ctx.netManager->clearRamCache();
        }
#if defined(__linux__) && !defined(__EMSCRIPTEN__) && !defined(__ANDROID__)
        malloc_trim(0);
#endif
      }
    }
    lastMemLogMs = now;
  }

  // 10-minute network cache prune — helps prevent disk and RAM bloat from time-stamped URLs (SDO/Moon)
  if (now - lastPruneMs_ > 10 * 60 * 1000) {
    if (ctx.netManager)
      ctx.netManager->pruneStaleCache();
    lastPruneMs_ = now;
  }

  if (isPowerOn) {
    for (auto *w : widgets)
      w->update();
    // satMgr->update(); // Deprecated: Auto-tracking handled by RotatorService
    ctx.brightnessMgr->update();
  }
}

// Renders the in-app help panel overlay.
// Panel origin is centered in the 800×480 logical space.
// Caller is responsible for calling only when g_help.visible is true.
static void renderHelpPanel(SDL_Renderer *renderer, FontCatalog &cat) {
  using namespace HamClock;

  const int panelX = (LOGICAL_WIDTH  - kHelpPanelW) / 2;
  const int panelY = (LOGICAL_HEIGHT - kHelpPanelH) / 2;

  // Dim background
  SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
  SDL_SetRenderDrawColor(renderer, 0, 0, 0, 180);
  SDL_Rect full = {0, 0, LOGICAL_WIDTH, LOGICAL_HEIGHT};
  SDL_RenderFillRect(renderer, &full);

  // Panel background
  SDL_SetRenderDrawColor(renderer, 18, 18, 28, 240);
  SDL_Rect panel = {panelX, panelY, kHelpPanelW, kHelpPanelH};
  SDL_RenderFillRect(renderer, &panel);
  SDL_SetRenderDrawColor(renderer, 80, 160, 255, 200);
  SDL_RenderDrawRect(renderer, &panel);

  // Title bar
  SDL_SetRenderDrawColor(renderer, 30, 60, 120, 220);
  SDL_Rect titleBar = {panelX, panelY, kHelpPanelW, kHelpLineH + kHelpPadY * 2};
  SDL_RenderFillRect(renderer, &titleBar);
  cat.drawText(renderer, "HamClock-Next  —  Help  (? or Esc to close)",
               panelX + kHelpPadX, panelY + kHelpPadY,
               {200, 220, 255, 255}, FontStyle::Fast, false, false, false);

  // Content viewport: clip to [panelY + titleH, panelY + panelH - footerH]
  const int contentTop    = panelY + kHelpLineH + kHelpPadY * 2 + 2;
  const int contentBottom = panelY + kHelpPanelH - kHelpLineH - kHelpPadY * 2;
  const int contentLeft   = panelX + kHelpPadX;

  // Build content lines and measure; also render when y is in viewport.
  auto drawLine = [&](int &y, const std::string &text, SDL_Color col, FontStyle fs) {
    const int screenY = contentTop + y - g_help.scrollY;
    if (screenY + kHelpLineH >= contentTop && screenY < contentBottom) {
      cat.drawText(renderer, text, contentLeft, screenY, col, fs);
    }
    y += kHelpLineH;
  };
  auto drawSep = [&](int &y) { y += kHelpLineH / 2; };
  auto drawSection = [&](int &y, const std::string &title) {
    y += kHelpLineH / 2;
    drawLine(y, title, {120, 190, 255, 255}, FontStyle::FastBold);
    // Underline
    const int screenY = contentTop + y - g_help.scrollY - kHelpLineH + kHelpLineH - 2;
    if (screenY >= contentTop && screenY < contentBottom) {
      SDL_SetRenderDrawColor(renderer, 60, 100, 180, 180);
      SDL_RenderDrawLine(renderer, contentLeft, screenY,
                         panelX + kHelpPanelW - kHelpPadX, screenY);
    }
  };

  int y = 0;
  const SDL_Color kWhite  = {220, 220, 220, 255};
  const SDL_Color kDim    = {150, 150, 150, 255};
  const SDL_Color kYellow = {230, 200,  80, 255};

  // ── Keyboard Shortcuts ──────────────────────────────────────────────────────
  drawSection(y, "KEYBOARD SHORTCUTS");
  drawSep(y);

  struct KBRow { const char *key; const char *desc; };
  static const KBRow kShortcuts[] = {
    { "K",       "Toggle interactive region highlights (K mode)" },
    { "L",       "Toggle scale-to-fullscreen layout mode" },
    { "H",       "Toggle glass HUD mode" },
    { "?",       "Toggle this help panel" },
    { "F11",     "Toggle fullscreen" },
    { "Ctrl+Q",  "Quit HamClock-Next" },
  };
  for (auto &row : kShortcuts) {
    const int screenY = contentTop + y - g_help.scrollY;
    if (screenY + kHelpLineH >= contentTop && screenY < contentBottom) {
      cat.drawText(renderer, row.key,  contentLeft,      screenY, kYellow, FontStyle::Fast);
      cat.drawText(renderer, row.desc, contentLeft + 72, screenY, kWhite,  FontStyle::Fast);
    }
    y += kHelpLineH;
  }
  drawSep(y);

  // ── Mouse / Touch ───────────────────────────────────────────────────────────
  drawSection(y, "MOUSE / TOUCH");
  drawSep(y);
  struct MouseRow { const char *action; const char *desc; };
  static const MouseRow kMouse[] = {
    { "Click pane strip",    "Open widget picker for that pane" },
    { "Click \xe2\x98\x85 (star)",    "Open Presets modal" },
    { "Click \xe2\x9a\x99 (gear)",    "Open Setup modal" },
    { "Pane arrows",         "Advance or retreat widget rotation" },
    { "Scroll wheel",        "Scroll list widgets; this panel" },
  };
  for (auto &row : kMouse) {
    const int screenY = contentTop + y - g_help.scrollY;
    if (screenY + kHelpLineH >= contentTop && screenY < contentBottom) {
      cat.drawText(renderer, row.action, contentLeft,       screenY, kYellow, FontStyle::Fast);
      cat.drawText(renderer, row.desc,   contentLeft + 110, screenY, kWhite,  FontStyle::Fast);
    }
    y += kHelpLineH;
  }
  drawSep(y);

  // ── Widgets ─────────────────────────────────────────────────────────────────
  drawSection(y, "WIDGETS  (press K to highlight interactive regions)");
  drawSep(y);

  auto &reg = WidgetRegistry::instance();
  auto allWidgets = reg.getAll(/*includeKeyRequired=*/true);
  for (auto *desc : allWidgets) {
    if (!desc) continue;
    const int screenY = contentTop + y - g_help.scrollY;
    if (screenY + kHelpLineH >= contentTop && screenY < contentBottom) {
      std::string nameStr = desc->displayName;
      std::string descStr = desc->description ? desc->description : "";
      cat.drawText(renderer, nameStr, contentLeft,       screenY, kYellow, FontStyle::Fast);
      cat.drawText(renderer, descStr, contentLeft + 110, screenY, kDim,    FontStyle::Fast);
    }
    y += kHelpLineH;
  }
  drawSep(y);

  // Save content height for scroll clamping
  g_help.contentH = y;

  // Footer
  const SDL_Color kFooter = {100, 100, 130, 255};
  cat.drawText(renderer, "\xe2\x86\x91\xe2\x86\x93 / wheel to scroll",
               panelX + kHelpPadX,
               panelY + kHelpPanelH - kHelpLineH - kHelpPadY,
               kFooter, FontStyle::Fast);

  // Scroll indicator
  if (g_help.contentH > kHelpViewH) {
    const int trackH   = contentBottom - contentTop;
    const int thumbH   = std::max(20, trackH * kHelpViewH / std::max(1, g_help.contentH));
    const int thumbY   = contentTop + (trackH - thumbH) * g_help.scrollY /
                         std::max(1, g_help.contentH - kHelpViewH);
    const int trackX   = panelX + kHelpPanelW - 5;
    SDL_SetRenderDrawColor(renderer, 50, 60, 90, 180);
    SDL_RenderDrawLine(renderer, trackX, contentTop, trackX, contentBottom);
    SDL_SetRenderDrawColor(renderer, 100, 140, 220, 220);
    SDL_Rect thumb = {trackX - 2, thumbY, 4, thumbH};
    SDL_RenderFillRect(renderer, &thumb);
  }
}

void DashboardContext::render(AppContext &ctx) {
  SDL_SetRenderDrawColor(ctx.renderer, 0, 0, 0, 255);
  SDL_RenderClear(ctx.renderer);

  if (FIDELITY_MODE) {
    SDL_RenderSetViewport(ctx.renderer, nullptr);
    SDL_RenderSetScale(ctx.renderer, ctx.layScaleX, ctx.layScaleY);
  }

  if (!ctx.displayPower->getPower()) {
    uint32_t nowMs = SDL_GetTicks();
    if (ctx.displayPower->consumeBlackFrame()) {
      SDL_RenderPresent(ctx.renderer); // first black frame to clear display
      lastBlackFrameMs_ = nowMs;
    } else if (nowMs - lastBlackFrameMs_ > 2000) {
      // Keep SDL's KMSDRM page-flip chain alive with a periodic black frame.
      // Stopping SDL_RenderPresent entirely can cause the KMS framebuffer
      // chain to fall into an inconsistent state, resulting in new GEM
      // buffer allocations (~50MB) that are never freed on each screen wake.
      SDL_SetRenderDrawColor(ctx.renderer, 0, 0, 0, 255);
      SDL_RenderClear(ctx.renderer);
      SDL_RenderPresent(ctx.renderer);
      lastBlackFrameMs_ = nowMs;
    }
    return;
  }

  Widget *activeModal = nullptr;
  bool isFS = (expandedPaneIdx_ >= 0 && panes[expandedPaneIdx_]->getActiveType() == "big_clock");
  
  if (ctx.appCfg.glassHudMode && mapArea) {
    SDL_Rect clip = mapArea->getRect();
    SDL_RenderSetClipRect(ctx.renderer, &clip);
    mapArea->render(ctx.renderer);
    SDL_RenderSetClipRect(ctx.renderer, nullptr);
  }

  for (auto *w : widgets) {
    if (expandedPaneIdx_ >= 0) {
      if (isFS) {
        if (w != panes[expandedPaneIdx_].get() && w != widgetSelector.get())
          continue;
      } else if (w == mapArea.get()) {
        continue;  // hide map behind expanded pane
      }
    }
    if (w->isModalActive())
      activeModal = w;
      
    if (ctx.appCfg.glassHudMode && w == mapArea.get())
      continue;
      
    SDL_Rect clip = w->getRect();
    
    if (ctx.appCfg.glassHudMode) {
      int mx, my;
      SDL_GetMouseState(&mx, &my);
      if (FIDELITY_MODE) {
        float pixX = mx * static_cast<float>(ctx.globalDrawW) / ctx.globalWinW;
        float pixY = my * static_cast<float>(ctx.globalDrawH) / ctx.globalWinH;
        mx = static_cast<int>(pixX / ctx.layScaleX);
        my = static_cast<int>(pixY / ctx.layScaleY);
      } else {
        mx = static_cast<int>(mx * (800.0f / ctx.globalWinW) - ctx.layLogicalOffX);
        my = static_cast<int>(my * (480.0f / ctx.globalWinH) - ctx.layLogicalOffY);
      }
      bool hovered = (mx >= clip.x && mx < clip.x + clip.w && my >= clip.y && my < clip.y + clip.h);
      
      SDL_Texture* target = glassTextures[w];
      int texW = FIDELITY_MODE ? static_cast<int>(clip.w * ctx.layScaleX) : clip.w;
      int texH = FIDELITY_MODE ? static_cast<int>(clip.h * ctx.layScaleY) : clip.h;
      
      if (target) {
        int actW, actH;
        SDL_QueryTexture(target, nullptr, nullptr, &actW, &actH);
        if (actW != texW || actH != texH) {
          SDL_DestroyTexture(target);
          target = nullptr;
          glassTextures.erase(w);
        }
      }
      
      if (!target && texW > 0 && texH > 0) {
        target = SDL_CreateTexture(ctx.renderer, SDL_PIXELFORMAT_RGBA8888, SDL_TEXTUREACCESS_TARGET, texW, texH);
        if (target) {
          SDL_SetTextureBlendMode(target, SDL_BLENDMODE_BLEND);
          glassTextures[w] = target;
        }
      }
      
      if (target) {
        SDL_SetRenderTarget(ctx.renderer, target);
        SDL_SetRenderDrawColor(ctx.renderer, 0, 0, 0, 0);
        SDL_RenderClear(ctx.renderer);
        
        if (FIDELITY_MODE) {
          SDL_Rect vp = {
            static_cast<int>(-clip.x * ctx.layScaleX),
            static_cast<int>(-clip.y * ctx.layScaleY),
            static_cast<int>(800 * ctx.layScaleX),
            static_cast<int>(480 * ctx.layScaleY)
          };
          SDL_RenderSetViewport(ctx.renderer, &vp);
          SDL_RenderSetScale(ctx.renderer, ctx.layScaleX, ctx.layScaleY);
        } else {
          SDL_Rect vp = { -clip.x, -clip.y, 800, 480 };
          SDL_RenderSetViewport(ctx.renderer, &vp);
        }
        
        SDL_RenderSetClipRect(ctx.renderer, &clip);
        SDL_SetRenderDrawBlendMode(ctx.renderer, SDL_BLENDMODE_BLEND);
        w->render(ctx.renderer);
        
        SDL_SetRenderTarget(ctx.renderer, nullptr);
        SDL_SetTextureAlphaMod(target, hovered ? 255 : 100);
        
        if (FIDELITY_MODE) {
          SDL_RenderSetScale(ctx.renderer, 1.0f, 1.0f);
          SDL_Rect dstClip = {
            static_cast<int>(clip.x * ctx.layScaleX),
            static_cast<int>(clip.y * ctx.layScaleY),
            texW,
            texH
          };
          SDL_RenderCopy(ctx.renderer, target, nullptr, &dstClip);
          SDL_RenderSetScale(ctx.renderer, ctx.layScaleX, ctx.layScaleY);
        } else {
          SDL_RenderCopy(ctx.renderer, target, nullptr, &clip);
        }
      } else {
        SDL_RenderSetClipRect(ctx.renderer, &clip);
        w->render(ctx.renderer);
      }
    } else {
      SDL_RenderSetClipRect(ctx.renderer, &clip);
      w->render(ctx.renderer);
    }
  }
  SDL_RenderSetClipRect(ctx.renderer, nullptr);

  // Deferred tooltip pass — renders after all widget content so tooltips
  // always appear on top regardless of widget rendering order.
  Widget::flushPendingTooltip(ctx.renderer);
  for (auto *w : widgets) {
    if (expandedPaneIdx_ >= 0) {
      if (isFS) {
        if (w != panes[expandedPaneIdx_].get() && w != widgetSelector.get())
          continue;
      } else if (w == mapArea.get()) {
        continue;
      }
    }
    w->renderTooltipLayer(ctx.renderer);
  }

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
      SDL_RenderSetScale(ctx.renderer, ctx.layScaleX, ctx.layScaleY);

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
      mx = static_cast<int>(pixX / ctx.layScaleX);
      my = static_cast<int>(pixY / ctx.layScaleY);
    }

    SDL_SetRenderDrawBlendMode(ctx.renderer, SDL_BLENDMODE_BLEND);
    std::string hoverTooltip;
    const char *hoverWidgetDesc = nullptr; // description of widget under the cursor

    for (auto *w : widgets) {
      // Track which widget body the cursor is inside (for description tooltip)
      SDL_Rect wr = w->getRect();
      if (mx >= wr.x && mx < wr.x + wr.w && my >= wr.y && my < wr.y + wr.h) {
        auto *wdesc = WidgetRegistry::instance().find(w->typeId());
        if (wdesc && wdesc->description)
          hoverWidgetDesc = wdesc->description;
      }

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

    // Render tooltip: action name (if hovering action) + widget description
    if (!hoverTooltip.empty() || hoverWidgetDesc) {
      auto *cat = &fontCatalog;
      if (cat) {
        int pad = 4;
        int tw1 = 0, th1 = 0, tw2 = 0, th2 = 0;
        SDL_Texture *tipTex1 = nullptr;
        SDL_Texture *tipTex2 = nullptr;

        if (!hoverTooltip.empty())
          tipTex1 = cat->renderText(ctx.renderer, hoverTooltip,
                    {255, 255, 200, 255}, FontStyle::Micro, &tw1, &th1);
        if (hoverWidgetDesc)
          tipTex2 = cat->renderText(ctx.renderer, hoverWidgetDesc,
                    {160, 200, 255, 255}, FontStyle::Micro, &tw2, &th2);

        int boxW = std::max(tw1, tw2) + pad * 2;
        int boxH = (th1 > 0 ? th1 + 2 : 0) + (th2 > 0 ? th2 : 0) + pad * 2;
        SDL_Rect box = {mx + 12, my + 12, boxW, boxH};
        if (box.x + box.w > LOGICAL_WIDTH)  box.x = mx - box.w - 4;
        if (box.y + box.h > LOGICAL_HEIGHT) box.y = my - box.h - 4;

        SDL_SetRenderDrawColor(ctx.renderer, 20, 20, 20, 220);
        SDL_RenderFillRect(ctx.renderer, &box);
        SDL_SetRenderDrawColor(ctx.renderer, 255, 255, 255, 180);
        SDL_RenderDrawRect(ctx.renderer, &box);

        int ty = box.y + pad;
        if (tipTex1) {
          SDL_Rect dst = {box.x + pad, ty, tw1, th1};
          SDL_RenderCopy(ctx.renderer, tipTex1, nullptr, &dst);
          cat->destroyTexture(tipTex1);
          ty += th1 + 2;
        }
        if (tipTex2) {
          SDL_Rect dst = {box.x + pad, ty, tw2, th2};
          SDL_RenderCopy(ctx.renderer, tipTex2, nullptr, &dst);
          cat->destroyTexture(tipTex2);
        }
      }
    }
  }

  // Debug overlay
#ifndef NDEBUG
  if (debugOverlay.isVisible()) {
    if (FIDELITY_MODE)
      SDL_RenderSetScale(ctx.renderer, 1.0f, 1.0f);

    std::vector<WidgetRect> actuals;
    for (auto *w : widgets) {
      if (w) {
        SDL_Rect r = w->getRect();
        r.x = static_cast<int>(r.x * ctx.layScaleX);
        r.y = static_cast<int>(r.y * ctx.layScaleY);
        r.w = static_cast<int>(r.w * ctx.layScaleX);
        r.h = static_cast<int>(r.h * ctx.layScaleY);
        actuals.push_back({w->getName(), r});
      }
    }
    debugOverlay.render(ctx.renderer, ctx.globalDrawW, ctx.globalDrawH, actuals);

    if (FIDELITY_MODE)
      SDL_RenderSetScale(ctx.renderer, ctx.layScaleX, ctx.layScaleY);
  }
#endif

  // In-app help panel (rendered topmost, above everything else)
  if (g_help.visible) {
    renderHelpPanel(ctx.renderer, fontCatalog);
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
