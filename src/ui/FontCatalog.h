#pragma once

#include "FontManager.h"

#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>

#include <algorithm>
#include <string>
#include <vector>

// Named font styles modeled after the original HamClock typography.
//   SmallRegular / SmallBold → general UI text (~43px line height at 800x480)
//   LargeBold               → clock digits    (~80px line height at 800x480)
//   Fast                    → compact/debug   (~15px line height at 800x480)
enum class FontStyle { SmallRegular, SmallBold, LargeBold, Fast, Count_ };

class FontCatalog {
public:
  explicit FontCatalog(FontManager &fontMgr) : fontMgr_(fontMgr) {}

  // Recalculate scaled point sizes for the given window dimensions.
  // Call once at startup and on every resize.
  void recalculate(int /*winW*/, int winH) {
    float scale = static_cast<float>(winH) / kLogicalH;
    scaledPt_[idx(FontStyle::SmallRegular)] = clampPt(kSmallBasePt * scale);
    scaledPt_[idx(FontStyle::SmallBold)] = clampPt(kSmallBasePt * scale);
    scaledPt_[idx(FontStyle::LargeBold)] = clampPt(kLargeBasePt * scale);
    scaledPt_[idx(FontStyle::Fast)] = clampPt(kFastBasePt * scale);
  }

  // Current scaled point size for a style.
  int ptSize(FontStyle style) const { return scaledPt_[idx(style)]; }

  // Whether the style requests bold rendering.
  static bool isBold(FontStyle style) {
    return style == FontStyle::SmallBold || style == FontStyle::LargeBold;
  }

  // Render text with the named style (handles bold via TTF_SetFontStyle).
  // Caller owns the returned texture.
  SDL_Texture *renderText(SDL_Renderer *renderer, const std::string &text,
                          SDL_Color color, FontStyle style) {
    if (text.empty())
      return nullptr;
    return fontMgr_.renderText(renderer, text, color, ptSize(style), nullptr,
                               nullptr, isBold(style));
  }

  // Convenience: render + blit + destroy (one-off draws only).
  void drawText(SDL_Renderer *renderer, const std::string &text, int x, int y,
                SDL_Color color, FontStyle style) {
    SDL_Texture *tex = renderText(renderer, text, color, style);
    if (!tex)
      return;
    int w, h;
    SDL_QueryTexture(tex, nullptr, nullptr, &w, &h);
    SDL_Rect dst = {x, y, w, h};
    SDL_RenderCopy(renderer, tex, nullptr, &dst);
    SDL_DestroyTexture(tex);
  }

  // ---- Calibration ----

  struct CalibEntry {
    const char *name;
    int targetHeight; // at 800x480
    int basePt;
    int scaledPt;
    int measuredHeight; // TTF_FontHeight at scaledPt
  };

  // Measure current font heights for comparison against targets.
  std::vector<CalibEntry> calibrate() {
    struct Info {
      FontStyle style;
      const char *name;
      int target;
      int basePt;
    };
    static const Info infos[] = {
        {FontStyle::SmallRegular, "SmallRegular", kSmallTargetH, kSmallBasePt},
        {FontStyle::SmallBold, "SmallBold", kSmallTargetH, kSmallBasePt},
        {FontStyle::LargeBold, "LargeBold", kLargeTargetH, kLargeBasePt},
        {FontStyle::Fast, "Fast", kFastTargetH, kFastBasePt},
    };
    std::vector<CalibEntry> entries;
    for (const auto &i : infos) {
      int pt = ptSize(i.style);
      TTF_Font *font = fontMgr_.getFont(pt);
      int h = font ? TTF_FontHeight(font) : 0;
      entries.push_back({i.name, i.target, i.basePt, pt, h});
    }
    return entries;
  }

  // Target line heights in the 800x480 logical space.
  static constexpr int kSmallTargetH = 43;
  static constexpr int kLargeTargetH = 80;
  static constexpr int kFastTargetH = 15;
  static constexpr int kLogicalH = 480;

private:
  static constexpr int kStyleCount = static_cast<int>(FontStyle::Count_);

  // Base point sizes at 800x480.  Tuned so TTF_FontHeight ≈ target.
  // Adjust these constants if the font file (assets/font.ttf) changes.
  static constexpr int kSmallBasePt = 33;
  static constexpr int kLargeBasePt = 60;
  static constexpr int kFastBasePt = 11;

  static int idx(FontStyle s) { return static_cast<int>(s); }
  static int clampPt(float v) {
    return std::clamp(static_cast<int>(v), 8, 200);
  }

  FontManager &fontMgr_;
  int scaledPt_[kStyleCount] = {kSmallBasePt, kSmallBasePt, kLargeBasePt,
                                kFastBasePt};
};
