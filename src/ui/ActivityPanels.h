#pragma once

#include "../services/ActivityProvider.h"
#include "ListPanel.h"
#include <SDL.h>
#include <chrono>
#include <functional>
#include <memory>
#include <string>

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
  enum class Filter { ALL, POTA, SOTA };

  ONTAPanel(int x, int y, int w, int h, FontManager &fontMgr,
            ActivityProvider &provider,
            std::shared_ptr<ActivityDataStore> store);

  void update() override;
  void render(SDL_Renderer *renderer) override;
  bool onMouseUp(int mx, int my, Uint16 mod) override;

  // Set initial filter from persisted config value ("all", "pota", "sota").
  void setFilter(const std::string &f);

  // Called when the user changes the filter; arg is the new string value.
  void setOnFilterChanged(std::function<void(const std::string &)> cb) {
    onFilterChanged_ = std::move(cb);
  }

  std::string getName() const override { return "ONTAPanel"; }

private:
  void rebuildRows(const ActivityData &data);
  static const char *filterLabel(Filter f);

  ActivityProvider &provider_;
  std::shared_ptr<ActivityDataStore> store_;
  std::chrono::system_clock::time_point lastUpdate_{};
  uint32_t lastFetch_ = 0;

  Filter filter_ = Filter::ALL;
  SDL_Rect chipRect_ = {0, 0, 0, 0};
  std::function<void(const std::string &)> onFilterChanged_;
};
