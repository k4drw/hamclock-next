#pragma once

#include "../core/MemoryMonitor.h"
#include "FontManager.h"
#include <SDL.h>
#include <SDL_ttf.h>

#include <algorithm>
#include <string>
#include <unordered_map>
#include <vector>

// Named font styles modeled after the HamClock logical system.
enum class FontStyle {
  Tiny,       // smallest (dates, auxiliary)
  TinyBold,
  Caption,    // slightly larger than Tiny (labels)
  CaptionBold,
  Micro,      // compact UI text (tooltips, small buttons)
  MicroBold,  // bold Micro (titles)
  Fast,       // high-perf UI list text (Live Spots, Cluster)
  FastBold,   // bold Fast
  UI,         // standard UI text
  UIBold,     // bold standard UI
  SmallRegular,
  SmallBold,
  MediumRegular,
  MediumBold,
  LargeRegular,
  LargeBold,
  Count_
};

// Provides semantic font access with automatic high-DPI scaling and internal
// caching of small rendered text segments (labels, buttons).
//
// In HamClock, fonts are traditionally tuned for 800x480. We maintain those
// "logical" sizes and scale them based on the actual renderer viewport height.
class FontCatalog {
public:
  explicit FontCatalog(FontManager &fontMgr) : fontMgr_(fontMgr) {
    for (int i = 0; i < kStyleCount; ++i)
      scaledPt_[i] = 0;
  }

  // (Re)calculate all style point sizes based on logical logicalH vs actual render height.
  void recalculate(int logicalW, int logicalH) {
    (void)logicalW;
    (void)logicalH;

    scaledPt_[idx(FontStyle::Tiny)] = kTinyBasePt;
    scaledPt_[idx(FontStyle::TinyBold)] = kTinyBasePt;
    scaledPt_[idx(FontStyle::Caption)] = kCaptionBasePt;
    scaledPt_[idx(FontStyle::CaptionBold)] = kCaptionBasePt;
    scaledPt_[idx(FontStyle::Micro)] = kMicroBasePt;
    scaledPt_[idx(FontStyle::MicroBold)] = kMicroBasePt;
    scaledPt_[idx(FontStyle::Fast)] = kFastBasePt;
    scaledPt_[idx(FontStyle::FastBold)] = kFastBasePt;
    scaledPt_[idx(FontStyle::UI)] = kUIBasePt;
    scaledPt_[idx(FontStyle::UIBold)] = kUIBasePt;
    scaledPt_[idx(FontStyle::SmallRegular)] = kSmallBasePt;
    scaledPt_[idx(FontStyle::SmallBold)] = kSmallBasePt;
    scaledPt_[idx(FontStyle::MediumRegular)] = kMediumBasePt;
    scaledPt_[idx(FontStyle::MediumBold)] = kMediumBasePt;
    scaledPt_[idx(FontStyle::LargeRegular)] = kLargeBasePt;
    scaledPt_[idx(FontStyle::LargeBold)] = kLargeBasePt;

    // After recalculating point sizes, clear text caches as metrics changed.
    clearCache();
  }

  // Returns the actual TTF point size for a logical style.
  int ptSize(FontStyle style) const { return scaledPt_[idx(style)]; }

  // Draw text using logical styles. Internally uses an LRU cache for high-perf
  // rendering of repeated UI strings (labels, button text).
  void drawText(SDL_Renderer *renderer, const std::string &text, int x, int y,
                SDL_Color color, FontStyle style, bool center = false,
                bool rightAlign = false, bool vertCentered = false) {
    if (text.empty() || !renderer)
      return;

    uint64_t key = hash(text, style);
    auto it = textCache_.find(key);
    if (it != textCache_.end()) {
      renderCached(renderer, it->second, x, y, color, center, rightAlign,
                    vertCentered);
      return;
    }

    // Cache miss: render new texture.
    int pt = scaledPt_[idx(style)];
    bool bold = isBold(style);

    int tw, th;
    SDL_Texture *tex = fontMgr_.renderText(renderer, text, {255, 255, 255, 255},
                                           pt, &tw, &th, bold);
    if (!tex)
      return;

    TextCacheEntry entry = {text, tex, tw, th, style};
    textCache_[key] = entry;

    renderCached(renderer, entry, x, y, color, center, rightAlign, vertCentered);

    // Housekeeping: periodic cache prune
    Uint32 now = SDL_GetTicks();
    if (now - lastCachePruneMs_ > 60000) {
      pruneCache();
      lastCachePruneMs_ = now;
    }
  }

  // Render text to a standalone texture (caller must destroy via
  // destroyTexture). Useful for one-off dynamic strings that shouldn't pollute
  // the LRU cache (e.g. clock HH:MM:SS).
  SDL_Texture *renderText(SDL_Renderer *renderer, const std::string &text,
                          SDL_Color color, FontStyle style, int *w = nullptr,
                          int *h = nullptr) {
    int pt = scaledPt_[idx(style)];
    bool bold = isBold(style);
    return fontMgr_.renderText(renderer, text, color, pt, w, h, bold);
  }

  void clearCache() {
    for (auto &kv : textCache_)
      destroyTexture(kv.second.tex);
    textCache_.clear();
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

  // --- Static Helper for UI components ---
  static bool isBold(FontStyle style) {
    return (style == FontStyle::TinyBold || style == FontStyle::CaptionBold ||
            style == FontStyle::MicroBold || style == FontStyle::FastBold ||
            style == FontStyle::UIBold ||
            style == FontStyle::SmallBold || style == FontStyle::MediumBold ||
            style == FontStyle::LargeBold);
  }

  // --- Semantic Metrics API ---

  struct CalibEntry {
    const char *name;
    int targetHeight;
    int basePt;
    int scaledPt;
    int measuredHeight;
  };
  std::vector<CalibEntry> calibrate() {
    std::vector<CalibEntry> out;
    out.push_back({"Tiny", kTinyTargetH, kTinyBasePt, scaledPt_[idx(FontStyle::Tiny)], 0});
    out.push_back({"Caption", kCaptionTargetH, kCaptionBasePt, scaledPt_[idx(FontStyle::Caption)], 0});
    out.push_back({"Micro", kMicroTargetH, kMicroBasePt, scaledPt_[idx(FontStyle::Micro)], 0});
    out.push_back({"UI", kUITargetH, kUIBasePt, scaledPt_[idx(FontStyle::UI)], 0});
    out.push_back({"Small", kSmallTargetH, kSmallBasePt, scaledPt_[idx(FontStyle::SmallRegular)], 0});
    out.push_back({"Medium", kMediumTargetH, kMediumBasePt, scaledPt_[idx(FontStyle::MediumRegular)], 0});
    out.push_back({"Large", kLargeTargetH, kLargeBasePt, scaledPt_[idx(FontStyle::LargeRegular)], 0});
    out.push_back({"Fast", kFastTargetH, kFastBasePt, scaledPt_[idx(FontStyle::Fast)], 0});

    for (auto &e : out) {
      TTF_Font *f = fontMgr_.getFont(e.scaledPt);
      if (f) e.measuredHeight = TTF_FontHeight(f);
    }
    return out;
  }

  static constexpr int kTinyTargetH = 10;
  static constexpr int kCaptionTargetH = 11;
  static constexpr int kMicroTargetH = 13;
  static constexpr int kUITargetH = 15;
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

  static int idx(FontStyle s) { return static_cast<int>(s); }
  static int clampPt(float v) {
    return std::clamp(static_cast<int>(v), 8, 200);
  }

  struct TextCacheEntry {
    std::string text;
    SDL_Texture *tex;
    int w, h;
    FontStyle style;
  };

  void renderCached(SDL_Renderer *renderer, const TextCacheEntry &e, int x,
                    int y, SDL_Color color, bool center, bool rightAlign,
                    bool vertCentered) {
    SDL_SetTextureColorMod(e.tex, color.r, color.g, color.b);
    SDL_SetTextureAlphaMod(e.tex, color.a);
    int tx = x;
    int ty = y;
    if (center)
      tx -= e.w / 2;
    else if (rightAlign)
      tx -= e.w;
    if (vertCentered)
      ty -= e.h / 2;

    SDL_Rect dst = {tx, ty, e.w, e.h};
    SDL_RenderCopy(renderer, e.tex, nullptr, &dst);
  }

  void pruneCache() {
    if (textCache_.size() < 500)
      return;
    // Simple nuclear prune for now — text catalog items are cheap to re-render.
    clearCache();
  }

  uint64_t hash(const std::string &text, FontStyle style) {
    uint64_t h = std::hash<std::string>{}(text);
    int ptSz = scaledPt_[idx(style)];
    uint32_t styleIdx = idx(style);
    // Combine hash with size + style index to avoid collisions on DPI change.
    h ^= (static_cast<uint64_t>(styleIdx) << 16) |
         static_cast<uint64_t>(ptSz);
    return h;
  }

  FontManager &fontMgr_;
  int scaledPt_[kStyleCount];
  std::unordered_map<uint64_t, TextCacheEntry> textCache_;
  Uint32 lastCachePruneMs_ = 0;
};
