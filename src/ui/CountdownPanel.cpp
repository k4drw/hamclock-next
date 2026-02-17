#include "CountdownPanel.h"
#include "../core/Astronomy.h"
#include "../core/Constants.h"
#include "FontManager.h"

#include <cstdio>
#include <ctime>

CountdownPanel::CountdownPanel(int x, int y, int w, int h, FontManager &fontMgr,
                               AppConfig &config)
    : Widget(x, y, w, h), fontMgr_(fontMgr), config_(config) {
  update(); // Initial parse
}

void CountdownPanel::update() {
  if (config_.countdownTime.empty()) {
    targetTime_ = std::chrono::system_clock::time_point(); // Epoch
    return;
  }

  struct tm t = {0};
  if (std::sscanf(config_.countdownTime.c_str(), "%d-%d-%d %d:%d", &t.tm_year,
                  &t.tm_mon, &t.tm_mday, &t.tm_hour, &t.tm_min) == 5) {
    t.tm_year -= 1900;
    t.tm_mon -= 1; // 0-11
    targetTime_ =
        std::chrono::system_clock::from_time_t(Astronomy::portable_timegm(&t));
  }
}

void CountdownPanel::render(SDL_Renderer *renderer) {
  if (!fontMgr_.ready())
    return;

  SDL_SetRenderDrawColor(renderer, 20, 20, 30, 255);
  SDL_Rect rect = {x_, y_, width_, height_};
  SDL_RenderFillRect(renderer, &rect);
  SDL_SetRenderDrawColor(renderer, 80, 80, 80, 255);
  SDL_RenderDrawRect(renderer, &rect);

  int centerY = y_ + height_ / 2;
  int centerX = x_ + width_ / 2;

  std::string label =
      config_.countdownLabel.empty() ? "Click to set" : config_.countdownLabel;
  fontMgr_.drawText(renderer, label, centerX, y_ + 14, {0, 200, 255, 255}, 11,
                    true, true);

  if (targetTime_ == std::chrono::system_clock::time_point()) {
    fontMgr_.drawText(renderer, "No target set", centerX, centerY,
                      {150, 150, 150, 255}, 14, true, true);
    return;
  }

  auto now = std::chrono::system_clock::now();
  auto diff =
      std::chrono::duration_cast<std::chrono::seconds>(targetTime_ - now)
          .count();

  if (diff <= 0) {
    fontMgr_.drawText(renderer, "EVENT ACTIVE!", centerX, centerY,
                      {255, 0, 0, 255}, 15, true, true);
  } else {
    int days = diff / 86400;
    int hours = (diff % 86400) / 3600;
    int mins = (diff % 3600) / 60;
    int secs = diff % 60;

    char buf[64];
    if (days > 0)
      std::snprintf(buf, sizeof(buf), "%dd %02dh %02dm %02ds", days, hours,
                    mins, secs);
    else
      std::snprintf(buf, sizeof(buf), "%02dh %02dm %02ds", hours, mins, secs);

    fontMgr_.drawText(renderer, buf, centerX, centerY, {255, 255, 255, 255}, 14,
                      true, true);
  }

  fontMgr_.drawText(renderer, "Remaining", centerX, y_ + height_ - 14,
                    {100, 100, 100, 255}, 9, false, true);
}

bool CountdownPanel::onMouseUp(int mx, int my, Uint16) {
  if (editing_) {
    // If outside menu, close as cancel
    if (mx < menuRect_.x || mx >= menuRect_.x + menuRect_.w ||
        my < menuRect_.y || my >= menuRect_.y + menuRect_.h) {
      stopEditing(false);
      return true;
    }
    return handleSetupClick(mx, my);
  }

  if (mx >= x_ && mx < x_ + width_ && my >= y_ && my < y_ + height_) {
    startEditing();
    return true;
  }
  return false;
}

void CountdownPanel::startEditing() {
  editing_ = true;
  activeField_ = 0;
  labelEdit_ = config_.countdownLabel;
  timeEdit_ = config_.countdownTime;
  if (timeEdit_.empty())
    timeEdit_ = "2026-06-27 18:00";
  cursorPos_ = static_cast<int>(labelEdit_.size());
  SDL_StartTextInput();
}

void CountdownPanel::stopEditing(bool apply) {
  if (apply) {
    config_.countdownLabel = labelEdit_;
    config_.countdownTime = timeEdit_;
    update();
  }
  editing_ = false;
  SDL_StopTextInput();
}

bool CountdownPanel::onKeyDown(SDL_Keycode key, Uint16 /*mod*/) {
  if (!editing_)
    return false;

  std::string &activeText = (activeField_ == 0) ? labelEdit_ : timeEdit_;

  if (key == SDLK_RETURN || key == SDLK_KP_ENTER) {
    stopEditing(true);
  } else if (key == SDLK_ESCAPE) {
    stopEditing(false);
  } else if (key == SDLK_TAB) {
    activeField_ = (activeField_ == 0) ? 1 : 0;
    std::string &newText = (activeField_ == 0) ? labelEdit_ : timeEdit_;
    cursorPos_ = static_cast<int>(newText.size());
  } else if (key == SDLK_BACKSPACE && cursorPos_ > 0) {
    activeText.erase(cursorPos_ - 1, 1);
    --cursorPos_;
  } else if (key == SDLK_LEFT && cursorPos_ > 0) {
    --cursorPos_;
  } else if (key == SDLK_RIGHT && cursorPos_ < (int)activeText.size()) {
    ++cursorPos_;
  }
  return true;
}

bool CountdownPanel::onTextInput(const char *text) {
  if (!editing_)
    return false;

  std::string &activeText = (activeField_ == 0) ? labelEdit_ : timeEdit_;
  activeText.insert(cursorPos_, text);
  cursorPos_ += static_cast<int>(std::strlen(text));
  return true;
}

void CountdownPanel::renderSetup(SDL_Renderer *renderer) {
  // --- Centered Modal Setup ---
  int menuW = 300;
  int menuH = 180;
  int menuX = (HamClock::LOGICAL_WIDTH - menuW) / 2;
  int menuY = (HamClock::LOGICAL_HEIGHT - menuH) / 2;
  menuRect_ = {menuX, menuY, menuW, menuH};

  // Shadow/Dim background (handled by main.cpp modal system usually,
  // but we can draw a border or fill)
  SDL_SetRenderDrawColor(renderer, 20, 20, 30, 255);
  SDL_RenderFillRect(renderer, &menuRect_);
  SDL_SetRenderDrawColor(renderer, 0, 200, 255, 255);
  SDL_RenderDrawRect(renderer, &menuRect_);

  SDL_Color white = {255, 255, 255, 255};
  SDL_Color cyan = {0, 200, 255, 255};
  SDL_Color gray = {150, 150, 150, 255};
  SDL_Color orange = {255, 165, 0, 255};

  int y = menuY + 15;
  int cx = menuX + menuW / 2;

  fontMgr_.drawText(renderer, "Countdown Settings", cx, y, cyan, 16, true,
                    true);
  y += 25;

  // Label Field
  fontMgr_.drawText(renderer, "Event Label:", menuX + 15, y, white, 12);
  y += 18;
  labelRect_ = {menuX + 15, y, menuW - 30, 24};
  SDL_SetRenderDrawColor(renderer, 40, 40, 50, 255);
  SDL_RenderFillRect(renderer, &labelRect_);
  SDL_SetRenderDrawColor(renderer, (activeField_ == 0) ? 255 : 80,
                         (activeField_ == 0) ? 165 : 80, 0, 255);
  SDL_RenderDrawRect(renderer, &labelRect_);
  fontMgr_.drawText(renderer, labelEdit_, labelRect_.x + 6, labelRect_.y + 5,
                    white, 14);

  if (activeField_ == 0 && (SDL_GetTicks() / 500) % 2 == 0) {
    int tw = fontMgr_.getLogicalWidth(labelEdit_.substr(0, cursorPos_), 14);
    SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
    SDL_RenderDrawLine(renderer, labelRect_.x + 6 + tw, labelRect_.y + 4,
                       labelRect_.x + 6 + tw, labelRect_.y + 20);
  }
  y += 30;

  // Time Field
  fontMgr_.drawText(renderer, "Target Time (YYYY-MM-DD HH:MM):", menuX + 15, y,
                    white, 11);
  y += 18;
  timeRect_ = {menuX + 15, y, menuW - 30, 24};
  SDL_SetRenderDrawColor(renderer, 40, 40, 50, 255);
  SDL_RenderFillRect(renderer, &timeRect_);
  SDL_SetRenderDrawColor(renderer, (activeField_ == 1) ? 255 : 80,
                         (activeField_ == 1) ? 165 : 80, 0, 255);
  SDL_RenderDrawRect(renderer, &timeRect_);
  fontMgr_.drawText(renderer, timeEdit_, timeRect_.x + 6, timeRect_.y + 5,
                    white, 14);

  if (activeField_ == 1 && (SDL_GetTicks() / 500) % 2 == 0) {
    int tw = fontMgr_.getLogicalWidth(timeEdit_.substr(0, cursorPos_), 14);
    SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
    SDL_RenderDrawLine(renderer, timeRect_.x + 6 + tw, timeRect_.y + 4,
                       timeRect_.x + 6 + tw, timeRect_.y + 20);
  }
  y += 35;

  // Buttons
  int btnW = 80;
  int btnH = 26;
  cancelRect_ = {cx - btnW - 10, y, btnW, btnH};
  okRect_ = {cx + 10, y, btnW, btnH};

  SDL_SetRenderDrawColor(renderer, 80, 30, 30, 255);
  SDL_RenderFillRect(renderer, &cancelRect_);
  SDL_SetRenderDrawColor(renderer, 200, 80, 80, 255);
  SDL_RenderDrawRect(renderer, &cancelRect_);
  fontMgr_.drawText(renderer, "Cancel", cancelRect_.x + btnW / 2,
                    cancelRect_.y + btnH / 2, white, 12, false, true);

  SDL_SetRenderDrawColor(renderer, 30, 80, 30, 255);
  SDL_RenderFillRect(renderer, &okRect_);
  SDL_SetRenderDrawColor(renderer, 80, 200, 80, 255);
  SDL_RenderDrawRect(renderer, &okRect_);
  fontMgr_.drawText(renderer, "Done", okRect_.x + btnW / 2,
                    okRect_.y + btnH / 2, white, 12, false, true);
}

bool CountdownPanel::handleSetupClick(int mx, int my) {
  if (mx >= labelRect_.x && mx < labelRect_.x + labelRect_.w &&
      my >= labelRect_.y && my < labelRect_.y + labelRect_.h) {
    activeField_ = 0;
    cursorPos_ = (int)labelEdit_.size();
    return true;
  }
  if (mx >= timeRect_.x && mx < timeRect_.x + timeRect_.w &&
      my >= timeRect_.y && my < timeRect_.y + timeRect_.h) {
    activeField_ = 1;
    cursorPos_ = (int)timeEdit_.size();
    return true;
  }
  if (mx >= okRect_.x && mx < okRect_.x + okRect_.w && my >= okRect_.y &&
      my < okRect_.y + okRect_.h) {
    stopEditing(true);
    return true;
  }
  if (mx >= cancelRect_.x && mx < cancelRect_.x + cancelRect_.w &&
      my >= cancelRect_.y && my < cancelRect_.y + cancelRect_.h) {
    stopEditing(false);
    return true;
  }
  return false;
}
