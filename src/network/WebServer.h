#pragma once

#include "../core/Constants.h"
#include <atomic>
#include <memory>
#include <mutex>
#include <thread>
#include <vector>

// Forward declaration to avoid pulling SDL into the header
struct SDL_Renderer;

// Forward declaration for httplib::Server (full definition in WebServer.cpp via httplib.h)
namespace httplib { class Server; }

struct AppConfig;
struct HamClockState;
class ConfigManager;
class WatchlistStore;
class SolarDataStore;
class DisplayPower;
class FrameCapture;
class ContestStore;
class DXClusterDataStore;
class LiveSpotDataStore;
class CPUMonitor;
class NetworkManager;
class ActivityDataStore;
class SatelliteManager;
class RotatorService;
class PaneContainer;
class WeatherStore;
class BrightnessManager;
class BME280Provider;
class LTR329Provider;
class ADIFProvider;
class StopwatchPanel;
class CalendarStore;
class TimePanel;
class RSSBanner;
class RSSProvider;
class MarineStore;

// Web interface for HamClock.
//
// Deadlock Prevention: This class interacts with HamClockState which has its
// own locationMutex. To avoid ABBA deadlocks, the following order must be
// strictly observed:
//   1. Acquire WebServer::dataMutex_
//   2. Acquire HamClockState::locationMutex
//
// Prefer snapshotting required state under dataMutex_ and releasing it
// before acquiring locationMutex whenever feasible.
class WebServer {
public:
  WebServer(SDL_Renderer *renderer, AppConfig &cfg, HamClockState &state,
            ConfigManager &cfgMgr, std::shared_ptr<DisplayPower> displayPower,
            std::atomic<bool> &reloadFlag,
            std::shared_ptr<WatchlistStore> watchlist = nullptr,
            std::shared_ptr<SolarDataStore> solar = nullptr,
            std::shared_ptr<ContestStore> contests = nullptr,
            std::shared_ptr<DXClusterDataStore> dxc = nullptr,
            std::shared_ptr<LiveSpotDataStore> spots = nullptr,
            std::shared_ptr<CPUMonitor> cpu = nullptr,
            int port = HamClock::DEFAULT_WEB_SERVER_PORT);
  ~WebServer();

  void start();
  void stop();

  void setFrameCapture(FrameCapture *fc) { std::lock_guard<std::mutex> lk(dataMutex_); frameCapture_ = fc; }
  void setLiveWebEnabled(bool enabled) { liveWebEnabled_ = enabled; }
  bool isLiveWebEnabled() const { return liveWebEnabled_; }
  void setNetworkManager(NetworkManager *nm) { std::lock_guard<std::mutex> lk(dataMutex_); netMgr_ = nm; }
  void setActivityStore(ActivityDataStore *a) { std::lock_guard<std::mutex> lk(dataMutex_); activityStore_ = a; }
  void setSatelliteManager(SatelliteManager *s) { std::lock_guard<std::mutex> lk(dataMutex_); satMgr_ = s; }
  void setRotatorService(RotatorService *r) { std::lock_guard<std::mutex> lk(dataMutex_); rotatorSvc_ = r; }
  void setRotationControl(std::atomic<int> *cmd, std::atomic<int> *pane,
                          std::atomic<int> *widget) {
    std::lock_guard<std::mutex> lk(dataMutex_);
    rotationCmd_ = cmd;
    rotationCmdPane_ = pane;
    rotationCmdWidget_ = widget;
  }
  void setPanes(std::vector<std::unique_ptr<PaneContainer>> *panes) {
    std::lock_guard<std::mutex> lk(dataMutex_);
    panes_ = panes;
  }
  void setWeatherStore(std::shared_ptr<WeatherStore> ws) { weatherStore_ = ws; }
  void setBrightnessManager(std::shared_ptr<BrightnessManager> bm) {
    brightnessMgr_ = bm;
  }
  void setADIFProvider(ADIFProvider *p) { std::lock_guard<std::mutex> lk(dataMutex_); adifProvider_ = p; }
  void setMapReloadFlag(std::atomic<bool> *flag) { std::lock_guard<std::mutex> lk(dataMutex_); mapReloadFlag_ = flag; }
  void setStopwatch(StopwatchPanel *s) { std::lock_guard<std::mutex> lk(dataMutex_); stopwatch_ = s; }
  void setCalendarStore(std::shared_ptr<CalendarStore> s) {
    calendarStore_ = std::move(s);
  }
  void setTimePanel(TimePanel *tp) { std::lock_guard<std::mutex> lk(dataMutex_); timePanel_ = tp; }
  void setRssBanner(RSSBanner *rb) { std::lock_guard<std::mutex> lk(dataMutex_); rssBanner_ = rb; }
  void setRssProvider(RSSProvider *p) { std::lock_guard<std::mutex> lk(dataMutex_); rssProvider_ = p; }
  void setPaneExpandControl(std::atomic<int> *cmd) { std::lock_guard<std::mutex> lk(dataMutex_); paneExpandCmd_ = cmd; }

  // Pane set commands (solo/add/remove/next) must be applied on the main/render
  // thread because setRotation() → onResize() may destroy SDL textures.
  struct PendingPaneSet {
    int pane = -1;          // 0-based pane index
    int action = 0;         // 1=next, 2=add, 3=remove, 4=solo
    std::string widget;     // widget typeId (empty for action=next)
  };
  // Called from the main thread: swaps out the entire pending queue in one
  // lock acquisition so rapid-fire API calls (e.g. 6 pane resets) all land.
  std::vector<PendingPaneSet> drainPendingPaneSets() {
    std::lock_guard<std::mutex> lk(dataMutex_);
    std::vector<PendingPaneSet> out;
    out.swap(pendingPaneSets_);
    return out;
  }
  void setBMEProvider(BME280Provider *bme) { std::lock_guard<std::mutex> lk(dataMutex_); bmeProvider_ = bme; }
  void setMarineStore(std::shared_ptr<MarineStore> s) {
    std::lock_guard<std::mutex> lk(dataMutex_);
    marineStore_ = std::move(s);
  }

private:
  void run();
  void registerRoutes(httplib::Server &svr);

  std::mutex dataMutex_;
  SDL_Renderer *renderer_;
  AppConfig *cfg_;
  HamClockState *state_;
  ConfigManager *cfgMgr_;
  std::shared_ptr<WatchlistStore> watchlist_;
  std::shared_ptr<SolarDataStore> solar_;
  std::shared_ptr<ContestStore> contests_;
  std::shared_ptr<DXClusterDataStore> dxc_;
  std::shared_ptr<LiveSpotDataStore> spots_;
  std::shared_ptr<CPUMonitor> cpu_;
  std::shared_ptr<DisplayPower> displayPower_;
  std::atomic<bool> *reloadFlag_; // points to AppContext::configReloadRequested
  std::atomic<bool> *mapReloadFlag_ = nullptr;
  FrameCapture *frameCapture_ = nullptr;
  NetworkManager *netMgr_ = nullptr;
  ActivityDataStore *activityStore_ = nullptr;
  SatelliteManager *satMgr_ = nullptr;
  RotatorService *rotatorSvc_ = nullptr;
  std::atomic<int> *rotationCmd_ = nullptr;
  std::atomic<int> *rotationCmdPane_ = nullptr;
  std::atomic<int> *rotationCmdWidget_ = nullptr;
  std::vector<std::unique_ptr<PaneContainer>> *panes_ = nullptr;
  std::shared_ptr<WeatherStore> weatherStore_;
  std::shared_ptr<BrightnessManager> brightnessMgr_;
  BME280Provider *bmeProvider_ = nullptr;
  ADIFProvider *adifProvider_ = nullptr;
  StopwatchPanel *stopwatch_ = nullptr;
  std::shared_ptr<CalendarStore> calendarStore_;
  TimePanel *timePanel_ = nullptr;
  RSSBanner *rssBanner_ = nullptr;
  RSSProvider *rssProvider_ = nullptr;
  std::atomic<int> *paneExpandCmd_ = nullptr;
  std::vector<PendingPaneSet> pendingPaneSets_;  // guarded by dataMutex_
  std::shared_ptr<MarineStore> marineStore_;
  bool screenLocked_ = false;
  bool liveWebEnabled_ = false;
  int port_;
  std::thread thread_;
  std::atomic<bool> running_{false};
  std::chrono::system_clock::time_point startTime_ =
      std::chrono::system_clock::now();

  void *svrPtr_ = nullptr; // httplib::Server*
};
