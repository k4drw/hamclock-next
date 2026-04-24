#pragma once
// WidgetDeps — non-owning dependency bag passed to widget factory lambdas.
// All pointed-to objects are owned by AppContext / DashboardContext and
// outlive every widget instance.  shared_ptr<T> members use incomplete types
// (valid per C++ std: shared_ptr supports incomplete T).

#include <memory>

// ConfigManager.h gives us AppConfig and ConfigManager.
// All factory lambdas need to access deps.appCfg fields, so include fully here.
#include "../core/ConfigManager.h"

class  FontManager;
class  TextureManager;
class  NetworkManager;
class  PrefixManager;
struct HamClockState;
class  CPUMonitor;

// Stores
class SolarDataStore;
class AuroraHistoryStore;
class WatchlistStore;
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
class KIndexHistoryStore;
class SFIHistoryStore;
class GreylineDXStore;
class AuroraMapStore;
class CalendarStore;
class SpaceWeatherAlertStore;
class LoTWActivityStore;
class ClublogStore;

// Providers / services (non-owning raw pointers)
class LiveSpotProvider;
class ActivityProvider;
class DRAPProvider;
class AuroraProvider;
class SDOProvider;
class BeaconProvider;
class SatelliteManager;
class AsteroidProvider;
class CallbookProvider;
class IonosondeProvider;
class RigService;
class FccProvider;
class MarineProvider;

// ---------------------------------------------------------------------------


struct WidgetDeps {
  // Core resources (long-lived, owned by DashboardContext / AppContext)
  FontManager      &fontMgr;
  TextureManager   &texMgr;
  AppConfig        &appCfg;
  ConfigManager    &cfgMgr;
  NetworkManager   &netManager;
  PrefixManager    &prefixMgr;
  std::shared_ptr<HamClockState> state;
  std::shared_ptr<CPUMonitor>    cpuMonitor;

  // Stores (shared ownership with AppContext)
  std::shared_ptr<SolarDataStore>       solarStore;
  std::shared_ptr<AuroraHistoryStore>   auroraHistoryStore;
  std::shared_ptr<WatchlistStore>       watchlistStore;
  std::shared_ptr<WatchlistHitStore>    watchlistHitStore;
  std::shared_ptr<LiveSpotDataStore>    spotStore;
  std::shared_ptr<ActivityDataStore>    activityStore;
  std::shared_ptr<DXClusterDataStore>   dxcStore;
  std::shared_ptr<BandConditionsStore>  bandStore;
  std::shared_ptr<ContestStore>         contestStore;
  std::shared_ptr<MoonStore>            moonStore;
  std::shared_ptr<HistoryStore>         historyStore;
  std::shared_ptr<WeatherStore>         deWeatherStore;
  std::shared_ptr<WeatherStore>         dxWeatherStore;
  std::shared_ptr<CallbookStore>        callbookStore;
  std::shared_ptr<DstStore>             dstStore;
  std::shared_ptr<ADIFStore>            adifStore;
  std::shared_ptr<SantaStore>           santaStore;
  std::shared_ptr<RotatorDataStore>     rotatorStore;
  std::shared_ptr<RigDataStore>         rigStore;
  std::shared_ptr<AlertsStore>          alertsStore;
  std::shared_ptr<ForecastStore>        forecastStore;
  std::shared_ptr<RepeaterStore>        repeaterStore;
  std::shared_ptr<HurricaneStore>       hurricaneStore;
  std::shared_ptr<MarineStore>          marineStore;
  std::shared_ptr<WinlinkStore>         winlinkStore;
  std::shared_ptr<DRAPDataStore>        drapDataStore;
  std::shared_ptr<XRayHistoryStore>     xrayHistoryStore;
  std::shared_ptr<KIndexHistoryStore>   kIndexHistoryStore;
  std::shared_ptr<SFIHistoryStore>      sfiHistoryStore;
  std::shared_ptr<GreylineDXStore>      greylineDXStore;
  std::shared_ptr<AuroraMapStore>       auroraMapStore;
  std::shared_ptr<CalendarStore>        calendarStore;
  std::shared_ptr<SpaceWeatherAlertStore> spaceWxAlertStore;
  std::shared_ptr<LoTWActivityStore>    lotwActivityStore;
  std::shared_ptr<ClublogStore>         clublogStore;

  // Providers / services (non-owning; owned by DashboardContext)
  LiveSpotProvider  *spotProvider      = nullptr;
  ActivityProvider  *activityProvider  = nullptr;
  DRAPProvider      *drapProvider      = nullptr;
  AuroraProvider    *auroraProvider    = nullptr;
  SDOProvider       *sdoProvider       = nullptr;
  BeaconProvider    *beaconProvider    = nullptr;
  SatelliteManager  *satMgr            = nullptr;
  AsteroidProvider  *asteroidProvider  = nullptr;
  CallbookProvider  *callbookProvider  = nullptr;
  std::shared_ptr<IonosondeProvider> ionosondeProvider;
  FccProvider       *fccProvider       = nullptr;
  RigService        *rigService        = nullptr;  // nullptr in WASM builds
  MarineProvider    *marineProvider    = nullptr;
};

