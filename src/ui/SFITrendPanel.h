#pragma once

#include "../core/SFIHistoryData.h"
#include "../core/SolarData.h"
#include "FontManager.h"
#include "Widget.h"
#include <memory>
#include <string>

struct SDL_Renderer;

class SFITrendPanel : public Widget {
public:
  SFITrendPanel(int x, int y, int w, int h, FontManager &fontMgr,
                std::shared_ptr<SFIHistoryStore> sfiHistoryStore,
                std::shared_ptr<SolarDataStore> solarStore);

  std::string getName() const override { return "SFI Trend"; }
  const char *typeId() const override { return "sfi_trend"; }
  std::string getDisplayName() const override { return "SFI 30-Day"; }
  void update() override;
  void render(SDL_Renderer *renderer) override;
  void onResize(int x, int y, int w, int h) override;
  void onMouseMove(int mx, int my) override;

private:
  FontManager &fontMgr_;
  std::shared_ptr<SFIHistoryStore> sfiHistoryStore_;
  std::shared_ptr<SolarDataStore> solarStore_;
};
