#pragma once

#include "../core/DXClusterData.h"
#include "ListPanel.h"
#include <SDL.h>
#include <chrono>
#include <functional>
#include <memory>

// Forward declarations
class RigService;
struct AppConfig;

class DXClusterPanel : public ListPanel {
public:
  DXClusterPanel(int x, int y, int w, int h, FontManager &fontMgr,
                 std::shared_ptr<DXClusterDataStore> store,
                 RigService *rigService = nullptr,
                 const AppConfig *config = nullptr);
  ~DXClusterPanel() override;

  void update() override;
  void onResize(int x, int y, int w, int h) override;
  bool onMouseUp(int mx, int my, Uint16 mod, int clicks) override;
  bool onMouseWheel(int scrollY) override;

  bool isSetupRequested() const { return setupRequested_; }
  void clearSetupRequest() { setupRequested_ = false; }

  void setOnSpotActivated(std::function<void(const DXClusterSpot &)> cb) {
    onSpotActivated_ = std::move(cb);
  }
  void setOnSpotDeactivated(std::function<void()> cb) {
    onSpotDeactivated_ = std::move(cb);
  }

  std::string getName() const override { return "DXCluster"; }
  std::vector<std::string> getActions() const override;
  bool performAction(const std::string &action) override;
  SDL_Rect getActionRect(const std::string &action) const override;
  nlohmann::json getDebugData() const override;
  void render(SDL_Renderer *renderer) override;

private:
  void rebuildRows(const DXClusterData &data);
  std::string
  formatAge(const std::chrono::system_clock::time_point &spottedAt) const;

  SDL_Color getRowColor(int index,
                        const SDL_Color &defaultColor) const override;

  std::function<void(const DXClusterSpot &)> onSpotActivated_;
  std::function<void()> onSpotDeactivated_;

  std::shared_ptr<DXClusterDataStore> store_;
  RigService *rigService_;
  const AppConfig *config_;
  std::chrono::system_clock::time_point lastUpdate_{};
  bool setupRequested_ = false;

  std::vector<std::string> allRows_;
  std::vector<double> allFreqs_;
  std::vector<double> visibleFreqs_;

  struct SpotDisplay {
    std::string call;
    double freq;
    std::chrono::system_clock::time_point time;
  };
  std::vector<SpotDisplay> allSpots_;
  std::vector<SpotDisplay> visibleSpots_;

  struct CachedSpot {
    SDL_Texture *freqTex = nullptr;
    int freqW = 0, freqH = 0;
    SDL_Texture *callTex = nullptr;
    int callW = 0, callH = 0;
    SDL_Texture *ageTex = nullptr;
    int ageW = 0, ageH = 0;
    std::string lastAge;
    double lastFreq = -1.0;
    std::string lastCall;
  };
  std::vector<CachedSpot> spotCache_;
  void clearSpotCache();

  int scrollOffset_ = 0;
  static constexpr int MAX_VISIBLE_ROWS = 15;

  int contentY_ = 0;  // Y where spot rows begin — set in render(), used in onMouseUp()
  int rowH_ = 14;     // Row height matching render — set in render(), used in onMouseUp()
};
