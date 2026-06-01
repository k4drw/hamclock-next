#include "StopwatchPanel.h"
#include "../core/Theme.h"
#include "FontCatalog.h"
#include <cstdio>

StopwatchPanel::StopwatchPanel(int x, int y, int w, int h, FontManager &fontMgr)
    : Widget(x, y, w, h), fontMgr_(fontMgr) {}

void StopwatchPanel::update() {
  // Nothing to update periodically; time is calculated during render
}

void StopwatchPanel::onResize(int x, int y, int w, int h) {
  Widget::onResize(x, y, w, h);
  ssRect_ = {};
  lapRect_ = {};
  rRect_ = {};
  doneRect_ = {};
}

std::chrono::steady_clock::duration StopwatchPanel::elapsed() const {
  auto e = accumulated_;
  if (running_)
    e += std::chrono::steady_clock::now() - startTime_;
  return e;
}

void StopwatchPanel::render(SDL_Renderer *renderer) {
  if (!fontMgr_.ready())
    return;

  if (showSetup_) {
    renderSetup(renderer);
    return;
  }

  ThemeColors themes = getThemeColors(theme_);

  renderChrome(renderer);

  // Title Bar
  int titleH = 20;
  fontMgr_.catalog()->drawText(renderer, "Stopwatch", x_ + 10, y_ + 5,
                               themes.accent, FontStyle::MicroBold);

  // Separator line
  SDL_SetRenderDrawColor(renderer, themes.border.r, themes.border.g,
                         themes.border.b, 60);
  SDL_RenderDrawLine(renderer, x_ + 5, y_ + titleH, x_ + width_ - 5,
                     y_ + titleH);

  auto elapsedDur = elapsed();
  std::string timeStr = formatTime(elapsedDur);

  // Button layout - 3 buttons
  int btnH = 24;
  int btnY = y_ + height_ - btnH - 6;
  int btnW = (width_ - 8) / 3;
  ssRect_ = {x_ + 4, btnY, btnW, btnH};
  lapRect_ = {x_ + 4 + btnW + 2, btnY, btnW, btnH};
  rRect_ = {x_ + 4 + btnW * 2 + 4, btnY, btnW, btnH};

  // Available space for time + laps
  int bodyH = btnY - y_ - titleH - 6;
  int bodyY = y_ + titleH + 4;
  int cx = x_ + width_ / 2;

  // Lap table (if any laps recorded)
  const int kMaxVisible = 5;
  int lapTableH = 0;
  int lapRowH = 13;
  if (!laps_.empty()) {
    lapTableH = std::min((int)laps_.size(), kMaxVisible) * lapRowH + 4;
  }

  // Main time display
  int timeAreaH = bodyH - lapTableH;
  int cy = bodyY + timeAreaH / 3; // Shift up slightly to make room for current lap
  int tw, th;
  SDL_Texture *tex =
      fontMgr_.renderText(renderer, timeStr, themes.text, 18, &tw, &th);
  if (tex) {
    SDL_Rect dst = {cx - tw / 2, cy - th / 2, tw, th};
    SDL_RenderCopy(renderer, tex, nullptr, &dst);
    MemoryMonitor::getInstance().destroyTexture(tex);
  }

  // Current Lap time (if laps exist)
  if (running_ && !laps_.empty()) {
    auto currentLapDur = elapsed() - laps_.back();
    std::string lapStr = "Lap " + std::to_string(laps_.size() + 1) + ": " +
                         formatTimeMini(currentLapDur);
    int lw, lh;
    SDL_Texture *ltex =
        fontMgr_.renderText(renderer, lapStr, themes.textDim, 10, &lw, &lh);
    if (ltex) {
      SDL_Rect ldst = {cx - lw / 2, cy + th / 2 + 2, lw, lh};
      SDL_RenderCopy(renderer, ltex, nullptr, &ldst);
      MemoryMonitor::getInstance().destroyTexture(ltex);
    }
  } else if (running_) {
    // First lap
    std::string lapStr = "Lap 1: " + formatTimeMini(elapsedDur);
    int lw, lh;
    SDL_Texture *ltex =
        fontMgr_.renderText(renderer, lapStr, themes.textDim, 10, &lw, &lh);
    if (ltex) {
      SDL_Rect ldst = {cx - lw / 2, cy + th / 2 + 2, lw, lh};
      SDL_RenderCopy(renderer, ltex, nullptr, &ldst);
      MemoryMonitor::getInstance().destroyTexture(ltex);
    }
  }

  // Lap table
  if (!laps_.empty()) {
    int lyBase = bodyY + timeAreaH + 2;
    int start = std::max(0, (int)laps_.size() - kMaxVisible);
    // Header divider
    SDL_SetRenderDrawColor(renderer, themes.border.r, themes.border.g, themes.border.b, 255);
    SDL_RenderDrawLine(renderer, x_ + 4, lyBase, x_ + width_ - 4, lyBase);
    lyBase += 2;

    for (int i = start; i < (int)laps_.size(); ++i) {
      // Compute split (time since previous lap)
      auto split = laps_[i];
      if (i > 0)
        split = laps_[i] - laps_[i - 1];

      char lapBuf[48];
      std::snprintf(lapBuf, sizeof(lapBuf), "L%02d  %s  +%s", i + 1,
                    formatTimeMini(laps_[i]).c_str(),
                    formatTimeMini(split).c_str());

      // Highlight most recent lap
      SDL_Color lapColor = (i == (int)laps_.size() - 1)
                               ? themes.success
                               : themes.textDim;
      fontMgr_.catalog()->drawText(renderer, lapBuf, x_ + 6,
                                   lyBase + (i - start) * lapRowH + lapRowH / 2,
                                   lapColor, FontStyle::Tiny, false, false,
                                   true);
    }
  }

  // Render buttons
  auto drawBtn = [&](SDL_Rect r, const char *label, SDL_Color bg,
                     SDL_Color border) {
    SDL_SetRenderDrawColor(renderer, bg.r, bg.g, bg.b, bg.a);
    SDL_RenderFillRect(renderer, &r);
    SDL_SetRenderDrawColor(renderer, border.r, border.g, border.b, border.a);
    SDL_RenderDrawRect(renderer, &r);
    int bw, bh;
    SDL_Color txtCol =
        (bg.r == themes.success.r && bg.g == themes.success.g &&
         bg.b == themes.success.b) ||
                (bg.r == themes.danger.r && bg.g == themes.danger.g &&
                 bg.b == themes.danger.b)
            ? themes.bg
            : themes.text;
    SDL_Texture *t = fontMgr_.renderText(renderer, label, txtCol, 9, &bw, &bh);
    if (t) {
      SDL_Rect dst = {r.x + (r.w - bw) / 2, r.y + (r.h - bh) / 2, bw, bh};
      SDL_RenderCopy(renderer, t, nullptr, &dst);
      MemoryMonitor::getInstance().destroyTexture(t);
    }
  };

  // Start/Stop button
  drawBtn(ssRect_, running_ ? "Stop" : "Start",
          running_ ? themes.danger : themes.success,
          themes.border);

  // Lap button (enabled only while running)
  drawBtn(lapRect_, "Lap",
          running_ ? themes.rowStripe1 : themes.bg,
          themes.border);

  // Reset button
  drawBtn(rRect_, "Reset", themes.rowStripe2, themes.border);
}

bool StopwatchPanel::onMouseUp(int mx, int my, Uint16, int) {
  if (showSetup_)
    return handleSetupClick(mx, my);

  if (my >= ssRect_.y && my < ssRect_.y + ssRect_.h) {
    if (mx >= ssRect_.x && mx < ssRect_.x + ssRect_.w) {
      performAction("Toggle");
      return true;
    }
    if (running_ && mx >= lapRect_.x && mx < lapRect_.x + lapRect_.w) {
      performAction("Lap");
      return true;
    }
    if (mx >= rRect_.x && mx < rRect_.x + rRect_.w) {
      performAction("Reset");
      return true;
    }
  }

  return false;
}

void StopwatchPanel::renderSetup(SDL_Renderer *renderer) {
  ThemeColors themes = getThemeColors(theme_);
  SDL_SetRenderDrawColor(renderer, themes.bg.r, themes.bg.g, themes.bg.b, themes.bg.a);
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

  t = fontMgr_.renderText(renderer, "No additional settings.", white, 10, &tw,
                          &th);
  if (t) {
    SDL_Rect tr = {cx - tw / 2, y, tw, th};
    SDL_RenderCopy(renderer, t, nullptr, &tr);
    MemoryMonitor::getInstance().destroyTexture(t);
  }

  // Done button
  int btnW = 80, btnH = 28;
  int btnY = y_ + height_ - btnH - 6;
  doneRect_ = {cx - btnW / 2, btnY, btnW, btnH};
  SDL_SetRenderDrawColor(renderer, themes.success.r, themes.success.g,
                         themes.success.b, 255);
  SDL_RenderFillRect(renderer, &doneRect_);
  SDL_SetRenderDrawColor(renderer, themes.border.r, themes.border.g,
                         themes.border.b, 100);
  SDL_RenderDrawRect(renderer, &doneRect_);
  t = fontMgr_.renderText(renderer, "Done", themes.bg, 10, &tw, &th);
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
  } else if (action == "Lap") {
    // Record current total elapsed time as a lap timestamp
    if (running_) {
      laps_.push_back(elapsed());
    }
    return true;
  } else if (action == "Reset") {
    running_ = false;
    accumulated_ = std::chrono::steady_clock::duration::zero();
    laps_.clear();
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

std::string
StopwatchPanel::formatTimeMini(std::chrono::steady_clock::duration d) const {
  auto m = std::chrono::duration_cast<std::chrono::minutes>(d);
  d -= m;
  auto s = std::chrono::duration_cast<std::chrono::seconds>(d);
  d -= s;
  auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(d);
  char buf[32];
  std::snprintf(buf, sizeof(buf), "%02d:%02d.%01d", (int)m.count(),
                (int)s.count(), (int)(ms.count() / 100));
  return buf;
}

#include "WidgetRegistry.h"
REGISTER_WIDGET("stopwatch", "Stopwatch", false, false, {
  return std::make_unique<StopwatchPanel>(0, 0, 0, 0, deps.fontMgr);
})
