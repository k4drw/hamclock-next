#pragma once

#include "../core/DXClusterData.h"
#include "ListPanel.h"
#include <chrono>
#include <memory>

class DXClusterPanel : public ListPanel {
public:
  DXClusterPanel(int x, int y, int w, int h, FontManager &fontMgr,
                 std::shared_ptr<DXClusterDataStore> store);

  void update() override;
  bool onMouseUp(int mx, int my, Uint16 mod) override;

  bool isSetupRequested() const { return setupRequested_; }
  void clearSetupRequest() { setupRequested_ = false; }

private:
  std::shared_ptr<DXClusterDataStore> store_;
  std::chrono::system_clock::time_point lastUpdate_{};
  bool setupRequested_ = false;
};
