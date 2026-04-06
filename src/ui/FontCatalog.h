#pragma once

#include "../core/MemoryMonitor.h"
#include "FontManager.h"
#include <SDL.h>
#include <SDL_ttf.h>

#include <algorithm>
#include <string>
#include <unordered_map>
#include <vector>

// Named font styles modeled after the original HamClock typography.
//   SmallRegular / SmallBold → general UI text (~43px line height at 800x480)
//   LargeBold               → clock digits    (~80px line height at 800x480)
//   Fast                    → compact/debug   (~15px line height at 800x480)
//   FastBold                → compact/debug   (~15px line height at 800x480)
enum class FontStyle {
  Tiny,
  TinyBold,
  Caption,
  CaptionBold,
  Micro,
  MicroBold,
  UI,
  UIBold,
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
  explicit FontCatalog(FontManager &fontMgr) : fontMgr_(fontMgr) {
    for (int i = 0; i < kStyleCount; ++i)
      scaledPt_[i] = kSmallBasePt;
  }

  // Recalculate scaled point sizes for the given window dimensions.
  // Call once at startup and on every resize.
  void recalculate(int /*winW*/, int /*winH*/) {
    // Evict texture cache: cached textures were rasterized at the old
    // renderScale_; on resize renderScale_ changes so they are all stale.
    for (auto &kv : textCache_)
      SDL_DestroyTexture(kv.second.tex);
    textCache_.clear();

    // We now keep all point sizes logical (800x480).
    // FontManager handles scaling via renderScale_.
    scaledPt_[idx(FontStyle::Tiny)] = kTinyBasePt;
    scaledPt_[idx(FontStyle::TinyBold)] = kTinyBasePt;
    scaledPt_[idx(FontStyle::Caption)] = kCaptionBasePt;
    scaledPt_[idx(FontStyle::CaptionBold)] = kCaptionBasePt;
    scaledPt_[idx(FontStyle::Micro)] = kMicroBasePt;
    scaledPt_[idx(FontStyle::MicroBold)] = kMicroBasePt;
    scaledPt_[idx(FontStyle::UI)] = kUIBasePt;
    scaledPt_[idx(FontStyle::UIBold)] = kUIBasePt;
    scaledPt_[idx(FontStyle::SmallRegular)] = kSmallBasePt;
    scaledPt_[idx(FontStyle::SmallBold)] = kSmallBasePt;
    scaledPt_[idx(FontStyle::MediumRegular)] = kMediumBasePt;
    scaledPt_[idx(FontStyle::MediumBold)] = kMediumBasePt;
    scaledPt_[idx(FontStyle::LargeBold)] = kLargeBasePt;
    scaledPt_[idx(FontStyle::Fast)] = kFastBasePt;
    scaledPt_[idx(FontStyle::FastBold)] = kFastBasePt;
  }

  // Current scaled point size for a style.
  int ptSize(FontStyle style) const { return scaledPt_[idx(style)]; }

  // Whether the style requests bold rendering.
  static bool isBold(FontStyle style) {
    return style == FontStyle::TinyBold || style == FontStyle::CaptionBold ||
           style == FontStyle::MicroBold || style == FontStyle::UIBold ||
           style == FontStyle::SmallBold || style == FontStyle::MediumBold ||
           style == FontStyle::LargeBold || style == FontStyle::FastBold;
  }

  // Render text with the named style (handles bold via TTF_SetFontStyle).
  // Caller owns the returned texture.
  SDL_Texture *renderText(SDL_Renderer *renderer, const std::string &text,
                          SDL_Color color, FontStyle style, int *outW = nullptr,
                          int *outH = nullptr) {
    if (text.empty())
      return nullptr;
    return fontMgr_.renderText(renderer, text, color, ptSize(style), outW, outH,
                               isBold(style));
  }

  // Destructor: release all cached textures.
  ~FontCatalog() {
    for (auto &kv : textCache_)
      SDL_DestroyTexture(kv.second.tex);
  }

  // Convenience: render + blit, using an internal LRU texture cache.
  // Avoids recreating SDL textures for the same text/color/style every frame
  // (previously "one-off" but was called for all text on every render tick,
  // costing ~0.5-1ms × 70+ calls = ~50ms per frame on RPi3B).
  void drawText(SDL_Renderer *renderer, const std::string &text, int x, int y,
                SDL_Color color, FontStyle style, bool centered = false,
                bool rightAlign = false, bool vertCentered = false) {
    if (text.empty())
      return;
    const int sIdx = idx(style);
    const int ptSz = scaledPt_[sIdx];
    const Uint32 nowMs = SDL_GetTicks();

    // Periodic prune: evict entries idle for >5 s
    if (nowMs - lastCachePruneMs_ > 5000) {
      for (auto it = textCache_.begin(); it != textCache_.end();) {
        if (nowMs - it->second.lastUsedMs > 5000) {
          SDL_DestroyTexture(it->second.tex);
          it = textCache_.erase(it);
        } else {
          ++it;
        }
      }
      lastCachePruneMs_ = nowMs;
    }

    const uint64_t hash = textHash(text, color, sIdx, ptSz);
    int w = 0, h = 0;
    SDL_Texture *tex = nullptr;

    auto it = textCache_.find(hash);
    if (it != textCache_.end()) {
      auto &e = it->second;
      // Verify no hash collision
      if (e.text == text && e.color.r == color.r && e.color.g == color.g &&
          e.color.b == color.b && e.color.a == color.a &&
          e.styleIdx == sIdx && e.ptSz == ptSz) {
        e.lastUsedMs = nowMs;
        tex = e.tex;
        w = e.w;
        h = e.h;
      } else {
        // Collision: evict old entry and fall through to recreate
        SDL_DestroyTexture(e.tex);
        textCache_.erase(it);
      }
    }

    if (!tex) {
      tex = renderText(renderer, text, color, style, &w, &h);
      if (!tex)
        return;
      // Evict oldest entry if at capacity
      if (textCache_.size() >= kMaxCacheEntries) {
        auto oldest = textCache_.begin();
        for (auto jt = textCache_.begin(); jt != textCache_.end(); ++jt) {
          if (jt->second.lastUsedMs < oldest->second.lastUsedMs)
            oldest = jt;
        }
        SDL_DestroyTexture(oldest->second.tex);
        textCache_.erase(oldest);
      }
      textCache_.emplace(hash, TextCacheEntry{text, color, sIdx, ptSz, tex, w, h, nowMs});
    }

    SDL_Rect dst = {x, y, w, h};
    if (centered)
      dst.x -= w / 2;
    else if (rightAlign)
      dst.x -= w;
    if (vertCentered)
      dst.y -= h / 2;
    SDL_RenderCopy(renderer, tex, nullptr, &dst);
    // Texture is now owned by textCache_; do NOT destroy it here.
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
        {FontStyle::Tiny, "Tiny", kTinyTargetH, kTinyBasePt},
        {FontStyle::Caption, "Caption", kCaptionTargetH, kCaptionBasePt},
        {FontStyle::Micro, "Micro", kMicroTargetH, kMicroBasePt},
        {FontStyle::UI, "UI", kUITargetH, kUIBasePt},
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
  static constexpr int kTinyTargetH = 12;
  static constexpr int kCaptionTargetH = 13;
  static constexpr int kMicroTargetH = 15;
  static constexpr int kUITargetH = 16;
  static constexpr int kSmallTargetH = 18;
  static constexpr int kMediumTargetH = 28;
  static constexpr int kLargeTargetH = 80;
  static constexpr int kFastTargetH = 15;

private:
  static constexpr int kStyleCount = static_cast<int>(FontStyle::Count_);

  // Base point sizes at 800x480.  Tuned so TTF_FontHeight ≈ target.
  // Adjust these constants if the embedded font changes.
  static constexpr int kTinyBasePt = 8;
  static constexpr int kCaptionBasePt = 9;
  static constexpr int kMicroBasePt = 10;
  static constexpr int kUIBasePt = 11;
  static constexpr int kSmallBasePt = 14;
  static constexpr int kMediumBasePt = 24;
  static constexpr int kLargeBasePt = 60;
  static constexpr int kFastBasePt = 12;

  void clearCache() {
    for (auto &kv : textCache_)
      SDL_DestroyTexture(kv.second.tex);
    textCache_.clear();
  }

  static int idx(FontStyle s) { return static_cast<int>(s); }
  static int clampPt(float v) {
    return std::clamp(static_cast<int>(v), 8, 200);
  }

  struct TextCacheEntry {
    std::string text;
    SDL_Color color;
    int styleIdx;
    int ptSz;
    SDL_Texture *tex;
    int w, h;
    Uint32 lastUsedMs;
  };

  static constexpr size_t kMaxCacheEntries = 256;

  static uint64_t textHash(const std::string &text, SDL_Color color,
                            int styleIdx, int ptSz) {
    uint64_t h = 14695981039346656037ULL;
    for (char c : text) {
      h ^= static_cast<uint8_t>(c);
      h *= 1099511628211ULL;
    }
    h ^= (static_cast<uint64_t>(color.r) << 56) |
         (static_cast<uint64_t>(color.g) << 48) |
         (static_cast<uint64_t>(color.b) << 40) |
         (static_cast<uint64_t>(color.a) << 32) |
         (static_cast<uint64_t>(styleIdx) << 16) |
         static_cast<uint64_t>(ptSz);
    return h;
  }

  FontManager &fontMgr_;
  int scaledPt_[kStyleCount];
  std::unordered_map<uint64_t, TextCacheEntry> textCache_;
  Uint32 lastCachePruneMs_ = 0;
};
