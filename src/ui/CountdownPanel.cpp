#include "CountdownPanel.h"
#include "../core/Astronomy.h"
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

  if (editing_) {
    renderEditOverlay(renderer);
  }
}

bool CountdownPanel::onMouseUp(int mx, int my, Uint16) {
  if (editing_) {
    stopEditing(true);
    return true;
  }

  if (mx >= x_ && mx < x_ + width_ && my >= y_ && my < y_ + height_) {
    if (my < y_ + height_ / 2) {
      startEditing(false); // Edit Label
    } else {
      startEditing(true); // Edit Time
    }
    return true;
  }
  return false;
}

void CountdownPanel::startEditing(bool editingTime) {
  editing_ = true;
  editingTime_ = editingTime;
  editText_ = editingTime ? config_.countdownTime : config_.countdownLabel;
  if (editingTime && editText_.empty())
    editText_ = "2026-06-27 18:00";
  cursorPos_ = static_cast<int>(editText_.size());
  SDL_StartTextInput();
}

void CountdownPanel::stopEditing(bool apply) {
  if (apply) {
    if (editingTime_)
      config_.countdownTime = editText_;
    else
      config_.countdownLabel = editText_;
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
  else if (key == SDLK_BACKSPACE && cursorPos_ > 0) {
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
  SDL_SetRenderDrawColor(renderer, 0, 0, 0, 220);
  SDL_Rect overlay = {x_ + 2, y_ + 2, width_ - 4, height_ - 4};
  SDL_RenderFillRect(renderer, &overlay);

  SDL_Color cyan = {0, 255, 255, 255};
  fontMgr_.drawText(renderer, editingTime_ ? "Set Time:" : "Set Label:", x_ + 6,
                    y_ + 10, cyan, 10);

  fontMgr_.drawText(renderer, editText_, x_ + 6, y_ + 25, {255, 255, 255, 255},
                    12);

  if ((SDL_GetTicks() / 500) % 2 == 0) {
    int tw = fontMgr_.getLogicalWidth(editText_.substr(0, cursorPos_), 12);
    SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
    SDL_RenderDrawLine(renderer, x_ + 6 + tw, y_ + 25, x_ + 6 + tw, y_ + 37);
  }

  SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_NONE);
}
