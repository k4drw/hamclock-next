#include "CountdownPanel.h"
#include "../core/Astronomy.h"
#include "../core/SoundManager.h"
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
    alarmTriggered_ = false;
    return;
  }

  struct tm t = {0};
  if (std::sscanf(config_.countdownTime.c_str(), "%d-%d-%d %d:%d", &t.tm_year,
                  &t.tm_mon, &t.tm_mday, &t.tm_hour, &t.tm_min) == 5) {
    t.tm_year -= 1900;
    t.tm_mon -= 1; // 0-11
    targetTime_ =
        std::chrono::system_clock::from_time_t(Astronomy::portable_timegm(&t));
    alarmTriggered_ = false;
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

  if (diff <= 0 && targetTime_.time_since_epoch().count() > 0) {
    fontMgr_.drawText(renderer, "EVENT ACTIVE!", centerX, centerY,
                      {255, 0, 0, 255}, 15, true, true);
    if (!alarmTriggered_) {
      SoundManager::getInstance().playAlarm();
      alarmTriggered_ = true;
    }
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

  if (editing_) {
    renderEditOverlay(renderer);
  }
}

bool CountdownPanel::onMouseUp(int mx, int my, Uint16) {
  if (editing_) {
    int pad = 10;
    int boxH = 24;
    int startY = y_ + 20;

    // Check clicks on fields
    if (mx >= x_ + pad && mx < x_ + width_ - pad) {
      // Label field
      if (my >= startY && my < startY + boxH) {
        if (editingTime_) {
          // Switch to Label
          tempTime_ = editText_;
          editText_ = tempLabel_;
          editingTime_ = false;
          cursorPos_ = editText_.size();
        }
        return true;
      }
      // Time field
      int timeY = startY + boxH + 20;
      if (my >= timeY && my < timeY + boxH) {
        if (!editingTime_) {
          // Switch to Time
          tempLabel_ = editText_;
          editText_ = tempTime_;
          editingTime_ = true;
          cursorPos_ = editText_.size();
        }
        return true;
      }
    }

    // Click outside -> Save and Close
    // stopEditing(true);
    return true;
  }

  // Click anywhere to open setup
  if (mx >= x_ && mx < x_ + width_ && my >= y_ && my < y_ + height_) {
    startEditing(false);
    return true;
  }
  return false;
}

void CountdownPanel::startEditing(bool editingTime) {
  editing_ = true;
  editingTime_ = false; // Start on label by default

  tempLabel_ = config_.countdownLabel;
  if (tempLabel_.empty())
    tempLabel_ = "Countdown";

  tempTime_ = config_.countdownTime;
  if (tempTime_.empty())
    tempTime_ = "2026-01-01 00:00";

  editText_ = tempLabel_;
  cursorPos_ = static_cast<int>(editText_.size());
  SDL_StartTextInput();
}

void CountdownPanel::stopEditing(bool apply) {
  if (apply) {
    if (editingTime_)
      tempTime_ = editText_;
    else
      tempLabel_ = editText_;

    config_.countdownLabel = tempLabel_;
    config_.countdownTime = tempTime_;
    update();
  }
  editing_ = false;
  SDL_StopTextInput();
}

bool CountdownPanel::onKeyDown(SDL_Keycode key, Uint16) {
  if (!editing_)
    return false;
  if (key == SDLK_RETURN || key == SDLK_KP_ENTER)
    stopEditing(true);
  else if (key == SDLK_ESCAPE)
    stopEditing(false);
  else if (key == SDLK_TAB) {
    // Switch fields
    if (editingTime_) {
      tempTime_ = editText_;
      editText_ = tempLabel_;
      editingTime_ = false;
    } else {
      tempLabel_ = editText_;
      editText_ = tempTime_;
      editingTime_ = true;
    }
    cursorPos_ = editText_.size();
  } else if (key == SDLK_BACKSPACE && cursorPos_ > 0) {
    editText_.erase(cursorPos_ - 1, 1);
    --cursorPos_;
  }
  return true;
}

bool CountdownPanel::onTextInput(const char *text) {
  if (!editing_)
    return false;
  editText_.insert(cursorPos_, text);
  cursorPos_ += static_cast<int>(std::strlen(text));
  return true;
}

void CountdownPanel::renderEditOverlay(SDL_Renderer *renderer) {
  SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
  SDL_SetRenderDrawColor(renderer, 0, 0, 0, 240);
  SDL_Rect overlay = {x_, y_, width_, height_};
  SDL_RenderFillRect(renderer, &overlay);
  SDL_SetRenderDrawColor(renderer, 100, 100, 100, 255);
  SDL_RenderDrawRect(renderer, &overlay);

  SDL_Color cyan = {0, 255, 255, 255};
  SDL_Color white = {255, 255, 255, 255};
  SDL_Color gray = {150, 150, 150, 255};
  SDL_Color activeColor = {0, 200, 0, 255};

  int pad = 10;
  int boxH = 24;
  int startY = y_ + 20;

  // Label Field
  fontMgr_.drawText(renderer, "Label:", x_ + pad, startY - 12, cyan, 9);
  SDL_Rect labelBox = {x_ + pad, startY, width_ - 2 * pad, boxH};
  SDL_SetRenderDrawColor(renderer, 40, 40, 40, 255);
  SDL_RenderFillRect(renderer, &labelBox);
  if (!editingTime_)
    SDL_SetRenderDrawColor(renderer, 0, 200, 0, 255);
  else
    SDL_SetRenderDrawColor(renderer, 100, 100, 100, 255);
  SDL_RenderDrawRect(renderer, &labelBox);

  std::string labelText = !editingTime_ ? editText_ : tempLabel_;
  fontMgr_.drawText(renderer, labelText, x_ + pad + 4, startY + 6, white, 11);

  if (!editingTime_ && (SDL_GetTicks() / 500) % 2 == 0) {
    int tw = fontMgr_.getLogicalWidth(editText_.substr(0, cursorPos_), 11);
    SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
    SDL_RenderDrawLine(renderer, x_ + pad + 4 + tw, startY + 4,
                       x_ + pad + 4 + tw, startY + boxH - 4);
  }

  // Time Field
  int timeY = startY + boxH + 20;
  fontMgr_.drawText(renderer, "Time (YYYY-MM-DD HH:MM):", x_ + pad, timeY - 12,
                    cyan, 9);
  SDL_Rect timeBox = {x_ + pad, timeY, width_ - 2 * pad, boxH};
  SDL_SetRenderDrawColor(renderer, 40, 40, 40, 255);
  SDL_RenderFillRect(renderer, &timeBox);
  if (editingTime_)
    SDL_SetRenderDrawColor(renderer, 0, 200, 0, 255);
  else
    SDL_SetRenderDrawColor(renderer, 100, 100, 100, 255);
  SDL_RenderDrawRect(renderer, &timeBox);

  std::string timeStr = editingTime_ ? editText_ : tempTime_;
  fontMgr_.drawText(renderer, timeStr, x_ + pad + 4, timeY + 6, white, 11);

  if (editingTime_ && (SDL_GetTicks() / 500) % 2 == 0) {
    int tw = fontMgr_.getLogicalWidth(editText_.substr(0, cursorPos_), 11);
    SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
    SDL_RenderDrawLine(renderer, x_ + pad + 4 + tw, timeY + 4,
                       x_ + pad + 4 + tw, timeY + boxH - 4);
  }

  SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_NONE);
}
