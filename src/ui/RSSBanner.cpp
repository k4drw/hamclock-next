#include "RSSBanner.h"
#include "../core/MemoryMonitor.h"
#include "../core/Theme.h"
#include "FontCatalog.h"

#include <cstdio>

RSSBanner::RSSBanner(int x, int y, int w, int h, FontManager &fontMgr,
                     std::shared_ptr<RSSDataStore> store)
    : Widget(x, y, w, h), fontMgr_(fontMgr), store_(std::move(store)),
      lastRotateMs_(SDL_GetTicks()) {
  auto *cat = fontMgr_.catalog();
  if (cat)
    fontSize_ = cat->ptSize(FontStyle::SmallRegular);
}

void RSSBanner::update() {
  auto data = store_->get();
  if (data.valid && data.headlines != lastHeadlines_) {
    lastHeadlines_ = data.headlines;
    currentIdx_ = 0;
    lastRotateMs_ = SDL_GetTicks();
    rebuildTextures(nullptr); // Initial trigger will happen in render
  }

  Uint32 now = SDL_GetTicks();
  if (now - lastRotateMs_ >= kRotateIntervalMs) {
    lastRotateMs_ = now;
    if (!lastHeadlines_.empty()) {
      currentIdx_ = (currentIdx_ + 1) % lastHeadlines_.size();
      rebuildTextures(nullptr); // Will be rebuilt in next render pass
    }
  }
}

void RSSBanner::render(SDL_Renderer *renderer) {
  // If we need to rebuild textures for the current headline
  if (currentLines_.empty() && !lastHeadlines_.empty()) {
    rebuildTextures(renderer);
  }

  if (currentLines_.empty())
    return;

  ThemeColors themes = getThemeColors(theme_);

  // Background
  SDL_SetRenderDrawBlendMode(
      renderer, (theme_ == "glass") ? SDL_BLENDMODE_BLEND : SDL_BLENDMODE_NONE);
  SDL_SetRenderDrawColor(renderer, themes.bg.r, themes.bg.g, themes.bg.b,
                         themes.bg.a);
  SDL_Rect rect = {x_, y_, width_, height_};
  SDL_RenderFillRect(renderer, &rect);

  // Draw pane border
  SDL_SetRenderDrawColor(renderer, themes.border.r, themes.border.g,
                         themes.border.b, themes.border.a);
  SDL_RenderDrawRect(renderer, &rect);

  // Clip to banner bounds
  SDL_Rect clipRect = {x_, y_, width_, height_};
  SDL_RenderSetClipRect(renderer, &clipRect);

  int startY = y_ + (height_ - totalLineHeight_) / 2;
  int curY = startY;

  for (const auto &line : currentLines_) {
    if (line.tex) {
      int curX = x_ + (width_ - line.w) / 2;
      SDL_Rect dst = {curX, curY, line.w, line.h};
      SDL_RenderCopy(renderer, line.tex, nullptr, &dst);
      curY += line.h;
    }
  }

  SDL_RenderSetClipRect(renderer, nullptr);
}

void RSSBanner::onResize(int x, int y, int w, int h) {
  Widget::onResize(x, y, w, h);
  auto *cat = fontMgr_.catalog();
  if (cat)
    fontSize_ = cat->ptSize(FontStyle::SmallRegular);
  destroyCache();
}

void RSSBanner::destroyCache() {
  for (auto &line : currentLines_) {
    if (line.tex)
      MemoryMonitor::getInstance().destroyTexture(line.tex);
  }
  currentLines_.clear();
  totalLineHeight_ = 0;
}

void RSSBanner::rebuildTextures(SDL_Renderer *renderer) {
  if (!renderer) {
    // Only clear, letting render() rebuild
    destroyCache();
    return;
  }

  destroyCache();
  if (lastHeadlines_.empty() || currentIdx_ >= (int)lastHeadlines_.size())
    return;

  std::string fullText = lastHeadlines_[currentIdx_];
  ThemeColors themes = getThemeColors(theme_);
  SDL_Color textColor = themes.accent;

  // CRITICAL: Limit text to prevent texture creation failures on RPi
  // Maximum texture dimension on KMSDRM is 2048px
  static constexpr int kMaxTextureWidth = 2048;
  static constexpr int kMaxCharWidth = 20; // Approximate max pixels per char

  // Estimate maximum safe character length
  int maxChars = (kMaxTextureWidth - 100) / kMaxCharWidth; // Leave margin

  if (fullText.size() > maxChars) {
    fullText = fullText.substr(0, maxChars - 3) + "...";
  }

  // 1. Try single line
  int w = 0, h = 0;
  SDL_Texture *tex =
      fontMgr_.renderText(renderer, fullText, textColor, fontSize_, &w, &h);

  if (tex && w <= width_ - 20 && w < kMaxTextureWidth) {
    currentLines_.push_back({tex, w, h});
    totalLineHeight_ = h;
  } else {
    // 2. Wrap to 2 lines if possible
    if (tex)
      MemoryMonitor::getInstance().destroyTexture(tex);

    // If texture was too wide, further truncate for wrapping
    if (w >= kMaxTextureWidth && fullText.size() > 60) {
      fullText = fullText.substr(0, 60) + "...";
    }

    // Naive word wrap for 2 lines
    size_t mid = fullText.length() / 2;
    size_t split = fullText.find_last_of(" \t\r\n", mid);
    if (split == std::string::npos)
      split = fullText.find_first_of(" \t\r\n", mid);

    std::string l1, l2;
    if (split != std::string::npos) {
      l1 = fullText.substr(0, split);
      l2 = fullText.substr(split + 1);
    } else {
      l1 = fullText;
    }

    // Truncate individual lines if still too long
    int perLineMaxChars = maxChars / 2;
    if (l1.size() > perLineMaxChars) {
      l1 = l1.substr(0, perLineMaxChars - 3) + "...";
    }
    if (l2.size() > perLineMaxChars) {
      l2 = l2.substr(0, perLineMaxChars - 3) + "...";
    }

    // Use smaller font for 2 lines if needed
    int wrapFontSize = (fontSize_ > 20) ? fontSize_ * 0.7 : fontSize_;

    int w1 = 0, h1 = 0;
    SDL_Texture *t1 =
        fontMgr_.renderText(renderer, l1, textColor, wrapFontSize, &w1, &h1);
    if (t1 && w1 < kMaxTextureWidth) {
      currentLines_.push_back({t1, w1, h1});
      totalLineHeight_ += h1;
    } else if (t1) {
      MemoryMonitor::getInstance().destroyTexture(
          t1); // Texture still too wide, discard
    }

    if (!l2.empty()) {
      int w2 = 0, h2 = 0;
      SDL_Texture *t2 =
          fontMgr_.renderText(renderer, l2, textColor, wrapFontSize, &w2, &h2);
      if (t2 && w2 < kMaxTextureWidth) {
        currentLines_.push_back({t2, w2, h2});
        totalLineHeight_ += h2;
      } else if (t2) {
        MemoryMonitor::getInstance().destroyTexture(
            t2); // Texture still too wide, discard
      }
    }
  }
}
