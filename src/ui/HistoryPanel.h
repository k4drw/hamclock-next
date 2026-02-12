#pragma once

#include "../core/HistoryData.h"
#include "FontManager.h"
#include "Widget.h"
#include <memory>
#include <string>

class HistoryPanel : public Widget {
public:
  HistoryPanel(int x, int y, int w, int h, FontManager &fontMgr,
               std::shared_ptr<HistoryStore> store,
               const std::string &seriesName);

  void update() override;
  void render(SDL_Renderer *renderer) override;

private:
  FontManager &fontMgr_;
  std::shared_ptr<HistoryStore> store_;
  std::string seriesName_;
  HistorySeries currentSeries_;
};
