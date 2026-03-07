#pragma once

#include "../core/HistoryData.h"
#include "FontManager.h"
#include "TextureManager.h"
#include "Widget.h"
#include <memory>
#include <string>

struct SDL_Renderer;

class HistoryPanel : public Widget {
public:
  HistoryPanel(int x, int y, int w, int h, FontManager &fontMgr,
               TextureManager &texMgr, std::shared_ptr<HistoryStore> store,
               const std::string &seriesName);

  std::string getName() const override { return "History"; }
  void update() override;
  void render(SDL_Renderer *renderer) override;
  void onResize(int x, int y, int w, int h) override;
  void onMouseMove(int mx, int my) override;

private:
  FontManager &fontMgr_;
  TextureManager &texMgr_;
  std::shared_ptr<HistoryStore> store_;
  std::string seriesName_;
  HistorySeries currentSeries_;
};
