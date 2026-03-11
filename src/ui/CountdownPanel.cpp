#include "CountdownPanel.h"
#include "../core/Astronomy.h"
#include "../core/SoundManager.h"
#include "../core/Theme.h"
#include "FontCatalog.h"
#include "FontManager.h"

#include <cstdio>
#include <ctime>

CountdownPanel::CountdownPanel(int x, int y, int w, int h, FontManager &fontMgr,
                               AppConfig &config, std::function<void()> onSave)
    : Widget(x, y, w, h), fontMgr_(fontMgr), config_(config),
      onSave_(std::move(onSave)) {
  update(); // Initial parse
}

void CountdownPanel::update() {
  if (config_.countdownTime.empty()) {
    targetTime_ = std::chrono::system_clock::time_point(); // Epoch
    alarmTriggered_ = false;
    return;
  }

  struct tm t;
  std::memset(&t, 0, sizeof(t));
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

  ThemeColors themes = getThemeColors(theme_);

  SDL_SetRenderDrawColor(renderer, themes.bg.r, themes.bg.g, themes.bg.b,
                         themes.bg.a);
  SDL_Rect rect = {x_, y_, width_, height_};
  SDL_RenderFillRect(renderer, &rect);
  SDL_SetRenderDrawColor(renderer, themes.border.r, themes.border.g,
                         themes.border.b, themes.border.a);
  SDL_RenderDrawRect(renderer, &rect);

  int titleH = 20;
  auto *cat = fontMgr_.catalog();

  // Unified Title Bar (Match standard font size/look)
  cat->drawText(renderer, "Countdown", x_ + 10, y_ + 5, themes.accent,
                FontStyle::MicroBold);

  // Separator line
  SDL_SetRenderDrawColor(renderer, themes.border.r, themes.border.g,
                         themes.border.b, 60);
  SDL_RenderDrawLine(renderer, x_ + 5, y_ + titleH, x_ + width_ - 5,
                     y_ + titleH);

  int centerX = x_ + width_ / 2;
  int centerY = y_ + titleH + (height_ - titleH) / 2;

  // Custom User Label
  std::string label = config_.countdownLabel;
  if (label.empty() && editing_)
    label = "Click to set";

  if (!label.empty()) {
    cat->drawText(renderer, label, centerX, centerY - 15, themes.text,
                  FontStyle::Micro, true);
  }

  if (targetTime_ == std::chrono::system_clock::time_point()) {
    cat->drawText(renderer, "No target set", centerX, centerY + 5,
                  themes.textDim, FontStyle::SmallRegular, true);
    return;
  }

  auto now = std::chrono::system_clock::now();
  auto diff =
      std::chrono::duration_cast<std::chrono::seconds>(targetTime_ - now)
          .count();

  if (diff <= 0 && targetTime_.time_since_epoch().count() > 0) {
    cat->drawText(renderer, "EVENT ACTIVE!", centerX, centerY + 5,
                  {255, 0, 0, 255}, FontStyle::MicroBold, true);
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

    cat->drawText(renderer, buf, centerX, centerY + 5, themes.text,
                  FontStyle::SmallRegular, true);
  }

  cat->drawText(renderer, "Remaining", centerX, y_ + height_ - 14,
                {100, 100, 100, 255}, FontStyle::Caption, true);

  if (editing_) {
    renderEditOverlay(renderer);
  }

  if (tooltip_.visible) {
    renderTooltip(renderer, fontMgr_);
  }
}

bool CountdownPanel::onMouseUp(int mx, int my, Uint16 mod, int clicks) {
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
          tempTime_ = editInput_.getValue();
          editInput_.setValue(tempLabel_);
          editingTime_ = false;
          editInput_.setCursorToEnd();
        }
        return true;
      }
      // Time field
      int timeY = startY + boxH + 20;
      if (my >= timeY && my < timeY + boxH) {
        if (!editingTime_) {
          // Switch to Time
          tempLabel_ = editInput_.getValue();
          editInput_.setValue(tempTime_);
          editingTime_ = true;
          editInput_.setCursorToEnd();
        }
        return true;
      }
    }

    // Check buttons
    int btnW = 80;
    int btnH = 28;
    int btnY = y_ + height_ - btnH - 10;
    int cx = x_ + width_ / 2;
    if (my >= btnY && my < btnY + btnH) {
      if (mx >= cx - btnW - 10 && mx < cx - 10) {
        stopEditing(false); // Cancel
        return true;
      } else if (mx >= cx + 10 && mx < cx + 10 + btnW) {
        stopEditing(true); // Done
        return true;
      }
    }

    // Click outside -> Ignore or beep? For now, do nothing since we have
    // buttons
    return true;
  }

  // Title bar → let PaneContainer show widget selector
  if (mx >= x_ && mx < x_ + width_ && my >= y_ && my < y_ + 20)
    return false;

  // Click body to open setup
  if (mx >= x_ && mx < x_ + width_ && my >= y_ + 20 && my < y_ + height_) {
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

  editInput_.setValue(tempLabel_);
  editInput_.setActive(true);
  editInput_.setCursorToEnd();
  SDL_StartTextInput();
}

void CountdownPanel::stopEditing(bool apply) {
  if (apply) {
    if (editingTime_)
      tempTime_ = editInput_.getValue();
    else
      tempLabel_ = editInput_.getValue();

    config_.countdownLabel = tempLabel_;
    config_.countdownTime = tempTime_;
    if (onSave_) {
      onSave_();
    }
    update();
  }
  editing_ = false;
  SDL_StopTextInput();
}

bool CountdownPanel::onKeyDown(SDL_Keycode key, Uint16 mod) {
  if (!editing_)
    return false;
  if (key == SDLK_RETURN || key == SDLK_KP_ENTER) {
    stopEditing(true);
    return true;
  }
  if (key == SDLK_ESCAPE) {
    stopEditing(false);
    return true;
  }
  if (key == SDLK_TAB) {
    // Switch fields
    if (editingTime_) {
      tempTime_ = editInput_.getValue();
      editInput_.setValue(tempLabel_);
      editingTime_ = false;
    } else {
      tempLabel_ = editInput_.getValue();
      editInput_.setValue(tempTime_);
      editingTime_ = true;
    }
    editInput_.setCursorToEnd();
    return true;
  }
  editInput_.onKeyDown(key, mod);
  return true;
}

bool CountdownPanel::onTextInput(const char *text) {
  if (!editing_)
    return false;
  editInput_.onTextInput(text);
  return true;
}

void CountdownPanel::renderEditOverlay(SDL_Renderer *renderer) {
  ThemeColors themes = getThemeColors(theme_);
  SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
  SDL_SetRenderDrawColor(renderer, themes.bg.r, themes.bg.g, themes.bg.b, 240);
  SDL_Rect overlay = {x_, y_, width_, height_};
  SDL_RenderFillRect(renderer, &overlay);
  SDL_SetRenderDrawColor(renderer, themes.border.r, themes.border.g, themes.border.b, 255);
  SDL_RenderDrawRect(renderer, &overlay);

  SDL_Color cyan = themes.accent;
  SDL_Color white = themes.text;
  auto *cat = fontMgr_.catalog();

  int pad = 10;
  int boxH = 24;
  int startY = y_ + 20;

  // Label Field
  cat->drawText(renderer, "Label:", x_ + pad, startY - 12, cyan,
                FontStyle::Caption);
  SDL_Rect labelBox = {x_ + pad, startY, width_ - 2 * pad, boxH};
  SDL_SetRenderDrawColor(renderer, themes.rowStripe1.r, themes.rowStripe1.g, themes.rowStripe1.b, themes.rowStripe1.a);
  SDL_RenderFillRect(renderer, &labelBox);
  if (!editingTime_)
    SDL_SetRenderDrawColor(renderer, themes.accent.r, themes.accent.g, themes.accent.b, themes.accent.a);
  else
    SDL_SetRenderDrawColor(renderer, themes.border.r, themes.border.g, themes.border.b, themes.border.a);
  SDL_RenderDrawRect(renderer, &labelBox);

  const std::string &editVal = editInput_.getValue();
  std::string labelText = !editingTime_ ? editVal : tempLabel_;
  cat->drawText(renderer, labelText, x_ + pad + 4, startY + 6, white,
                FontStyle::UI);

  if (!editingTime_ && (SDL_GetTicks() / 500) % 2 == 0) {
    int tw = fontMgr_.getLogicalWidth(editVal.substr(0, editInput_.getCursorPos()),
                                      cat->ptSize(FontStyle::UI));
    SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
    SDL_RenderDrawLine(renderer, x_ + pad + 4 + tw, startY + 4,
                       x_ + pad + 4 + tw, startY + boxH - 4);
  }

  // Time Field
  int timeY = startY + boxH + 20;
  cat->drawText(renderer, "Time (YYYY-MM-DD HH:MM):", x_ + pad, timeY - 12, cyan,
                FontStyle::Caption);
  SDL_Rect timeBox = {x_ + pad, timeY, width_ - 2 * pad, boxH};
  SDL_SetRenderDrawColor(renderer, themes.rowStripe1.r, themes.rowStripe1.g, themes.rowStripe1.b, themes.rowStripe1.a);
  SDL_RenderFillRect(renderer, &timeBox);
  if (editingTime_)
    SDL_SetRenderDrawColor(renderer, themes.accent.r, themes.accent.g, themes.accent.b, themes.accent.a);
  else
    SDL_SetRenderDrawColor(renderer, themes.border.r, themes.border.g, themes.border.b, themes.border.a);
  SDL_RenderDrawRect(renderer, &timeBox);

  std::string timeStr = editingTime_ ? editVal : tempTime_;
  cat->drawText(renderer, timeStr, x_ + pad + 4, timeY + 6, white,
                FontStyle::UI);

  if (editingTime_ && (SDL_GetTicks() / 500) % 2 == 0) {
    int tw = fontMgr_.getLogicalWidth(editVal.substr(0, editInput_.getCursorPos()),
                                      cat->ptSize(FontStyle::UI));
    SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
    SDL_RenderDrawLine(renderer, x_ + pad + 4 + tw, timeY + 4,
                       x_ + pad + 4 + tw, timeY + boxH - 4);
  }

  // Done / Cancel Buttons
  int btnW = 80;
  int btnH = 28;
  int btnY = y_ + height_ - btnH - 10;
  int cx = x_ + width_ / 2;

  // Cancel
  SDL_Rect cancelRect = {cx - btnW - 10, btnY, btnW, btnH};
  SDL_SetRenderDrawColor(renderer, themes.danger.r, themes.danger.g,
                         themes.danger.b, 255);
  SDL_RenderFillRect(renderer, &cancelRect);
  SDL_SetRenderDrawColor(renderer, 255, 255, 255, 100);
  SDL_RenderDrawRect(renderer, &cancelRect);
  cat->drawText(renderer, "Cancel", cancelRect.x + btnW / 2,
                cancelRect.y + btnH / 2, white, FontStyle::UI, true, false,
                true);

  // Done
  SDL_Rect okRect = {cx + 10, btnY, btnW, btnH};
  SDL_SetRenderDrawColor(renderer, themes.success.r, themes.success.g,
                         themes.success.b, 255);
  SDL_RenderFillRect(renderer, &okRect);
  SDL_SetRenderDrawColor(renderer, 255, 255, 255, 100);
  SDL_RenderDrawRect(renderer, &okRect);
  cat->drawText(renderer, "Done", okRect.x + btnW / 2, okRect.y + btnH / 2, white,
                FontStyle::UI, true, false, true);

  SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_NONE);
}

void CountdownPanel::onResize(int x, int y, int w, int h) {
  Widget::onResize(x, y, w, h);
}

void CountdownPanel::onMouseMove(int mx, int my) {
  tooltip_.visible = false;
  if (editing_)
    return;

  // Render text is centered horizontally.
  // Check roughly if mouse is over the countdown
  int textY = y_ + height_ / 2 - 10;
  int textH = 30;

  if (mx >= x_ + 10 && mx < x_ + width_ - 10 && my >= textY &&
      my < textY + textH) {
    auto now = std::chrono::system_clock::now();
    if (targetTime_ > now) {
      tooltip_.text = "Target: " + config_.countdownTime;
      tooltip_.x = mx;
      tooltip_.y = my;
      tooltip_.visible = true;
    }
  }
}

