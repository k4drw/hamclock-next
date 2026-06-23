#include "HurricanePanel.h"
#include "../core/Theme.h"
#include "FontCatalog.h"
#include <algorithm>
#include <cstdio>

HurricanePanel::HurricanePanel(int x, int y, int w, int h, FontManager &fontMgr,
                               std::shared_ptr<HurricaneStore> store)
    : Widget(x, y, w, h), fontMgr_(fontMgr), store_(std::move(store)) {}

void HurricanePanel::update() { currentData_ = store_->get(); }

SDL_Color HurricanePanel::categoryColor(int category) {
  switch (category) {
  case 5:
    return {255, 0, 0, 255}; // Extreme danger — red
  case 4:
    return {255, 60, 0, 255};
  case 3:
    return {255, 140, 0, 255};
  case 2:
    return {255, 220, 0, 255};
  case 1:
    return {255, 255, 80, 255};
  case 0:
    return {200, 200, 200, 255}; // Invest
  default:
    return {180, 180, 255, 255}; // TS or TD
  }
}

void HurricanePanel::render(SDL_Renderer *renderer) {
  if (!fontMgr_.ready())
    return;

  ThemeColors themes = getThemeColors(theme_);

  renderChrome(renderer);

  int titleH = 20;
  int pad = 6;
  int curY = y_ + titleH + 4;
  auto *cat = fontMgr_.catalog();

  cat->drawText(renderer, "Tropics", x_ + 10, y_ + 5, themes.accent,
                FontStyle::MicroBold);

  int centerX = x_ + width_ / 2;
  if (!currentData_.valid) {
    cat->drawText(renderer, "Loading...", centerX, y_ + height_ / 2,
                  themes.textDim, FontStyle::Micro, true);
    return;
  }

  if (currentData_.storms.empty()) {
    cat->drawText(renderer, "No active storms", centerX, y_ + height_ / 2,
                  themes.success, FontStyle::Micro, true);
    return;
  }

  // Each storm block: name+category | wind | pressure | position
  int startIdx = std::max(0, scrollOffset_);
  int renderedCount = 0;

  for (int i = startIdx; i < (int)currentData_.storms.size(); ++i) {
    const auto &s = currentData_.storms[i];
    
    int neededH = nameFontSize_ + detailFontSize_ + 4;
    if (!s.movement.empty()) neededH += detailFontSize_ + 4;
    
    // Check if we have enough room for this block (leaving space for pagination text if needed)
    int pageTextH = ((int)currentData_.storms.size() > renderedCount + 1) ? detailFontSize_ : 0;
    if (curY + neededH + pageTextH > y_ + height_ - pad) {
        break; // Out of space
    }
    
    renderedCount++;

    SDL_Color col = categoryColor(s.category);

    // Highlight if selected
    if (s.id == currentData_.selectedStormId) {
      SDL_SetRenderDrawColor(renderer, 255, 255, 255, 30);
      SDL_Rect selRect = {x_ + 2, curY - 2, width_ - 4, neededH - 2};
      SDL_RenderFillRect(renderer, &selRect);
    }

    // Storm name + category
    char nameLabel[64];
    bool isInvest = (s.category == 0 && s.id.find("invest_") == 0);
    if (isInvest) {
      // Name might already have "Invest " from parser, so check to avoid "Invest Invest"
      if (s.name.find("Invest ") == 0) {
        std::snprintf(nameLabel, sizeof(nameLabel), "%s", s.name.c_str());
      } else {
        std::snprintf(nameLabel, sizeof(nameLabel), "Invest %s", s.name.c_str());
      }
    } else if (s.category >= 1) {
      std::snprintf(nameLabel, sizeof(nameLabel), "%s (Cat %d)", s.name.c_str(),
                    s.category);
    } else {
      std::snprintf(nameLabel, sizeof(nameLabel), "%s (TS/TD)", s.name.c_str());
    }
    cat->drawText(renderer, nameLabel, x_ + pad, curY, col, FontStyle::Fast);

    curY += nameFontSize_ + 2;

    // Wind + pressure
    char wBuf[48];
    std::snprintf(wBuf, sizeof(wBuf), "%dkt  %dmb  %.1fN %.1f%c", s.maxWindKt,
                  s.pressureMb, std::abs(s.lat), std::abs(s.lon),
                  s.lon < 0 ? 'W' : 'E');
    cat->drawText(renderer, wBuf, x_ + pad, curY, themes.textDim,
                  FontStyle::Micro);
    curY += detailFontSize_ + 2;

    // Movement
    if (!s.movement.empty()) {
      std::string mv = s.movement;
      if (mv.size() > 28)
        mv = mv.substr(0, 27) + "~";
      cat->drawText(renderer, mv, x_ + pad, curY, themes.textDim,
                    FontStyle::Micro);
      curY += detailFontSize_ + 4;
    } else {
      curY += 2; // small padding if no movement
    }

    if (i < (int)currentData_.storms.size() - 1 && curY < y_ + height_ - pad - 10) {
      SDL_SetRenderDrawColor(renderer, themes.border.r, themes.border.g,
                             themes.border.b, 100);
      SDL_RenderDrawLine(renderer, x_ + pad, curY - 2, x_ + width_ - pad,
                         curY - 2);
    }
  }

  // maxScroll_ can be updated during render so mouse wheel works
  maxScroll_ = std::max(0, (int)currentData_.storms.size() - renderedCount);

  if (startIdx > 0 || startIdx + renderedCount < (int)currentData_.storms.size()) {
    char buf[32];
    std::snprintf(buf, sizeof(buf), "%d/%d", startIdx + 1,
                  (int)currentData_.storms.size());
    cat->drawText(renderer, buf, x_ + width_ - pad,
                  y_ + height_ - pad - detailFontSize_, themes.textDim,
                  FontStyle::Micro, false, true);
  }
}

void HurricanePanel::onResize(int x, int y, int w, int h) {
  Widget::onResize(x, y, w, h);
  auto *cat = fontMgr_.catalog();
  titleFontSize_ = cat->ptSize(FontStyle::FastBold);
  nameFontSize_ = cat->ptSize(FontStyle::Fast);
  detailFontSize_ = cat->ptSize(FontStyle::Micro);
  scrollOffset_ = 0;
}

bool HurricanePanel::onMouseWheel(int scrollY) {
  if (scrollY > 0) {
    scrollOffset_ = std::max(0, scrollOffset_ - 1);
  } else if (scrollY < 0) {
    scrollOffset_ = std::min(maxScroll_, scrollOffset_ + 1);
  }
  return true;
}

bool HurricanePanel::onMouseUp(int mx, int my, Uint16 mod, int clicks) {
  if (currentData_.storms.empty()) return false;

  int titleH = 20;
  int curY = y_ + titleH + 4;
  int startIdx = std::max(0, scrollOffset_);
  
  for (int i = startIdx; i < (int)currentData_.storms.size(); ++i) {
    const auto &s = currentData_.storms[i];
    int neededH = nameFontSize_ + detailFontSize_ + 4;
    if (!s.movement.empty()) neededH += detailFontSize_ + 4;

    if (my >= curY && my < curY + neededH) {
      // Clicked on this storm!
      auto data = store_->get();
      if (data.selectedStormId == s.id) {
          data.selectedStormId = ""; // Toggle off
      } else {
          data.selectedStormId = s.id;
      }
      store_->update(data);
      return true;
    }
    
    curY += neededH;
    if (curY > y_ + height_) break;
  }
  
  return false;
}

#include "WidgetRegistry.h"
REGISTER_WIDGET("hurricane", "Tropics", false, false, {
  return std::make_unique<HurricanePanel>(0, 0, 0, 0, deps.fontMgr, deps.hurricaneStore);
})
