#pragma once

#include <SDL.h>
#include <atomic>
#include <memory>
#include <mutex>
#include <thread>
#include <vector>

struct AppConfig;
struct HamClockState;
class ConfigManager;
class WatchlistStore;
class SolarDataStore;

class WebServer {
public:
  WebServer(SDL_Renderer *renderer, AppConfig &cfg, HamClockState &state,
            ConfigManager &cfgMgr,
            std::shared_ptr<WatchlistStore> watchlist = nullptr,
            std::shared_ptr<SolarDataStore> solar = nullptr, int port = 8080);
  ~WebServer();

  void start();
  void stop();

  // Call this once per frame from main thread to update the web mirror
  void updateFrame();

private:
  void run();

  SDL_Renderer *renderer_;
  AppConfig *cfg_;
  HamClockState *state_;
  ConfigManager *cfgMgr_;
  std::shared_ptr<WatchlistStore> watchlist_;
  std::shared_ptr<SolarDataStore> solar_;
  int port_;
  std::thread thread_;
  std::atomic<bool> running_{false};

  // The latest captured JPEG
  std::vector<unsigned char> latestJpeg_;
  std::mutex jpegMutex_;

  // Timing to avoid capturing every single frame if no one is watching
  uint32_t lastCaptureTicks_ = 0;
  std::atomic<bool> needsCapture_{true};
  void *svrPtr_ = nullptr; // httplib::Server*
};
