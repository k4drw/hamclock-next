#pragma once

// Shared header for AppContext and DashboardContext structs.
// Method implementations live in DashboardContext.cpp.

#include "ConfigManager.h"       // AppConfig, ConfigManager
#include "Constants.h"           // INITIAL_WIDTH, INITIAL_HEIGHT, etc.
#include "PrefixManager.h"       // PrefixManager (AppContext value member)
#include "DXClusterData.h"
#include "LiveSpotData.h"
#include "../ui/DebugOverlay.h"  // DebugOverlay (DashboardContext value member)
#include "../ui/LayoutManager.h" // LayoutManager, Widget (transitively)
#include "../ui/TextureManager.h"// TextureManager (DashboardContext value member)
#include "../services/FccProvider.h" // FccProvider (DashboardContext value member)

#include <SDL.h>
#include <atomic>
#include <functional>
#include <map>
#include <memory>
#include <string>
#include <vector>

// --------------------------------------------------------------------------
// Forward declarations for AppContext smart-pointer members
// --------------------------------------------------------------------------
struct HamClockState;

// Data stores
class SolarDataStore;
class AuroraHistoryStore;
class KIndexHistoryStore;
class SFIHistoryStore;
class WatchlistStore;
class RSSDataStore;
class WatchlistHitStore;
class LiveSpotDataStore;
class ActivityDataStore;
class DXClusterDataStore;
class BandConditionsStore;
class ContestStore;
class MoonStore;
class HistoryStore;
class WeatherStore;
class CallbookStore;
class DstStore;
class ADIFStore;
class LoTWActivityStore;
class ClublogStore;
class FlareDataStore;
class HeardMeStore;
class SantaStore;
class RotatorDataStore;
class RigDataStore;
class AlertsStore;
class ForecastStore;
class RepeaterStore;
class HurricaneStore;
class MarineStore;
class WinlinkStore;
class DRAPDataStore;
class XRayHistoryStore;
class GreylineDXStore;
class AuroraMapStore;
class CalendarStore;
class LaunchStore;

// Managers / services (AppContext)
class NetworkManager;
class DisplayPower;
class BrightnessManager;
class CPUMonitor;
class WebServer;
class FrameCapture;
class UpdateChecker;
class BMEProvider;
class BME280Provider;
class RemoteEnvProvider;
class LTR329Provider;
namespace HamClock { class GPSProvider; }

// --------------------------------------------------------------------------
// Forward declarations for DashboardContext smart-pointer members
// --------------------------------------------------------------------------

// Providers (unique_ptr)
class NOAAProvider;
class RSSProvider;
class LiveSpotProvider;
class LoTWActivityProvider;
class ClublogProvider;
class LoTWProvider;
class ActivityProvider;
class DXClusterProvider;
class RBNProvider;
class BandConditionsProvider;
class ContestProvider;
class FlareProvider;
class MoonProvider;
class HistoryProvider;
class WeatherProvider;
class DRAPProvider;
class ADIFProvider;
class SantaProvider;
class SatelliteManager;
class BeaconProvider;
class AlertsProvider;
class RepeaterProvider;
class HurricaneProvider;
class MarineProvider;
class WinlinkProvider;
class GreylineDXProvider;
namespace HamClock {
class TropoProvider;
class ReachProvider;
class MeteorProvider;
class SolarStormProvider;
class LightningProvider;
}

class SpaceWeatherAlertProvider;
class AsteroidProvider;
class LaunchProvider;
class OpenMeteoForecastProvider;

// New widget providers (forward declarations)
class SpaceWeatherAlertStore;

// Providers (shared_ptr)
class SDOProvider;
class AuroraProvider;
class CallbookProvider;
class DstProvider;
class MufRtProvider;
class IonosondeProvider;
class WxMbProvider;
class QRZProvider;

// Services
class RotatorService;
class RigService;

// UI panels
class TimePanel;
class WidgetSelector;
class PaneContainer;
class MapWidget;
class RSSBanner;
class UpdateOverlay;

// --------------------------------------------------------------------------
// Cross-reference forward declaration
// --------------------------------------------------------------------------
struct AppContext;

// --------------------------------------------------------------------------
// DashboardContext — re-created on exit from setup
// --------------------------------------------------------------------------
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
  std::unique_ptr<SpaceWeatherAlertProvider> spaceWeatherAlertProvider;
  std::shared_ptr<HamClock::ReachProvider> reachProvider;
  std::shared_ptr<AsteroidProvider> asteroidProvider;
  std::shared_ptr<LaunchProvider> launchProvider;
  std::unique_ptr<ContestProvider> contestProvider;
  std::unique_ptr<FlareProvider> flareProvider;
  std::unique_ptr<MoonProvider> moonProvider;
  std::unique_ptr<HistoryProvider> historyProvider;
  std::unique_ptr<WeatherProvider> deWeatherProvider;
  std::unique_ptr<WeatherProvider> dxWeatherProvider;
  std::shared_ptr<SDOProvider> sdoProvider;
  std::unique_ptr<DRAPProvider> drapProvider;
  std::shared_ptr<AuroraProvider> auroraProvider;
  std::shared_ptr<CallbookProvider> callbookProvider;
  FccProvider fccProvider;
  std::shared_ptr<DstProvider> dstProvider;
  std::unique_ptr<ADIFProvider> adifProvider;
  std::shared_ptr<LoTWActivityProvider> lotwActivityProvider;
  std::shared_ptr<ClublogProvider> clublogProvider;
  std::shared_ptr<LoTWProvider> lotwProvider;
  std::shared_ptr<MufRtProvider> mufRtProvider;
  std::shared_ptr<IonosondeProvider> ionosondeProvider;
  std::unique_ptr<SantaProvider> santaProvider;
  std::unique_ptr<HamClock::TropoProvider> tropoProvider;
  std::shared_ptr<HamClock::LightningProvider> lightningProvider;
  std::unique_ptr<HamClock::MeteorProvider> meteorProvider;
  std::shared_ptr<HamClock::SolarStormProvider> solarStormProvider;
  std::unique_ptr<SatelliteManager> satMgr;
  std::unique_ptr<BeaconProvider> beaconProvider;
  std::unique_ptr<AlertsProvider> alertsProvider;
  std::shared_ptr<OpenMeteoForecastProvider> forecastProvider;
  std::unique_ptr<RepeaterProvider> repeaterProvider;
  std::unique_ptr<HurricaneProvider> hurricaneProvider;
  std::unique_ptr<MarineProvider> marineProvider;
  std::unique_ptr<WinlinkProvider> winlinkProvider;
  std::unique_ptr<GreylineDXProvider> greylineDXProvider;
  std::shared_ptr<WxMbProvider> wxMbProvider;
  std::shared_ptr<QRZProvider> qrzProvider;

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
  
  std::map<Widget*, SDL_Texture*> glassTextures;
  std::unique_ptr<RSSBanner> rssBanner;
  std::unique_ptr<UpdateOverlay> updateOverlay;
  LayoutManager layout;

  // Collections
  //
  // Ownership invariant (load-bearing — do not rearrange):
  //   • widgetPool is the sole owner of every Widget. Entries are only ever
  //     inserted (see getOrCreateWidget in DashboardContext.cpp); they are
  //     never erased during the DashboardContext lifetime.
  //   • widgets and eventWidgets hold non-owning raw Widget* aliases into
  //     widgetPool. They stay valid because (a) the pool is insert-only, and
  //     (b) the member declaration order below places widgetPool first, so
  //     reverse-order destruction tears down the alias vectors before the
  //     owning map frees the Widget objects.
  //   • If future code ever erases from widgetPool mid-life, it MUST also
  //     remove the corresponding raw pointers from widgets and eventWidgets
  //     in the same step, or both vectors will alias freed memory.
  std::map<std::string, std::unique_ptr<Widget>> widgetPool;
  std::vector<Widget *> widgets;
  std::vector<Widget *> eventWidgets;

  // Widget factory — stored as a member so pane containers hold a valid [this]
  // capture rather than a dangling reference to a constructor-local lambda.
  std::function<Widget *(const std::string &)> widgetFactory_;

  // State
  Uint32 lastFetchMs = 0;
  Uint32 lastGreylineFetchMs = 0;
  Uint32 lastReachFetchMs = 0;
  PropOverlayType prevPropOverlayForReach_ = PropOverlayType::None;
  Uint32 lastDrapFetchMs = 0;
  Uint32 lastCalendarCheckMs = 0;
  Uint32 lastResizeMs = 0;
  Uint32 lastFpsUpdate = 0;
  int frames = 0;
  Uint32 lastMouseMotionMs = 0;
  int expandedPaneIdx_ = -1;
  bool cursorVisible = true;
  Uint32 lastSleepAssert = 0;
  Uint32 lastMemLogMs = 0;
  Uint32 lastPruneMs_ = 0;
  Uint32 lastBlackFrameMs_ = 0;
  uint32_t lastDxcVer_ = 0;
  uint32_t lastSpotVer_ = 0;
  std::shared_ptr<const DXClusterData> currentDxcSnapshot_;
  std::shared_ptr<const LiveSpotData> currentSpotSnapshot_;
  float fingerScrollAccum_ = 0.0f;  // accumulated normalized finger-Y for swipe-to-scroll
  bool fingerWasScrolling_ = false;  // true if current touch gesture crossed scroll threshold
  // Guards provider callbacks captured by background threads.
  // Set to false in ~DashboardContext() so callbacks exit safely after
  // the dashboard is destroyed, avoiding the data race on ctx.dashboard.get().
  std::shared_ptr<std::atomic<bool>> parksReadyLive_ =
      std::make_shared<std::atomic<bool>>(true);
  std::shared_ptr<std::atomic<bool>> dashboardLive_ =
      std::make_shared<std::atomic<bool>>(true);

  // State for background data aggregation
  std::vector<std::string> rssHeadlines[4];
  bool rssDataDirty = false;

  DashboardContext(AppContext &ctx);
  ~DashboardContext();

  void applySidePanelMode(const std::string &chosen, AppContext &ctx);
  void expandPane(int idx, AppContext &ctx);
  void restorePane(AppContext &ctx);
  void update(AppContext &ctx);
  void render(AppContext &ctx);

private:
  AppContext &appContext_;
};

// --------------------------------------------------------------------------
// AppContext — global application context (persistent state)
// --------------------------------------------------------------------------
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
  int globalWinW = HamClock::INITIAL_WIDTH;
  int globalWinH = HamClock::INITIAL_HEIGHT;
  int globalDrawW = HamClock::INITIAL_WIDTH;
  int globalDrawH = HamClock::INITIAL_HEIGHT;

  // Layout Metrics
  float layScaleX = 1.0f;
  float layScaleY = 1.0f;
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
  std::shared_ptr<LoTWActivityStore> lotwActivityStore;
  std::shared_ptr<ClublogStore> clublogStore;
  std::shared_ptr<FlareDataStore> flareStore;
  std::shared_ptr<HeardMeStore> heardMeStore;
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
  std::shared_ptr<KIndexHistoryStore> kIndexHistoryStore;
  std::shared_ptr<SFIHistoryStore> sfiHistoryStore;
  std::shared_ptr<GreylineDXStore> greylineDXStore;
  std::shared_ptr<AuroraMapStore> auroraMapStore;
  std::shared_ptr<CalendarStore> calendarStore;
  std::shared_ptr<LaunchStore> launchStore;
  std::shared_ptr<AsteroidProvider> asteroidProvider;
  std::shared_ptr<SpaceWeatherAlertStore> spaceWxAlertStore;

  #ifndef __EMSCRIPTEN__
  std::unique_ptr<WebServer> webServer;
  std::unique_ptr<FrameCapture> frameCapture;
  std::unique_ptr<UpdateChecker> updateChecker;
  std::unique_ptr<HamClock::GPSProvider> gpsProvider;
  #endif

  // Managers & Services
  std::unique_ptr<NetworkManager> netManager;
  PrefixManager prefixMgr;
  std::shared_ptr<DisplayPower> displayPower;
  std::shared_ptr<BrightnessManager> brightnessMgr;
  std::shared_ptr<CPUMonitor> cpuMonitor;

  std::unique_ptr<BME280Provider> bmeProvider;
  std::unique_ptr<RemoteEnvProvider> remoteEnvProvider;
  std::unique_ptr<LTR329Provider> ltr329Provider;

  // Setup State
  enum class SetupMode { None, Loading, Main };
  SetupMode activeSetup = SetupMode::None;
  std::unique_ptr<Widget> setupWidget;
  std::unique_ptr<FontManager> setupFontMgr;
  std::unique_ptr<FontCatalog> setupCatalog;

  // Remote-config reload signal.  WebServer thread sets this to true after a
  // successful POST /api/reload or /set_config; main_tick() reads and clears
  // it, then re-applies the in-memory config to live state (callsign, proxy,
  // themes, etc.) without tearing down the dashboard.
  std::atomic<bool> configReloadRequested{false};
  std::atomic<bool> mapUpdateRequested{false};
  // Rotation control commands written by WebServer thread, consumed on main
  // thread. rotationCmd: 0=none, 1=pause, 2=resume, 3=next, 4=jump-to-widget
  // rotationCmdPane: -1=all panes, 0-5=specific pane
  // rotationCmdWidget: registry index (used with cmd=4)
  std::atomic<int> rotationCmd{0};
  std::atomic<int> rotationCmdPane{-1};
  std::atomic<int> rotationCmdWidget{-1};
  // Pane expand/collapse command from WebServer thread, consumed on main thread.
  // -1=idle, 0-5=expand that pane index, -2=collapse
  std::atomic<int> paneExpandCmd{-1};
  bool startOnUpdateTab = false;
  bool startOnServicesTab = false;
  bool startupAnnounceDone = false;

  // Dashboard State (Transient)
  std::unique_ptr<DashboardContext> dashboard;

  void updateLayoutMetrics();
  ~AppContext();
};
