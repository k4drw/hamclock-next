#pragma once

#include "../core/Constants.h"
#include "../core/MemoryMonitor.h"
#include "FontManager.h"
#include <SDL.h>
#include <SDL_ttf.h>

#include <algorithm>
#include <string>
#include <vector>

// Named font styles modeled after the original HamClock typography.
//   SmallRegular / SmallBold → general UI text (~43px line height at 800x480)
//   LargeBold               → clock digits    (~80px line height at 800x480)
//   Fast                    → compact/debug   (~15px line height at 800x480)
//   FastBold                → compact/debug   (~15px line height at 800x480)
enum class FontStyle {
  Micro,
  SmallRegular,
  SmallBold,
  MediumRegular,
  MediumBold,
  LargeBold,
  Fast,
  FastBold,
  Count_
};

class FontCatalog {
public:
  explicit FontCatalog(FontManager &fontMgr) : fontMgr_(fontMgr) {}

  // Recalculate scaled point sizes for the given window dimensions.
  // Call once at startup and on every resize.
  void recalculate(int /*winW*/, int winH) {
    if (HamClock::FIDELITY_MODE) {
      // In Fidelity Mode, keep pt sizes logical (800x480).
      // FontManager handles super-sampling via renderScale_.
      scaledPt_[idx(FontStyle::Micro)] = kMicroBasePt;
      scaledPt_[idx(FontStyle::SmallRegular)] = kSmallBasePt;
      scaledPt_[idx(FontStyle::SmallBold)] = kSmallBasePt;
      scaledPt_[idx(FontStyle::MediumRegular)] = kMediumBasePt;
      scaledPt_[idx(FontStyle::MediumBold)] = kMediumBasePt;
      scaledPt_[idx(FontStyle::LargeBold)] = kLargeBasePt;
      scaledPt_[idx(FontStyle::Fast)] = kFastBasePt;
      scaledPt_[idx(FontStyle::FastBold)] = kFastBasePt;
    } else {
      float scale = static_cast<float>(winH) / HamClock::LOGICAL_HEIGHT;
      scaledPt_[idx(FontStyle::Micro)] = clampPt(kMicroBasePt * scale);
      scaledPt_[idx(FontStyle::SmallRegular)] = clampPt(kSmallBasePt * scale);
      scaledPt_[idx(FontStyle::SmallBold)] = clampPt(kSmallBasePt * scale);
      scaledPt_[idx(FontStyle::MediumRegular)] = clampPt(kMediumBasePt * scale);
      scaledPt_[idx(FontStyle::MediumBold)] = clampPt(kMediumBasePt * scale);
      scaledPt_[idx(FontStyle::LargeBold)] = clampPt(kLargeBasePt * scale);
      scaledPt_[idx(FontStyle::Fast)] = clampPt(kFastBasePt * scale);
      scaledPt_[idx(FontStyle::FastBold)] = clampPt(kFastBasePt * scale);
    }
  }

  // Current scaled point size for a style.
  int ptSize(FontStyle style) const { return scaledPt_[idx(style)]; }

  // Whether the style requests bold rendering.
  static bool isBold(FontStyle style) {
    return style == FontStyle::SmallBold || style == FontStyle::MediumBold ||
           style == FontStyle::LargeBold || style == FontStyle::FastBold;
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
    destroyTexture(tex);
  }

  void destroyTexture(SDL_Texture *tex) {
    if (tex) {
      int w, h;
      SDL_QueryTexture(tex, nullptr, nullptr, &w, &h);
      MemoryMonitor::getInstance().markVramDestroyed(static_cast<int64_t>(w) *
                                                     h * 4);
      SDL_DestroyTexture(tex);
    }
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
        {FontStyle::FastBold, "FastBold", kFastTargetH, kFastBasePt},
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
  static constexpr int kMicroTargetH = 12;
  static constexpr int kSmallTargetH = 18;
  static constexpr int kMediumTargetH = 28;
  static constexpr int kLargeTargetH = 80;
  static constexpr int kFastTargetH = 15;

private:
  static constexpr int kStyleCount = static_cast<int>(FontStyle::Count_);

  // Base point sizes at 800x480.  Tuned so TTF_FontHeight ≈ target.
  // Adjust these constants if the embedded font changes.
  static constexpr int kMicroBasePt = 10;
  static constexpr int kSmallBasePt = 14;
  static constexpr int kMediumBasePt = 24;
  static constexpr int kLargeBasePt = 60;
  static constexpr int kFastBasePt = 12;

  static int idx(FontStyle s) { return static_cast<int>(s); }
  static int clampPt(float v) {
    return std::clamp(static_cast<int>(v), 8, 200);
  }

  FontManager &fontMgr_;
  int scaledPt_[kStyleCount] = {kSmallBasePt, kSmallBasePt, kLargeBasePt,
                                kFastBasePt};
};
