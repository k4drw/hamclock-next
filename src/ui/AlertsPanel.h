#pragma once

#include "../core/AlertsData.h"
#include "FontManager.h"
#include "Widget.h"
#include <memory>
#include <string>

struct SDL_Renderer;

class AlertsPanel : public Widget {
public:
  AlertsPanel(int x, int y, int w, int h, FontManager &fontMgr,
              std::shared_ptr<AlertsStore> store);

  void update() override;
  void render(SDL_Renderer *renderer) override;
  void onResize(int x, int y, int w, int h) override;
  bool onMouseWheel(int scrollY) override;

  std::string getName() const override { return "WeatherAlerts"; }

private:
  SDL_Color severityColor(const std::string &severity) const;

  FontManager &fontMgr_;
  std::shared_ptr<AlertsStore> store_;
  AlertsData currentData_;
  int scrollOffset_ = 0;
  int maxScroll_ = 0;

  int titleFontSize_ = 12;
  int rowFontSize_ = 11;
};
