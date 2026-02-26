#include "StopwatchPanel.h"
#include "../core/Theme.h"
#include "FontCatalog.h"
#include <cstdio>

StopwatchPanel::StopwatchPanel(int x, int y, int w, int h, FontManager &fontMgr)
    : Widget(x, y, w, h), fontMgr_(fontMgr) {}

void StopwatchPanel::update() {
  // Nothing to update periodically, time is calculated during render
}

void StopwatchPanel::render(SDL_Renderer *renderer) {
  if (!fontMgr_.ready())
    return;

  if (showSetup_) {
    renderSetup(renderer);
    return;
  }

  ThemeColors themes = getThemeColors(theme_);

  // Background and border
  SDL_SetRenderDrawColor(renderer, themes.bg.r, themes.bg.g, themes.bg.b,
                         themes.bg.a);
  SDL_Rect rect = {x_, y_, width_, height_};
  SDL_RenderFillRect(renderer, &rect);
  SDL_SetRenderDrawColor(renderer, themes.border.r, themes.border.g,
                         themes.border.b, themes.border.a);
  SDL_RenderDrawRect(renderer, &rect);

  // Unified Title Bar (Match standard font size/look)
  int titleH = 20;
  fontMgr_.catalog()->drawText(renderer, "Stopwatch", x_ + 10, y_ + 5, themes.accent,
                               FontStyle::MicroBold);

  // Separator line
  SDL_SetRenderDrawColor(renderer, themes.border.r, themes.border.g,
                         themes.border.b, 60);
  SDL_RenderDrawLine(renderer, x_ + 5, y_ + titleH, x_ + width_ - 5,
                     y_ + titleH);

  auto elapsed = accumulated_;
  if (running_) {
    elapsed += std::chrono::steady_clock::now() - startTime_;
  }

  std::string timeStr = formatTime(elapsed);
  int cx = x_ + width_ / 2;
  int cy = y_ + titleH + (height_ - titleH) / 2 - 5;

  // Render Time String with high quality
  int tw, th;
  SDL_Texture *tex =
      fontMgr_.renderText(renderer, timeStr, themes.text, 18, &tw, &th);
  if (tex) {
    SDL_Rect dst = {cx - tw / 2, cy - th / 2, tw, th};
    SDL_RenderCopy(renderer, tex, nullptr, &dst);
    MemoryMonitor::getInstance().destroyTexture(tex);
  }

  // Draw discrete buttons at the bottom (Premium style)
  int btnW = 60; // Standardized button width
  int btnH = 24; // Standardized button height
  int btnY = y_ + height_ - btnH - 6;

  SDL_Color white = themes.text;

  // Start/Stop
  SDL_Rect ssRect = {cx - btnW - 10, btnY, btnW, btnH};
  if (running_) {
    SDL_SetRenderDrawColor(renderer, 60, 20, 20, 255); // Red-ish bg
  } else {
    SDL_SetRenderDrawColor(renderer, 20, 60, 20, 255); // Green-ish bg
  }
  SDL_RenderFillRect(renderer, &ssRect);
  SDL_SetRenderDrawColor(renderer, running_ ? 150 : 50, running_ ? 50 : 150, 50,
                         255);
  SDL_RenderDrawRect(renderer, &ssRect);

  tex = fontMgr_.renderText(renderer, running_ ? "Stop" : "Start", white, 10,
                            &tw, &th);
  if (tex) {
    SDL_Rect dst = {ssRect.x + (btnW - tw) / 2, ssRect.y + (btnH - th) / 2, tw,
                    th};
    SDL_RenderCopy(renderer, tex, nullptr, &dst);
    MemoryMonitor::getInstance().destroyTexture(tex);
  }

  // Reset
  SDL_Rect rRect = {cx + 10, btnY, btnW, btnH};
  SDL_SetRenderDrawColor(renderer, 30, 30, 40, 255);
  SDL_RenderFillRect(renderer, &rRect);
  SDL_SetRenderDrawColor(renderer, 80, 80, 100, 255);
  SDL_RenderDrawRect(renderer, &rRect);

  tex = fontMgr_.renderText(renderer, "Reset", white, 10, &tw, &th);
  if (tex) {
    SDL_Rect dst = {rRect.x + (btnW - tw) / 2, rRect.y + (btnH - th) / 2, tw,
                    th};
    SDL_RenderCopy(renderer, tex, nullptr, &dst);
    MemoryMonitor::getInstance().destroyTexture(tex);
  }
}

bool StopwatchPanel::onMouseUp(int mx, int my, Uint16, int) {
  if (showSetup_) {
    return handleSetupClick(mx, my);
  }

  int cx = x_ + width_ / 2;
  int btnW = 60;
  int btnH = 24;
  int btnY = y_ + height_ - btnH - 6;

  // Check buttons first
  if (my >= btnY && my < btnY + btnH) {
    if (mx >= cx - btnW - 10 && mx < cx - 10) {
      performAction("Toggle");
      return true;
    } else if (mx >= cx + 10 && mx < cx + 10 + btnW) {
      performAction("Reset");
      return true;
    }
  }

  // Click is handled by PaneContainer if not on buttons
  return false;
}

void StopwatchPanel::renderSetup(SDL_Renderer *renderer) {
  ThemeColors themes = getThemeColors(theme_);
  SDL_SetRenderDrawColor(renderer, themes.bg.r, themes.bg.g, themes.bg.b, 255);
  SDL_Rect bg = {x_, y_, width_, height_};
  SDL_RenderFillRect(renderer, &bg);
  SDL_SetRenderDrawColor(renderer, themes.border.r, themes.border.g,
                         themes.border.b, 255);
  SDL_RenderDrawRect(renderer, &bg);

  SDL_Color cyan = themes.accent;
  SDL_Color white = themes.text;
  int cx = x_ + width_ / 2;
  int y = y_ + 10;

  int tw, th;
  SDL_Texture *t = fontMgr_.renderText(renderer, "--- Stopwatch Setup ---",
                                       cyan, 14, &tw, &th);
  if (t) {
    SDL_Rect tr = {cx - tw / 2, y, tw, th};
    SDL_RenderCopy(renderer, t, nullptr, &tr);
    MemoryMonitor::getInstance().destroyTexture(t);
  }
  y += th + 15;

  t = fontMgr_.renderText(renderer, "No settings for Stopwatch yet.", white, 10,
                          &tw, &th);
  if (t) {
    SDL_Rect tr = {cx - tw / 2, y, tw, th};
    SDL_RenderCopy(renderer, t, nullptr, &tr);
    MemoryMonitor::getInstance().destroyTexture(t);
  }

  // Bottom buttons
  int btnW = 60;
  int btnH = 24;
  int btnY = y_ + height_ - btnH - 6;

  // Done button
  doneRect_ = {cx - btnW / 2, btnY, btnW, btnH};
  SDL_SetRenderDrawColor(renderer, 20, 60, 20, 255);
  SDL_RenderFillRect(renderer, &doneRect_);
  SDL_SetRenderDrawColor(renderer, 50, 150, 50, 255);
  SDL_RenderDrawRect(renderer, &doneRect_);
  t = fontMgr_.renderText(renderer, "Done", white, 10, &tw, &th);
  if (t) {
    SDL_Rect tr = {doneRect_.x + (btnW - tw) / 2, doneRect_.y + (btnH - th) / 2,
                   tw, th};
    SDL_RenderCopy(renderer, t, nullptr, &tr);
    MemoryMonitor::getInstance().destroyTexture(t);
  }
}

bool StopwatchPanel::handleSetupClick(int mx, int my) {
  if (mx >= doneRect_.x && mx < doneRect_.x + doneRect_.w &&
      my >= doneRect_.y && my < doneRect_.y + doneRect_.h) {
    showSetup_ = false;
    return true;
  }
  return true;
}

bool StopwatchPanel::performAction(const std::string &action) {
  if (action == "Toggle") {
    if (running_) {
      accumulated_ += std::chrono::steady_clock::now() - startTime_;
      running_ = false;
    } else {
      startTime_ = std::chrono::steady_clock::now();
      running_ = true;
    }
    return true;
  } else if (action == "Reset") {
    running_ = false;
    accumulated_ = std::chrono::steady_clock::duration::zero();
    return true;
  }
  return false;
}

std::string
StopwatchPanel::formatTime(std::chrono::steady_clock::duration d) const {
  auto h = std::chrono::duration_cast<std::chrono::hours>(d);
  d -= h;
  auto m = std::chrono::duration_cast<std::chrono::minutes>(d);
  d -= m;
  auto s = std::chrono::duration_cast<std::chrono::seconds>(d);
  d -= s;
  auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(d);

  char buf[32];
  if (h.count() > 0) {
    std::snprintf(buf, sizeof(buf), "%02d:%02d:%02d.%01d", (int)h.count(),
                  (int)m.count(), (int)s.count(), (int)(ms.count() / 100));
  } else {
    std::snprintf(buf, sizeof(buf), "%02d:%02d.%01d", (int)m.count(),
                  (int)s.count(), (int)(ms.count() / 100));
  }
  return buf;
}
