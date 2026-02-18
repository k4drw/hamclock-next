#include "ActivityPanels.h"
#include "../core/Theme.h"
#include <algorithm>
#include <iomanip>
#include <sstream>

// --- DXPedPanel ---

DXPedPanel::DXPedPanel(int x, int y, int w, int h, FontManager &fontMgr,
                       ActivityProvider &provider,
                       std::shared_ptr<ActivityDataStore> store)
    : ListPanel(x, y, w, h, fontMgr, "DX Peditions", {}), provider_(provider),
      store_(store) {}

void DXPedPanel::update() {
  uint32_t nowTicks = SDL_GetTicks();
  if (nowTicks - lastFetch_ > 20 * 60 * 1000 || lastFetch_ == 0) {
    lastFetch_ = nowTicks;
    provider_.fetch();
  }

  auto data = store_->get();
  if (data.lastUpdated != lastUpdate_) {
    std::vector<std::string> rows;
    for (const auto &de : data.dxpeds) {
      std::stringstream ss;
      ss << std::left << std::setw(12) << de.call << de.location;
      rows.push_back(ss.str());
      if (rows.size() >= 10)
        break;
    }
    if (rows.empty() && data.valid) {
      rows.push_back("No upcoming expeditions");
    }
    setRows(rows);
    lastUpdate_ = data.lastUpdated;
  }
}

// --- ONTAPanel ---

ONTAPanel::ONTAPanel(int x, int y, int w, int h, FontManager &fontMgr,
                     ActivityProvider &provider,
                     std::shared_ptr<ActivityDataStore> store)
    : ListPanel(x, y, w, h, fontMgr, "On The Air", {}), provider_(provider),
      store_(store) {}

const char *ONTAPanel::filterLabel(Filter f) {
  switch (f) {
  case Filter::POTA: return "POTA";
  case Filter::SOTA: return "SOTA";
  default:           return "ALL";
  }
}

void ONTAPanel::setFilter(const std::string &f) {
  if (f == "pota")      filter_ = Filter::POTA;
  else if (f == "sota") filter_ = Filter::SOTA;
  else                  filter_ = Filter::ALL;
  // Force row rebuild on next update
  lastUpdate_ = {};
}

void ONTAPanel::rebuildRows(const ActivityData &data) {
  std::vector<std::string> rows;
  for (const auto &os : data.ontaSpots) {
    if (filter_ == Filter::POTA && os.program != "POTA")
      continue;
    if (filter_ == Filter::SOTA && os.program != "SOTA")
      continue;

    std::stringstream ss;
    ss << std::left << std::setw(6) << os.mode << std::setw(10) << os.call
       << os.ref << " (" << os.program << ")";
    rows.push_back(ss.str());
    if (rows.size() >= 12)
      break;
  }
  if (rows.empty() && data.valid) {
    std::string prog = (filter_ == Filter::POTA) ? "POTA"
                     : (filter_ == Filter::SOTA) ? "SOTA"
                     : "";
    rows.push_back("No active" + (prog.empty() ? "" : " " + prog) + " spots");
  }
  setRows(rows);
}

void ONTAPanel::update() {
  uint32_t nowTicks = SDL_GetTicks();
  if (nowTicks - lastFetch_ > 5 * 60 * 1000 || lastFetch_ == 0) {
    lastFetch_ = nowTicks;
    provider_.fetch();
  }

  auto data = store_->get();
  if (data.lastUpdated != lastUpdate_) {
    rebuildRows(data);
    lastUpdate_ = data.lastUpdated;
  }
}

void ONTAPanel::render(SDL_Renderer *renderer) {
  // Let ListPanel draw background, border, title, and rows
  ListPanel::render(renderer);

  // Overlay filter chip in the top-right of the title row
  if (!fontMgr_.ready())
    return;

  ThemeColors themes = getThemeColors(theme_);
  int pad = std::max(2, static_cast<int>(width_ * 0.03f));

  // Build chip label: "[ALL]" / "[POTA]" / "[SOTA]"
  std::string chip = "[";
  chip += filterLabel(filter_);
  chip += "]";

  // Measure chip text at rowFontSize_
  int chipFontSize = rowFontSize_;
  TTF_Font *font = fontMgr_.getFont(chipFontSize);
  if (!font)
    return;

  int cw = 0, ch = 0;
  TTF_SizeText(font, chip.c_str(), &cw, &ch);

  // Position: right-aligned, same Y as title row
  int chipX = x_ + width_ - pad - cw;
  int chipY = y_ + pad;
  chipRect_ = {chipX, chipY, cw, ch};

  // Draw chip with accent color (active filter) or dim color (all selected)
  SDL_Color chipColor = (filter_ != Filter::ALL) ? themes.accent : themes.info;
  fontMgr_.drawText(renderer, chip, chipX, chipY, chipColor, chipFontSize);
}

bool ONTAPanel::onMouseUp(int mx, int my, Uint16 mod) {
  (void)mod;

  // Bounds check
  if (mx < x_ || mx >= x_ + width_ || my < y_ || my >= y_ + height_)
    return false;

  // Check chip hit
  if (chipRect_.w > 0 &&
      mx >= chipRect_.x && mx < chipRect_.x + chipRect_.w &&
      my >= chipRect_.y && my < chipRect_.y + chipRect_.h) {
    // Cycle: ALL -> POTA -> SOTA -> ALL
    switch (filter_) {
    case Filter::ALL:  filter_ = Filter::POTA; break;
    case Filter::POTA: filter_ = Filter::SOTA; break;
    case Filter::SOTA: filter_ = Filter::ALL;  break;
    }

    // Rebuild rows immediately with current data
    rebuildRows(store_->get());

    if (onFilterChanged_) {
      std::string fstr;
      switch (filter_) {
      case Filter::POTA: fstr = "pota"; break;
      case Filter::SOTA: fstr = "sota"; break;
      default:           fstr = "all";  break;
      }
      onFilterChanged_(fstr);
    }
    return true;
  }

  return false;
}
