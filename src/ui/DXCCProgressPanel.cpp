#include "DXCCProgressPanel.h"
#include "../core/Theme.h"
#include "FontCatalog.h"
#include <SDL.h>
#include <algorithm>
#include <cstdio>
#include <map>
#include <set>

DXCCProgressPanel::DXCCProgressPanel(int x, int y, int w, int h,
                                     FontManager &fontMgr,
                                     std::shared_ptr<ADIFStore> store,
                                     PrefixManager &prefixMgr)
    : Widget(x, y, w, h), fontMgr_(fontMgr), store_(std::move(store)),
      prefixMgr_(prefixMgr) {}

void DXCCProgressPanel::rebuild(const ADIFStats &stats) {
  uniqueEntities_ = 0;
  entitiesByBand_.clear();
  recentEntities_.clear();

  std::set<int> seenGlobal;
  std::map<std::string, std::set<int>> seenByBand;
  std::set<std::string> seenEntityNames;

  for (const auto &qso : stats.recentQSOs) {
    if (qso.callsign.empty())
      continue;
    int dxcc = prefixMgr_.findDXCC(qso.callsign);
    if (dxcc < 0)
      continue;
    std::string entityName = prefixMgr_.getCountryName(dxcc);
    if (entityName.empty())
      continue;

    seenGlobal.insert(dxcc);
    if (!qso.band.empty())
      seenByBand[qso.band].insert(dxcc);

    if (recentEntities_.size() < 10 &&
        seenEntityNames.find(entityName) == seenEntityNames.end()) {
      seenEntityNames.insert(entityName);
      recentEntities_.push_back(entityName);
    }
  }

  uniqueEntities_ = static_cast<int>(seenGlobal.size());
  for (const auto &kv : seenByBand)
    entitiesByBand_[kv.first] = static_cast<int>(kv.second.size());
}

void DXCCProgressPanel::update() {
  ADIFStats stats = store_->get();
  if (!stats.valid)
    return;
  if (stats.totalQSOs != lastTotalQSOs_) {
    lastTotalQSOs_ = stats.totalQSOs;
    rebuild(stats);
  }
}

void DXCCProgressPanel::render(SDL_Renderer *renderer) {
  if (!fontMgr_.ready())
    return;

  ThemeColors themes = getThemeColors(theme_);
  auto *cat = fontMgr_.catalog();
  if (!cat)
    return;

  renderChrome(renderer);

  const int pad = 6;
  cat->drawText(renderer, "DXCC (Recent)", x_ + pad, y_ + 5, themes.accent,
                FontStyle::MicroBold);

  int curY = y_ + 22;

  if (lastTotalQSOs_ < 0) {
    cat->drawText(renderer, "No ADIF data", x_ + width_ / 2, y_ + height_ / 2,
                  themes.textDim, FontStyle::Fast, true, false, true);
    return;
  }

  // Entity count
  char countBuf[32];
  std::snprintf(countBuf, sizeof(countBuf), "%d entities worked",
                uniqueEntities_);
  cat->drawText(renderer, countBuf, x_ + pad, curY, themes.text,
                FontStyle::FastBold);
  curY += rowFontSize_ + 2;

  // Progress bar toward 100 (basic DXCC award threshold)
  int barX = x_ + pad;
  int barW = width_ - 2 * pad;
  int barH = 8;
  SDL_SetRenderDrawColor(renderer, themes.rowStripe1.r, themes.rowStripe1.g,
                         themes.rowStripe1.b, 255);
  SDL_Rect bgBar = {barX, curY, barW, barH};
  SDL_RenderFillRect(renderer, &bgBar);

  if (uniqueEntities_ > 0) {
    int fillW = barW * std::min(uniqueEntities_, 100) / 100;
    SDL_Color fillCol =
        uniqueEntities_ >= 100 ? themes.success : themes.accent;
    SDL_SetRenderDrawColor(renderer, fillCol.r, fillCol.g, fillCol.b, 255);
    SDL_Rect fillBar = {barX, curY, fillW, barH};
    SDL_RenderFillRect(renderer, &fillBar);
  }
  SDL_SetRenderDrawColor(renderer, themes.border.r, themes.border.g,
                         themes.border.b, 255);
  SDL_RenderDrawRect(renderer, &bgBar);

  char progBuf[16];
  std::snprintf(progBuf, sizeof(progBuf), "%d/100",
                std::min(uniqueEntities_, 100));
  cat->drawText(renderer, progBuf, x_ + width_ - pad, curY, themes.textDim,
                FontStyle::Tiny, false, true);
  curY += barH + 6;

  // Band breakdown
  if (!entitiesByBand_.empty()) {
    cat->drawText(renderer, "Entities by band:", x_ + pad, curY,
                  themes.textDim, FontStyle::Tiny);
    curY += 12;

    // Sort bands by entity count descending
    std::vector<std::pair<std::string, int>> bands(entitiesByBand_.begin(),
                                                   entitiesByBand_.end());
    std::sort(bands.begin(), bands.end(),
              [](const auto &a, const auto &b) { return a.second > b.second; });

    int bx = x_ + pad;
    for (const auto &kv : bands) {
      if (curY + 12 > y_ + height_ - pad)
        break;
      char bbuf[24];
      std::snprintf(bbuf, sizeof(bbuf), "%s:%d", kv.first.c_str(), kv.second);
      cat->drawText(renderer, bbuf, bx, curY, themes.text, FontStyle::Tiny);
      bx += 44;
      if (bx + 44 > x_ + width_ - pad) {
        bx = x_ + pad;
        curY += 12;
      }
    }
    if (bx > x_ + pad)
      curY += 12;
  }

  // Recent unique entities list
  if (!recentEntities_.empty() && curY < y_ + height_ - pad) {
    cat->drawText(renderer, "Recent:", x_ + pad, curY, themes.textDim,
                  FontStyle::Tiny);
    curY += 12;

    int rowH = rowFontSize_ + 2;
    int maxRows = (y_ + height_ - pad - curY) / rowH;
    maxScroll_ = std::max(0, static_cast<int>(recentEntities_.size()) - maxRows);
    scrollOffset_ = std::min(scrollOffset_, maxScroll_);

    int startIdx = scrollOffset_;
    int endIdx = std::min(static_cast<int>(recentEntities_.size()),
                          startIdx + maxRows);
    for (int i = startIdx; i < endIdx; ++i) {
      std::string label = recentEntities_[i];
      if (label.size() > 24)
        label = label.substr(0, 23) + "~";
      cat->drawText(renderer, label, x_ + pad, curY, themes.text,
                    FontStyle::Fast);
      curY += rowH;
    }
  }
}

void DXCCProgressPanel::onResize(int x, int y, int w, int h) {
  Widget::onResize(x, y, w, h);
  auto *cat = fontMgr_.catalog();
  rowFontSize_ = cat ? cat->ptSize(FontStyle::Fast) : 11;
  scrollOffset_ = 0;
}

bool DXCCProgressPanel::onMouseWheel(int scrollY) {
  scrollOffset_ = std::clamp(scrollOffset_ - scrollY, 0, maxScroll_);
  return true;
}
