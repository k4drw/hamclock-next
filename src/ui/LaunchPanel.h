#pragma once

#include "Widget.h"
#include "WidgetDeps.h"
#include "../services/LaunchProvider.h"

class LaunchPanel : public Widget {
public:
  LaunchPanel(int x, int y, int w, int h, FontManager &fontMgr, LaunchProvider *launchProvider);
  ~LaunchPanel() override = default;

  void update() override;
  void render(SDL_Renderer *renderer) override;
  void onMouseMove(int mx, int my) override;

private:
  FontManager &fontMgr_;
  LaunchProvider *launchProvider_;
  std::vector<LaunchEvent> upcomingLaunches_;
};
