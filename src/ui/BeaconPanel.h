#pragma once

#include "../services/BeaconProvider.h"
#include "FontManager.h"
#include "Widget.h"
#include <map>
#include <string>

struct SDL_Renderer;
struct SDL_Texture;

class BeaconPanel : public Widget {
public:
  BeaconPanel(int x, int y, int w, int h, FontManager &fontMgr,
              BeaconProvider &provider);
  ~BeaconPanel();

  void update() override;
  void render(SDL_Renderer *renderer) override;
  void onResize(int x, int y, int w, int h) override;

  void setTheme(const std::string &theme) override { theme_ = theme; }
  std::string getName() const override { return "BeaconPanel"; }

private:
  // Texture cache to prevent per-frame texture creation/destruction
  struct CachedText {
    SDL_Texture *texture = nullptr;
    int w = 0;
    int h = 0;
  };

  void clearTextCache();
  CachedText &getCachedText(SDL_Renderer *renderer, const std::string &key,
                            const std::string &text, SDL_Color color,
                            int fontSize, bool bold);

  FontManager &fontMgr_;
  BeaconProvider &provider_;

  std::vector<ActiveBeacon> active_;
  float progress_ = 0;

  int labelFontSize_ = 10;
  int callfontSize_ = 11;

  // For debug logging
  int lastSlot_ = -1;

  // Texture cache: key -> (texture, width, height)
  // Key format: "text_R_G_B_size_bold"
  std::map<std::string, CachedText> textCache_;
  int lastWidth_ = 0;
  int lastHeight_ = 0;
};
