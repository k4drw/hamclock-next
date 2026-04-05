#pragma once

#include "../core/ConfigManager.h"
#include "FontManager.h"
#include "Widget.h"

#include <SDL.h>
#include <array>
#include <string>

class BigClockPanel : public Widget {
public:
  BigClockPanel(int x, int y, int w, int h, FontManager &fontMgr,
                AppConfig &config, ConfigManager &cfgMgr);
  ~BigClockPanel() override;

  void update() override;
  void render(SDL_Renderer *renderer) override;
  void onResize(int x, int y, int w, int h) override;
  bool onMouseUp(int mx, int my, Uint16 mod, int clicks) override;
  void setLineAATexture(SDL_Texture *tex) override { lineAATex_ = tex; }

  bool isModalActive() const override { return menuVisible_; }
  void renderModal(SDL_Renderer *renderer) override;

  std::string getName() const override { return "Big Clock"; }
  const char *typeId() const override { return "big_clock"; }
  std::string getDisplayName() const override { return "Big Clock"; }

private:
  FontManager   &fontMgr_;
  AppConfig     &config_;
  ConfigManager &cfgMgr_;
  SDL_Texture *lineAATex_ = nullptr;

  // Font sizes (recalculated on resize)
  int hmPtSize_   = 60;
  int secPtSize_  = 36;
  int datePtSize_ = 12;

  // Texture cache for digital mode (caller-owned, destroyed by us)
  SDL_Texture *hmTex_  = nullptr;
  SDL_Texture *secTex_ = nullptr;
  int hmW_ = 0, hmH_ = 0;
  int secW_ = 0, secH_ = 0;
  std::string lastHM_;
  std::string lastSec_;

  // Modal state
  bool menuVisible_ = false;

  // Temp settings while modal is open (committed on Done)
  bool    tmpDigital_  = true;
  bool    tmp12h_      = false;
  bool    tmpUtc_      = false;
  bool    tmpUseDefaultTz_ = false;
  bool    tmpShowSec_  = true;
  bool    tmpShowDate_ = true;
  uint8_t tmpHue_      = 85;

  // Modal layout rects
  SDL_Rect modalRect_  = {};
  SDL_Rect doneRect_   = {};
  SDL_Rect cancelRect_ = {};
  static constexpr int kNumOptions = 6;
  SDL_Rect optRects_[kNumOptions] = {};
  static constexpr int kNumSwatches = 8;
  SDL_Rect swatchRects_[kNumSwatches] = {};
  static constexpr std::array<uint8_t, kNumSwatches> kSwatchHues = {0, 32, 64, 85, 128, 160, 192, 224};

  static SDL_Color hueToColor(uint8_t hue);
  void invalidateTextures();
  void recalcModalLayout();
  void renderDigital(SDL_Renderer *renderer);
  void renderAnalog(SDL_Renderer *renderer);
  void renderModalInternal(SDL_Renderer *renderer);

  static bool pointInRect(int mx, int my, const SDL_Rect &r) {
    return mx >= r.x && mx < r.x + r.w && my >= r.y && my < r.y + r.h;
  }
};
