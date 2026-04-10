#include "WorldClockPanel.h"
#include "RenderUtils.h"
#include "WidgetRegistry.h"
#include "../core/Astronomy.h"
#include <cstdio>
#include <ctime>

WorldClockPanel::WorldClockPanel(int x, int y, int w, int h, ConfigManager &configMgr, FontManager &fontMgr)
    : Widget(x, y, w, h), configMgr_(configMgr), fontMgr_(fontMgr) {
  recalcLayout();
}

void WorldClockPanel::update() {
  // Logic for time calculation is handled during render/on-the-fly
  // or we could cache strings here. For simplicity, we'll calculate during render.
}

void WorldClockPanel::render(SDL_Renderer *renderer) {
  renderChrome(renderer);
  if (configuring_) {
    renderConfig(renderer);
  } else {
    renderMain(renderer);
  }
}

void WorldClockPanel::renderMain(SDL_Renderer *renderer) {
  renderTitle(renderer, fontMgr_, "World Clock");
  
  auto *cat = fontMgr_.catalog();
  if (!cat) return;
  
  ThemeColors themes = getThemeColors(theme_);
  AppConfig &config = configMgr_.getConfig();
  
  // Draw gear icon in bottom strip
  cat->drawText(renderer, "Setup", ui_.gearRect.x + width_ / 2 - 10, ui_.gearRect.y + (ui_.gearRect.h - 10)/2, themes.textDim, FontStyle::Tiny);

  std::time_t now = std::time(nullptr);
  int activeCount = 0;
  for (const auto &wc : config.worldClocks) {
    if (wc.active) activeCount++;
  }

  if (activeCount == 0) {
    cat->drawText(renderer, "No clocks active", x_ + 10, y_ + 40, themes.textDim, FontStyle::Micro);
    return;
  }

  int titleH = height_ / 10;
  int footerH = height_ / 10;
  int mainH = height_ - titleH - footerH - 10;
  int rowH = mainH / activeCount;
  int curY = y_ + titleH + 5;

  for (const auto &wc : config.worldClocks) {
    if (!wc.active) continue;

    // Calculate time
    std::time_t localT = now + (wc.offsetMinutes * 60);
    struct tm tm;
    Astronomy::portable_gmtime(&localT, &tm);

    char timeStr[16];
    std::snprintf(timeStr, sizeof(timeStr), "%02d:%02d", tm.tm_hour, tm.tm_min);

    // Label
    cat->drawText(renderer, wc.label, x_ + 10, curY + (rowH - 12)/2, themes.textDim, FontStyle::Micro);
    
    // Time (right-aligned; drawText with rightAlign+vertCentered avoids a
    // throwaway renderText call that would leak a GPU texture each frame)
    cat->drawText(renderer, timeStr, x_ + width_ - 10, curY + rowH / 2,
                  themes.text, FontStyle::MediumBold, false, true, true);

    curY += rowH;
  }
}

void WorldClockPanel::renderConfig(SDL_Renderer *renderer) {
  renderTitle(renderer, fontMgr_, "World Clock Setup");
  
  auto *cat = fontMgr_.catalog();
  if (!cat) return;
  
  ThemeColors themes = getThemeColors(theme_);
  AppConfig &config = configMgr_.getConfig();
  if (tempClocks_.empty()) tempClocks_ = config.worldClocks;

  for (int i = 0; i < 4; ++i) {
    auto &wc = tempClocks_[i];
    SDL_SetRenderDrawColor(renderer, themes.border.r, themes.border.g, themes.border.b, 50);
    SDL_RenderDrawRect(renderer, &ui_.labelRects[i]);

    // Active checkbox (Box)
    SDL_SetRenderDrawColor(renderer, themes.border.r, themes.border.g, themes.border.b, 255);
    SDL_RenderDrawRect(renderer, &ui_.activeRects[i]);
    if (wc.active) {
        SDL_SetRenderDrawColor(renderer, themes.accent.r, themes.accent.g, themes.accent.b, 255);
        SDL_Rect inner = {ui_.activeRects[i].x + 2, ui_.activeRects[i].y + 2, 
                          ui_.activeRects[i].w - 4, ui_.activeRects[i].h - 4};
        SDL_RenderFillRect(renderer, &inner);
    }

    // Label or TextInput
    if (editingIndex_ == i) {
        labelInput_.render(renderer, fontMgr_, ui_.labelRects[i].x, ui_.labelRects[i].y, 
                          ui_.labelRects[i].w, ui_.labelRects[i].h, 
                          FontStyle::Micro, 4, true, true, 
                          themes.accent, themes.border, themes.accent, themes.text, themes.textDim);
    } else {
        std::string label = wc.label.empty() ? "(empty)" : wc.label;
        cat->drawText(renderer, label, ui_.labelRects[i].x + 4, ui_.labelRects[i].y + 2, 
                      wc.label.empty() ? themes.textDim : themes.text, FontStyle::Micro);
    }

    // Offset
    char offStr[16];
    char sign = (wc.offsetMinutes < 0) ? '-' : '+';
    int absMins = std::abs(wc.offsetMinutes);
    int h = absMins / 60;
    int m = absMins % 60;
    if (m == 0) std::snprintf(offStr, sizeof(offStr), "%c%d", sign, h);
    else std::snprintf(offStr, sizeof(offStr), "%c%d:%02d", sign, h, m);
    
    // Minus/Plus boxes
    auto drawSmallBtn = [&](const SDL_Rect &r, const char *lbl) {
        SDL_SetRenderDrawColor(renderer, themes.border.r, themes.border.g, themes.border.b, 60);
        SDL_RenderFillRect(renderer, &r);
        SDL_SetRenderDrawColor(renderer, themes.border.r, themes.border.g, themes.border.b, 255);
        SDL_RenderDrawRect(renderer, &r);
        cat->drawText(renderer, lbl, r.x + r.w / 2, r.y + r.h / 2, themes.text, FontStyle::Micro, true, false, true);
    };

    drawSmallBtn(ui_.minusRects[i], "-");
    cat->drawText(renderer, offStr, ui_.minusRects[i].x + ui_.minusRects[i].w + 5, 
                  ui_.minusRects[i].y + ui_.minusRects[i].h / 2, themes.text, FontStyle::Micro, false, false, true);
    drawSmallBtn(ui_.plusRects[i], "+");
  }

  // Done and Cancel buttons
  auto drawConfigBtn = [&](const SDL_Rect &r, const char *label, SDL_Color color) {
    SDL_SetRenderDrawColor(renderer, color.r, color.g, color.b, 255);
    SDL_RenderFillRect(renderer, &r);
    cat->drawText(renderer, label, r.x + r.w / 2, r.y + r.h / 2,
                  themes.bg, FontStyle::MicroBold, true, false, true);
  };

  drawConfigBtn(ui_.cancelRect, "Cancel", themes.danger);
  drawConfigBtn(ui_.doneRect, "Done", themes.success);
}

void WorldClockPanel::recalcLayout() {
  int titleH = height_ / 10;
  int footerH = height_ / 10;
  ui_.gearRect = {x_, y_ + height_ - footerH, width_, footerH};
  
  int startY = y_ + titleH + 2;
  // Rows must fit above the cancel/done buttons (bottom 38px); cap at 36px so they
  // stay compact on larger widgets rather than spreading to fill all available space.
  int avail = (height_ - titleH - 38) / 4;
  int rowH = avail < 36 ? avail : 36;
  for (int i = 0; i < 4; ++i) {
    ui_.activeRects[i] = {x_ + 5, startY + i * rowH + (rowH - 10)/2, 10, 10};
    ui_.labelRects[i] = {x_ + 20, startY + i * rowH + (rowH - 20) / 2, width_ - 95, 20};
    ui_.minusRects[i] = {x_ + width_ - 70, startY + i * rowH + (rowH - 10)/2, 10, 10};
    ui_.plusRects[i] = {x_ + width_ - 15, startY + i * rowH + (rowH - 10)/2, 10, 10};
  }

  int btnW = (width_ - 30) / 2;
  ui_.cancelRect = {x_ + 10, y_ + height_ - 28, btnW, 22};
  ui_.doneRect = {x_ + width_ - 10 - btnW, y_ + height_ - 28, btnW, 22};
}

void WorldClockPanel::onResize(int x, int y, int w, int h) {
  Widget::onResize(x, y, w, h);
  recalcLayout();
}

bool WorldClockPanel::onMouseDown(int mx, int my, Uint16 mod, int clicks) {
  if (my < y_ + height_ / 10) return false;
  return true; // Capture mouse to prevent pane drag/swap if necessary, or just return true for hits
}

bool WorldClockPanel::onMouseUp(int mx, int my, Uint16 mod, int clicks) {
  if (my < y_ + height_ / 10) return false;

  SDL_Point p = {mx, my};
  if (configuring_) {
    if (SDL_PointInRect(&p, &ui_.doneRect)) {
      if (editingIndex_ != -1) {
        tempClocks_[editingIndex_].label = labelInput_.getValue();
        editingIndex_ = -1;
      }
      configMgr_.getConfig().worldClocks = tempClocks_;
      configuring_ = false;
      configMgr_.save(configMgr_.getConfig());
      tempClocks_.clear();
      return true;
    }
    if (SDL_PointInRect(&p, &ui_.cancelRect)) {
      editingIndex_ = -1;
      configuring_ = false;
      tempClocks_.clear();
      return true;
    }

    for (int i = 0; i < 4; ++i) {
      if (SDL_PointInRect(&p, &ui_.activeRects[i])) {
        tempClocks_[i].active = !tempClocks_[i].active;
        return true;
      }
      if (SDL_PointInRect(&p, &ui_.labelRects[i])) {
        if (editingIndex_ != -1) {
           tempClocks_[editingIndex_].label = labelInput_.getValue();
        }
        editingIndex_ = i;
        labelInput_.setValue(tempClocks_[i].label);
        labelInput_.setActive(true);
        recalcLayout();
        return true;
      }
      if (SDL_PointInRect(&p, &ui_.plusRects[i])) {
        tempClocks_[i].offsetMinutes += (mod & KMOD_SHIFT) ? 60 : 30;
        if (tempClocks_[i].offsetMinutes > 840) tempClocks_[i].offsetMinutes = 840;
        return true;
      }
      if (SDL_PointInRect(&p, &ui_.minusRects[i])) {
        tempClocks_[i].offsetMinutes -= (mod & KMOD_SHIFT) ? 60 : 30;
        if (tempClocks_[i].offsetMinutes < -720) tempClocks_[i].offsetMinutes = -720;
        return true;
      }
    }
  } else {
    if (SDL_PointInRect(&p, &ui_.gearRect)) {
      tempClocks_ = configMgr_.getConfig().worldClocks;
      configuring_ = true;
      recalcLayout();
      return true;
    }
  }
  return false;
}

void WorldClockPanel::onMouseMove(int mx, int my) {
  (void)mx; (void)my;
}

bool WorldClockPanel::onTextInput(const char *text) {
  if (configuring_ && editingIndex_ != -1) {
    return labelInput_.onTextInput(text);
  }
  return false;
}

bool WorldClockPanel::onKeyDown(SDL_Keycode key, Uint16 mod) {
  if (configuring_) {
    if (editingIndex_ != -1) {
      if (key == SDLK_RETURN || key == SDLK_KP_ENTER || key == SDLK_ESCAPE) {
        tempClocks_[editingIndex_].label = labelInput_.getValue();
        editingIndex_ = -1;
        recalcLayout();
        return true;
      }
      return labelInput_.onKeyDown(key, mod);
    }
    if (key == SDLK_ESCAPE) {
      configuring_ = false;
      tempClocks_.clear();
      return true;
    }
    if (key == SDLK_RETURN || key == SDLK_KP_ENTER) {
      configMgr_.getConfig().worldClocks = tempClocks_;
      configuring_ = false;
      configMgr_.save(configMgr_.getConfig());
      tempClocks_.clear();
      return true;
    }
  }
  return false;
}

// Widget Registry Registration
REGISTER_WIDGET("world_clock", "World Clock", false, false, {
  return std::make_unique<WorldClockPanel>(0, 0, 0, 0, deps.cfgMgr, deps.fontMgr);
});
