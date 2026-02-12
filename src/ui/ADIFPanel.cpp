#include "ADIFPanel.h"
#include <algorithm>
#include <cstdio>
#include <vector>

ADIFPanel::ADIFPanel(int x, int y, int w, int h, FontManager &fontMgr,
                     std::shared_ptr<ADIFStore> store)
    : Widget(x, y, w, h), fontMgr_(fontMgr), store_(std::move(store)) {}

void ADIFPanel::update() { stats_ = store_->get(); }

void ADIFPanel::render(SDL_Renderer *renderer) {
  if (!fontMgr_.ready())
    return;

  SDL_SetRenderDrawColor(renderer, 30, 20, 20, 255);
  SDL_Rect rect = {x_, y_, width_, height_};
  SDL_RenderFillRect(renderer, &rect);
  SDL_SetRenderDrawColor(renderer, 80, 80, 80, 255);
  SDL_RenderDrawRect(renderer, &rect);

  int pad = 8;
  int curY = y_ + pad;

  fontMgr_.drawText(renderer, "ADIF Log Stats", x_ + pad, curY,
                    {255, 100, 100, 255}, 10, true);
  curY += 16;

  if (!stats_.valid) {
    fontMgr_.drawText(renderer, "No Log Found", x_ + width_ / 2,
                      y_ + height_ / 2, {150, 150, 150, 255}, 12, false, true);
    return;
  }

  char buf[64];
  std::snprintf(buf, sizeof(buf), "Total QSOs: %d", stats_.totalQSOs);
  fontMgr_.drawText(renderer, buf, x_ + pad, curY, {255, 255, 255, 255}, 11);
  curY += 18;

  // Top Bands
  std::vector<std::pair<std::string, int>> topBands(stats_.bandCounts.begin(),
                                                    stats_.bandCounts.end());
  std::sort(topBands.begin(), topBands.end(),
            [](auto &a, auto &b) { return a.second > b.second; });

  fontMgr_.drawText(renderer, "Top Bands:", x_ + pad, curY,
                    {150, 150, 150, 255}, 9);
  curY += 12;
  for (size_t i = 0; i < std::min((size_t)3, topBands.size()); ++i) {
    std::snprintf(buf, sizeof(buf), "%s: %d", topBands[i].first.c_str(),
                  topBands[i].second);
    fontMgr_.drawText(renderer, buf, x_ + pad + 5, curY, {200, 200, 200, 255},
                      10);
    curY += 12;
  }
  curY += 5;

  // Latest Calls
  fontMgr_.drawText(renderer, "Latest:", x_ + pad, curY, {150, 150, 150, 255},
                    9);
  curY += 12;
  for (const auto &call : stats_.latestCalls) {
    fontMgr_.drawText(renderer, call, x_ + pad + 5, curY, {0, 255, 255, 255},
                      10);
    curY += 12;
  }
}
