#pragma once

#include "../core/SantaData.h"
#include "FontManager.h"
#include "Widget.h"
#include <memory>

struct SDL_Renderer;

class SantaPanel : public Widget {
public:
  SantaPanel(int x, int y, int w, int h, FontManager &fontMgr,
             std::shared_ptr<SantaStore> store);

  void update() override;
  void render(SDL_Renderer *renderer) override;

private:
  FontManager &fontMgr_;
  std::shared_ptr<SantaStore> store_;
  SantaData currentData_;
};
