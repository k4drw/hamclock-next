#pragma once

#include "../core/Constants.h"
#include <atomic>
#include <memory>
#include <thread>
#include <vector>

// Forward declaration to avoid pulling SDL into the header
struct SDL_Renderer;

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
class ADIFProvider;

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

  void setFrameCapture(FrameCapture *fc) { frameCapture_ = fc; }
  void setLiveWebEnabled(bool enabled) { liveWebEnabled_ = enabled; }
  bool isLiveWebEnabled() const { return liveWebEnabled_; }
  void setNetworkManager(NetworkManager *nm) { netMgr_ = nm; }
  void setActivityStore(ActivityDataStore *a) { activityStore_ = a; }
  void setSatelliteManager(SatelliteManager *s) { satMgr_ = s; }
  void setRotatorService(RotatorService *r) { rotatorSvc_ = r; }
  void setRotationControl(std::atomic<int> *cmd, std::atomic<int> *pane,
                          std::atomic<int> *widget) {
    rotationCmd_ = cmd;
    rotationCmdPane_ = pane;
    rotationCmdWidget_ = widget;
  }
  void setPanes(std::vector<std::unique_ptr<PaneContainer>> *panes) {
    panes_ = panes;
  }
  void setWeatherStore(std::shared_ptr<WeatherStore> ws) { weatherStore_ = ws; }
  void setBrightnessManager(std::shared_ptr<BrightnessManager> bm) {
    brightnessMgr_ = bm;
  }
  void setADIFProvider(ADIFProvider *p) { adifProvider_ = p; }

private:
  void run();

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
  ADIFProvider *adifProvider_ = nullptr;
  bool screenLocked_ = false;
  bool liveWebEnabled_ = false;
  int port_;
  std::thread thread_;
  std::atomic<bool> running_{false};
  std::chrono::system_clock::time_point startTime_ =
      std::chrono::system_clock::now();

  void *svrPtr_ = nullptr; // httplib::Server*
};
