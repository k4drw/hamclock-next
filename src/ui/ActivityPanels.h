#pragma once

#include "../core/ActivityData.h"
#include "ListPanel.h"
#include <chrono>
#include <memory>

class DXPedPanel : public ListPanel {
public:
  DXPedPanel(int x, int y, int w, int h, FontManager &fontMgr,
             std::shared_ptr<ActivityDataStore> store);

  void update() override;

private:
  std::shared_ptr<ActivityDataStore> store_;
  std::chrono::system_clock::time_point lastUpdate_{};
};

class ONTAPanel : public ListPanel {
public:
  ONTAPanel(int x, int y, int w, int h, FontManager &fontMgr,
            std::shared_ptr<ActivityDataStore> store);

  void update() override;

private:
  std::shared_ptr<ActivityDataStore> store_;
  std::chrono::system_clock::time_point lastUpdate_{};
};
