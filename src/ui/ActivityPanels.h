#pragma once

#include "../services/ActivityProvider.h"
#include "ListPanel.h"
#include <SDL.h>
#include <chrono>
#include <memory>

class DXPedPanel : public ListPanel {
public:
  DXPedPanel(int x, int y, int w, int h, FontManager &fontMgr,
             ActivityProvider &provider,
             std::shared_ptr<ActivityDataStore> store);

  void update() override;

private:
  ActivityProvider &provider_;
  std::shared_ptr<ActivityDataStore> store_;
  std::chrono::system_clock::time_point lastUpdate_{};
  uint32_t lastFetch_ = 0;
};

class ONTAPanel : public ListPanel {
public:
  ONTAPanel(int x, int y, int w, int h, FontManager &fontMgr,
            ActivityProvider &provider,
            std::shared_ptr<ActivityDataStore> store);

  void update() override;

private:
  ActivityProvider &provider_;
  std::shared_ptr<ActivityDataStore> store_;
  std::chrono::system_clock::time_point lastUpdate_{};
  uint32_t lastFetch_ = 0;
};
