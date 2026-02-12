#pragma once

#include "../core/ADIFData.h"
#include "FontManager.h"
#include "Widget.h"
#include <memory>

class ADIFPanel : public Widget {
public:
  ADIFPanel(int x, int y, int w, int h, FontManager &fontMgr,
            std::shared_ptr<ADIFStore> store);

  void update() override;
  void render(SDL_Renderer *renderer) override;

private:
  FontManager &fontMgr_;
  std::shared_ptr<ADIFStore> store_;
  ADIFStats stats_;
};
