#pragma once

#include "../core/RSSData.h"
#include "FontManager.h"
#include "Widget.h"

#include <memory>
#include <string>
#include <vector>

class RSSBanner : public Widget {
public:
  RSSBanner(int x, int y, int w, int h, FontManager &fontMgr,
            std::shared_ptr<RSSDataStore> store);
  ~RSSBanner() override { destroyCache(); }

  void update() override;
  void render(SDL_Renderer *renderer) override;
  void onResize(int x, int y, int w, int h) override;

private:
  void destroyCache();
  void rebuildTextures(SDL_Renderer *renderer);

  FontManager &fontMgr_;
  std::shared_ptr<RSSDataStore> store_;

  // Display state
  int currentIdx_ = 0;
  Uint32 lastRotateMs_ = 0;
  static constexpr Uint32 kRotateIntervalMs = 5000;

  // Headline textures for current display
  struct Line {
    SDL_Texture *tex = nullptr;
    int w = 0;
    int h = 0;
  };
  std::vector<Line> currentLines_;
  int totalLineHeight_ = 0;

  // Track when headlines change
  std::vector<std::string> lastHeadlines_;

  // Font size for the banner
  int fontSize_ = 33;

  static constexpr float kScrollSpeed = 60.0f; // pixels per second
  static constexpr const char *kSeparator = " - ";
};
