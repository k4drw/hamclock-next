#pragma once

#include "../core/MoonData.h"
#include "../network/NetworkManager.h"
#include "FontManager.h"
#include "TextureManager.h"
#include "Widget.h"
#include <memory>
#include <mutex>
#include <string>

struct SDL_Renderer;

class MoonPanel : public Widget {
public:
  MoonPanel(int x, int y, int w, int h, FontManager &fontMgr,
            TextureManager &texMgr, NetworkManager &net,
            std::shared_ptr<MoonStore> store);

  void update() override;
  void render(SDL_Renderer *renderer) override;
  void onResize(int x, int y, int w, int h) override;

private:
  FontManager &fontMgr_;
  TextureManager &texMgr_;
  NetworkManager &net_;
  std::shared_ptr<MoonStore> store_;
  MoonData currentData_;
  bool dataValid_ = false;

  std::string lastImageUrl_;
  bool imageLoading_ = false;
  std::string pendingImageData_;
  std::mutex imageMutex_;

  void drawMoon(SDL_Renderer *renderer, int cx, int cy, int r);

  int labelFontSize_ = 12;
  int valueFontSize_ = 14;
};
