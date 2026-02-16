#pragma once

#include "../core/DstData.h"
#include "FontManager.h"
#include "Widget.h"
#include <memory>

struct SDL_Renderer;

class DstPanel : public Widget {
public:
  DstPanel(int x, int y, int w, int h, FontManager &fontMgr,
           std::shared_ptr<DstStore> store);

  void update() override;
  void render(SDL_Renderer *renderer) override;

  std::string getName() const override { return "DstPanel"; }
  nlohmann::json getDebugData() const override;

private:
  FontManager &fontMgr_;
  std::shared_ptr<DstStore> store_;
  DstData currentData_;
};
