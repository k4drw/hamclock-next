#pragma once

#include "../core/DXClusterData.h"
#include "ListPanel.h"
#include <SDL.h>
#include <chrono>
#include <memory>

// Forward declarations
class RigService;
class AppConfig;

class DXClusterPanel : public ListPanel {
public:
  DXClusterPanel(int x, int y, int w, int h, FontManager &fontMgr,
                 std::shared_ptr<DXClusterDataStore> store,
                 RigService *rigService = nullptr,
                 const AppConfig *config = nullptr);

  void update() override;
  bool onMouseUp(int mx, int my, Uint16 mod) override;
  bool onMouseWheel(int scrollY) override;

  bool isSetupRequested() const { return setupRequested_; }
  void clearSetupRequest() { setupRequested_ = false; }

  std::string getName() const override { return "DXCluster"; }
  std::vector<std::string> getActions() const override;
  bool performAction(const std::string &action) override;
  SDL_Rect getActionRect(const std::string &action) const override;
  nlohmann::json getDebugData() const override;

private:
  void rebuildRows(const DXClusterData &data);
  std::string
  formatAge(const std::chrono::system_clock::time_point &spottedAt) const;

  SDL_Color getRowColor(int index,
                        const SDL_Color &defaultColor) const override;

  std::shared_ptr<DXClusterDataStore> store_;
  RigService *rigService_;
  const AppConfig *config_;
  std::chrono::system_clock::time_point lastUpdate_{};
  bool setupRequested_ = false;

  std::vector<std::string> allRows_;
  std::vector<double> allFreqs_;
  std::vector<double> visibleFreqs_;
  int scrollOffset_ = 0;
  static constexpr int MAX_VISIBLE_ROWS = 15;
};
