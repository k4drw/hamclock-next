#include "HeardMePanel.h"
#include "../core/PrefixManager.h"
#include "../core/TimeUtils.h"
#include "FontManager.h"
#include "RenderUtils.h"
#include "../core/Theme.h"
#include "WidgetRegistry.h"
#include "../core/HamClockState.h"
#include "TextureManager.h"
#include <algorithm>
#include <cmath>
#include <chrono>

HeardMePanel::HeardMePanel(int x, int y, int w, int h, FontManager &fontMgr,
                           TextureManager &texMgr,
                           std::shared_ptr<HeardMeStore> store,
                           AppConfig *config,
                           std::shared_ptr<HamClockState> state)
    : Widget(x, y, w, h), fontMgr_(fontMgr), texMgr_(texMgr), store_(store),
      config_(config), state_(state) {}

HeardMePanel::~HeardMePanel() { clearCache(); }

void HeardMePanel::update() {
  auto spots = store_->getSpots();
  // Simple check to see if we need to update our local list
  if (spots.size() != lastSpots_.size() || (spots.size() > 0 && spots[0].spottedAt != lastSpots_[0].spottedAt)) {
    lastSpots_ = spots;
    clearCache();
  }
}

void HeardMePanel::onResize(int x, int y, int w, int h) {
  Widget::onResize(x, y, w, h);
  clearCache();
}

void HeardMePanel::render(SDL_Renderer *renderer) {
  ThemeColors themes = getThemeColors(theme_);
  renderChrome(renderer);
  renderTitle(renderer, fontMgr_, "Heard Me");

  auto *cat = fontMgr_.catalog();
  int pad = 5;
  int curY = y_ + 25; // below title
  int rowH = 14;

  // Header
  cat->drawText(renderer, "Skimmer", x_ + pad, curY, themes.textDim, FontStyle::Micro);
  cat->drawText(renderer, "SNR", x_ + width_ - 90, curY, themes.textDim, FontStyle::Micro);
  cat->drawText(renderer, "WPM", x_ + width_ - 60, curY, themes.textDim, FontStyle::Micro);
  cat->drawText(renderer, "Age", x_ + width_ - pad - 25, curY, themes.textDim, FontStyle::Micro);
  curY += 12;

  if (cache_.size() != lastSpots_.size()) {
    clearCache();
    cache_.resize(lastSpots_.size());
  }

  for (size_t i = 0; i < lastSpots_.size() && curY + rowH < y_ + height_; ++i) {
    const auto &spot = lastSpots_[i];
    auto &cache = cache_[i];
    SDL_Color color = themes.text;

    // Render / Cache Skimmer Call
    if (!cache.callTex) {
      cache.callTex = fontMgr_.renderText(renderer, spot.rxCall, color, 10, &cache.callW, &cache.callH);
    }
    if (cache.callTex) {
      SDL_Rect dst = {x_ + pad, curY + (rowH - cache.callH) / 2, cache.callW, cache.callH};
      SDL_RenderCopy(renderer, cache.callTex, nullptr, &dst);
    }

    // SNR
    std::string snrStr = std::to_string((int)spot.snr) + "dB";
    if (!cache.snrTex) {
      cache.snrTex = fontMgr_.renderText(renderer, snrStr, themes.accent, 10, &cache.snrW, &cache.snrH);
    }
    if (cache.snrTex) {
      SDL_Rect dst = {x_ + width_ - 95, curY + (rowH - cache.snrH) / 2, cache.snrW, cache.snrH};
      SDL_RenderCopy(renderer, cache.snrTex, nullptr, &dst);
    }

    // WPM
    std::string wpmStr = std::to_string(spot.wpm);
    if (!cache.wpmTex) {
      cache.wpmTex = fontMgr_.renderText(renderer, wpmStr, themes.text, 10, &cache.wpmW, &cache.wpmH);
    }
    if (cache.wpmTex) {
      SDL_Rect dst = {x_ + width_ - 60, curY + (rowH - cache.wpmH) / 2, cache.wpmW, cache.wpmH};
      SDL_RenderCopy(renderer, cache.wpmTex, nullptr, &dst);
    }

    // Age
    std::string age = formatAge(spot.spottedAt);
    if (!cache.ageTex || cache.lastAge != age) {
      if (cache.ageTex) MemoryMonitor::getInstance().destroyTexture(cache.ageTex);
      cache.ageTex = fontMgr_.renderText(renderer, age, themes.textDim, 10, &cache.ageW, &cache.ageH);
      cache.lastAge = age;
    }
    if (cache.ageTex) {
      SDL_Rect dst = {x_ + width_ - pad - cache.ageW, curY + (rowH - cache.ageH) / 2, cache.ageW, cache.ageH};
      SDL_RenderCopy(renderer, cache.ageTex, nullptr, &dst);
    }

    curY += rowH;
  }
}

void HeardMePanel::renderMapOverlay(SDL_Renderer *renderer, const MapContext &ctx) {
  auto spots = store_->getSpots();
  if (spots.empty())
    return;

  double deLat = state_ ? state_->deLocation.lat : config_->lat;
  double deLon = state_ ? state_->deLocation.lon : config_->lon;

  for (const auto &spot : spots) {
    if (spot.rxLat == 0.0 && spot.rxLon == 0.0)
      continue;

    int x0 = ctx.lonToScreenX(spot.rxLon);
    int y0 = ctx.latToScreenY(spot.rxLat);
    int x1 = ctx.lonToScreenX(deLon);
    int y1 = ctx.latToScreenY(deLat);

    SDL_Color col = {255, 255, 0, 180}; // Yellow
    SDL_Texture *lineTex = texMgr_.get("line_aa");

    SDL_FPoint pts[2] = {
        {static_cast<float>(x0), static_cast<float>(y0)},
        {static_cast<float>(x1), static_cast<float>(y1)}
    };
    RenderUtils::drawPolylineTextured(renderer, lineTex, pts, 2, 1.5f, col);
  }
}

void HeardMePanel::clearCache() {
  for (auto &c : cache_) {
    if (c.callTex) MemoryMonitor::getInstance().destroyTexture(c.callTex);
    if (c.snrTex) MemoryMonitor::getInstance().destroyTexture(c.snrTex);
    if (c.wpmTex) MemoryMonitor::getInstance().destroyTexture(c.wpmTex);
    if (c.ageTex) MemoryMonitor::getInstance().destroyTexture(c.ageTex);
  }
  cache_.clear();
}

std::string HeardMePanel::formatAge(const std::chrono::system_clock::time_point &spottedAt) const {
  auto now = std::chrono::system_clock::now();
  auto secs = std::chrono::duration_cast<std::chrono::seconds>(now - spottedAt).count();
  if (secs < 60) return std::to_string(secs) + "s";
  return std::to_string(secs / 60) + "m";
}

REGISTER_WIDGET("heard_me", "Heard Me", true, false, {
  return std::make_unique<HeardMePanel>(0, 0, 0, 0, deps.fontMgr, deps.texMgr,
                                        deps.heardMeStore, &deps.appCfg,
                                        deps.state);
})
