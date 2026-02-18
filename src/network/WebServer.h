#pragma once

#include "../core/Constants.h"
#include <atomic>
#include <memory>
#include <mutex>
#include <thread>

// Forward declaration to avoid pulling SDL into the header
struct SDL_Renderer;

struct AppConfig;
struct HamClockState;
class ConfigManager;
class WatchlistStore;
class SolarDataStore;
class DisplayPower;

class WebServer {
public:
  WebServer(SDL_Renderer *renderer, AppConfig &cfg, HamClockState &state,
            ConfigManager &cfgMgr, std::shared_ptr<DisplayPower> displayPower,
            std::atomic<bool> &reloadFlag,
            std::shared_ptr<WatchlistStore> watchlist = nullptr,
            std::shared_ptr<SolarDataStore> solar = nullptr,
            int port = HamClock::DEFAULT_WEB_SERVER_PORT);
  ~WebServer();

  void start();
  void stop();

private:
  void run();

  SDL_Renderer *renderer_;
  AppConfig *cfg_;
  HamClockState *state_;
  ConfigManager *cfgMgr_;
  std::shared_ptr<WatchlistStore> watchlist_;
  std::shared_ptr<SolarDataStore> solar_;
  std::shared_ptr<DisplayPower> displayPower_;
  std::atomic<bool> *reloadFlag_; // points to AppContext::configReloadRequested
  int port_;
  std::thread thread_;
  std::atomic<bool> running_{false};

  void *svrPtr_ = nullptr; // httplib::Server*
};
