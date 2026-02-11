#include "RSSBanner.h"
#include "FontCatalog.h"

#include <algorithm>
#include <cstdio>

RSSBanner::RSSBanner(int x, int y, int w, int h, FontManager &fontMgr,
                     std::shared_ptr<RSSDataStore> store)
    : Widget(x, y, w, h), fontMgr_(fontMgr), store_(std::move(store)),
      lastTick_(SDL_GetTicks()) {
  auto *cat = fontMgr_.catalog();
  if (cat)
    fontSize_ = cat->ptSize(FontStyle::SmallRegular);
}

void RSSBanner::update() {
  Uint32 now = SDL_GetTicks();
  Uint32 dt = now - lastTick_;
  lastTick_ = now;

  scrollOffset_ += kScrollSpeed * (static_cast<float>(dt) / 1000.0f);

  // Wrap when the first copy has scrolled fully off-screen
  if (totalWidth_ > 0 && scrollOffset_ >= static_cast<float>(totalWidth_)) {
    scrollOffset_ -= static_cast<float>(totalWidth_);
  }
}

void RSSBanner::render(SDL_Renderer *renderer) {
  // Check if headlines changed
  auto data = store_->get();
  if (data.valid && data.headlines != lastHeadlines_) {
    lastHeadlines_ = data.headlines;
    rebuildTextures(renderer);
  }

  // Semi-transparent black background
  SDL_Rect bgRect = {x_, y_, width_, height_};
  SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
  SDL_SetRenderDrawColor(renderer, 0, 0, 0, 180);
  SDL_RenderFillRect(renderer, &bgRect);

  if (entries_.empty() || totalWidth_ <= 0)
    return;

  // Clip to banner bounds
  SDL_Rect clipRect = {x_, y_, width_, height_};
  SDL_RenderSetClipRect(renderer, &clipRect);

  int textY = y_ + (height_ - maxHeight_) / 2;
  int offset = static_cast<int>(scrollOffset_);

  // Draw two copies of the full entry chain for seamless wrapping.
  // The clip rect culls anything outside the banner bounds.
  // Draw enough copies to fill the width (usually 2, maybe 3 if very wide)
  // Seamless wrapping logic:
  // cx starts at x_ - scrollOffset_
  // We must draw copies until we cover the range [x_, x_ + width_]

  // 1. Calculate safe start copy index to ensure we don't draw way to the left
  // We want (copy * totalWidth_) > scrollOffset_ - width_ (roughly)
  // Actually, just looping 0..2 is usually enough unless width > 2*totalWidth
  int numCopies = 2 + (width_ / (totalWidth_ > 0 ? totalWidth_ : 1));

  for (int copy = 0; copy < numCopies; ++copy) {
    int copyStartX = x_ - offset + copy * totalWidth_;

    // Optimization: Skip valid check if fully outside
    if (copyStartX + totalWidth_ < x_ || copyStartX > x_ + width_)
      continue;

    int cx = copyStartX;
    for (const auto &e : entries_) {
      // Draw if this segment overlaps the visible banner area [x_, x_+width_]
      if (e.tex && cx + e.w > x_ && cx < x_ + width_) {
        SDL_Rect dst = {cx, textY, e.w, e.h};
        SDL_RenderCopy(renderer, e.tex, nullptr, &dst);
      }
      cx += e.w;
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
  for (auto &e : entries_) {
    if (e.tex)
      SDL_DestroyTexture(e.tex);
  }
  entries_.clear();
  totalWidth_ = 0;
  maxHeight_ = 0;
  lastHeadlines_.clear();
}

void RSSBanner::rebuildTextures(SDL_Renderer *renderer) {
  for (auto &e : entries_) {
    if (e.tex)
      SDL_DestroyTexture(e.tex);
  }
  entries_.clear();
  totalWidth_ = 0;
  maxHeight_ = 0;

  if (lastHeadlines_.empty())
    return;

  SDL_Color white = {255, 255, 255, 255};

  // Create a separator texture once
  // Create a separator texture once
  int sepW = 0, sepH = 0;
  SDL_Texture *sepTex =
      fontMgr_.renderText(renderer, kSeparator, white, fontSize_, &sepW, &sepH);

  for (size_t i = 0; i < lastHeadlines_.size(); ++i) {
    // Headline texture
    Entry e;
    e.tex = fontMgr_.renderText(renderer, lastHeadlines_[i], white, fontSize_,
                                &e.w, &e.h);
    if (e.tex) {
      totalWidth_ += e.w;
      maxHeight_ = std::max(maxHeight_, e.h);
      entries_.push_back(e);
    }

    // Separator texture (after every headline including last, for wrap)
    if (sepTex) {
      // Re-render separate texture for each use
      Entry sepEntry;
      sepEntry.tex = fontMgr_.renderText(renderer, kSeparator, white, fontSize_,
                                         &sepEntry.w, &sepEntry.h);
      if (sepEntry.tex) {
        totalWidth_ += sepEntry.w;
        maxHeight_ = std::max(maxHeight_, sepEntry.h);
        entries_.push_back(sepEntry);
      }
    }
  }

  // Clean up the first separator texture (we created individual copies above)
  if (sepTex)
    SDL_DestroyTexture(sepTex);

  std::fprintf(stderr, "RSSBanner: %zu headlines, %zu entries, totalWidth=%d\n",
               lastHeadlines_.size(), entries_.size(), totalWidth_);
}
