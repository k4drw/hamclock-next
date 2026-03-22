#include "SolarTimelinePanel.h"
#include "../core/Astronomy.h"
#include "../core/Logger.h"
#include "../core/StringUtils.h"
#include "../core/Theme.h"
#include "FontCatalog.h"
#include <SDL.h>
#include <cstdio>
#include <ctime>
#include <nlohmann/json.hpp>

SolarTimelinePanel::SolarTimelinePanel(int x, int y, int w, int h,
                                       FontManager &fontMgr, NetworkManager &net)
    : Widget(x, y, w, h), fontMgr_(fontMgr), net_(net) {}

SDL_Color SolarTimelinePanel::kpColor(float kp) const {
  if (kp >= 8.0f) return {255, 50,  50,  255}; // red   (severe)
  if (kp >= 7.0f) return {255, 140, 0,   255}; // orange (strong)
  if (kp >= 5.0f) return {255, 220, 0,   255}; // yellow (minor)
  return              {70,  200, 70,  255};     // green  (quiet)
}

void SolarTimelinePanel::update() {
  static constexpr uint32_t kFetchIntervalMs = 3 * 3600 * 1000; // 3h
  uint32_t now = SDL_GetTicks();
  if (async_->fetching.load() ||
      (lastFetch_ != 0 && now - lastFetch_ < kFetchIntervalMs))
    return;

  async_->fetching.store(true);
  lastFetch_ = now;

  net_.fetchAsync(
      "https://services.swpc.noaa.gov/products/noaa-planetary-k-index-forecast.json",
      [async = async_](std::string body) {
        SolarTimelineData d;
        try {
          auto j = nlohmann::json::parse(body);
          // Format: [[header...], [time_tag, kp, status, source], ...]
          for (size_t i = 1; i < j.size(); ++i) {
            auto &row = j[i];
            if (!row.is_array() || row.size() < 2)
              continue;
            std::string timeStr = row[0].get<std::string>();
            std::string kpStr   = row[1].get<std::string>();

            struct tm t{};
            if (std::sscanf(timeStr.c_str(), "%d-%d-%d %d:%d:%d",
                            &t.tm_year, &t.tm_mon, &t.tm_mday,
                            &t.tm_hour, &t.tm_min, &t.tm_sec) != 6)
              continue;
            t.tm_year -= 1900;
            t.tm_mon  -= 1;

            KpForecastBlock blk;
            blk.timeUtc = Astronomy::portable_timegm(&t);
            blk.kp      = StringUtils::safe_stof(kpStr);
            d.blocks.push_back(blk);
          }
          d.fetchedAt = std::time(nullptr);
          d.valid     = !d.blocks.empty();
          LOG_I("SolarTimeline", "Loaded {} Kp blocks", d.blocks.size());
        } catch (...) {
          LOG_E("SolarTimeline", "Parse failed");
        }
        {
          std::lock_guard<std::mutex> lk(async->mutex);
          async->data = std::move(d);
        }
        async->fetching.store(false);
      },
      10800 // cache 3h
  );
}

void SolarTimelinePanel::render(SDL_Renderer *renderer) {
  if (!fontMgr_.ready())
    return;

  renderChrome(renderer);
  ThemeColors themes = getThemeColors(theme_);
  auto *cat = fontMgr_.catalog();

  cat->drawText(renderer, "Solar Impact", x_ + 10, y_ + 5, themes.accent,
                FontStyle::MicroBold);

  SolarTimelineData d;
  {
    std::lock_guard<std::mutex> lk(async_->mutex);
    d = async_->data;
  }

  if (!d.valid || d.blocks.empty()) {
    cat->drawText(renderer, "Loading...", x_ + width_ / 2,
                  y_ + height_ / 2, themes.textDim, FontStyle::Fast, true);
    return;
  }

  // Timeline area
  const int pad    = 6;
  const int titleH = 20;
  int tx = x_ + pad;
  int ty = y_ + titleH + pad;
  int tw = width_  - 2 * pad;
  int th = height_ - titleH - 2 * pad - 14; // 14px for label row

  // Time window: show blocks within ±72h of now
  time_t nowT    = std::time(nullptr);
  time_t winSec  = 72 * 3600;
  time_t tStart  = nowT - winSec;
  time_t tEnd    = nowT + winSec;
  double winSpan = (double)(tEnd - tStart);

  // Map time → x pixel
  auto timeToX = [&](time_t t) -> int {
    return tx + (int)((double)(t - tStart) / winSpan * tw);
  };

  // Block width for one 3-hour slot
  int blockW = (int)((double)(3 * 3600) / winSpan * tw);
  if (blockW < 1) blockW = 1;

  // Draw Kp blocks
  for (const auto &blk : d.blocks) {
    if (blk.timeUtc + 3 * 3600 < tStart || blk.timeUtc > tEnd)
      continue;
    int bx = timeToX(blk.timeUtc);
    int bx2 = timeToX(blk.timeUtc + 3 * 3600);
    if (bx  < tx)            bx  = tx;
    if (bx2 > tx + tw)       bx2 = tx + tw;
    int bw = bx2 - bx;
    if (bw <= 0) continue;

    SDL_Color col = kpColor(blk.kp);
    // Height proportional to kp (0-9 → 0-th)
    float frac = blk.kp / 9.0f;
    if (frac > 1.0f) frac = 1.0f;
    int bh = (int)(frac * th);
    if (bh < 2) bh = 2;

    SDL_Rect r = {bx, ty + th - bh, bw - 1, bh};
    SDL_SetRenderDrawColor(renderer, col.r, col.g, col.b, 220);
    SDL_RenderFillRect(renderer, &r);
  }

  // Axis: 24h tick marks
  SDL_SetRenderDrawColor(renderer, themes.border.r, themes.border.g,
                         themes.border.b, 150);
  for (int h = -72; h <= 72; h += 24) {
    time_t tick = nowT + (time_t)(h * 3600);
    int px = timeToX(tick);
    if (px >= tx && px <= tx + tw)
      SDL_RenderDrawLine(renderer, px, ty, px, ty + th);
  }

  // "Now" marker
  int nowX = timeToX(nowT);
  SDL_SetRenderDrawColor(renderer, 255, 255, 255, 200);
  SDL_RenderDrawLine(renderer, nowX, ty, nowX, ty + th);

  // Labels: -48h, -24h, NOW, +24h, +48h
  const struct { int h; const char *label; } kLabels[] = {
    {-48, "-48h"}, {-24, "-24h"}, {0, "NOW"}, {24, "+24h"}, {48, "+48h"}
  };
  int labelY = ty + th + 2;
  for (const auto &lbl : kLabels) {
    time_t tick = nowT + (time_t)(lbl.h * 3600);
    int px = timeToX(tick);
    if (px < tx || px > tx + tw) continue;
    SDL_Color col = (lbl.h == 0) ? themes.text : themes.textDim;
    cat->drawText(renderer, lbl.label, px, labelY, col, FontStyle::Micro, true);
  }
}
