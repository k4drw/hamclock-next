#pragma once

#include "../core/MoonData.h"
#include "FontManager.h"
#include "Widget.h"
#include <memory>

struct SDL_Renderer;

class EMEToolPanel : public Widget {
public:
  EMEToolPanel(int x, int y, int w, int h, FontManager &fontMgr,
               std::shared_ptr<MoonStore> store);

  void update() override;
  void render(SDL_Renderer *renderer) override;

  std::string getName() const override { return "EMEToolPanel"; }
  nlohmann::json getDebugData() const override;

private:
  FontManager &fontMgr_;
  std::shared_ptr<MoonStore> store_;
  MoonData currentData_;
};
