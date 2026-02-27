#include "GreylineModal.h"
#include "../core/Theme.h"
#include "FontCatalog.h"
#include <iomanip>
#include <sstream>

namespace HamClock {

GreylineModal::GreylineModal(int x, int y, int w, int h, FontManager &fontMgr)
    : Widget(x, y, w, h), fontMgr_(fontMgr) {
  calculateLayout();
}

void GreylineModal::calculateLayout() {
  int modalW = 340;
  int modalH = 220;
  int modalX = x_ + (width_ - modalW) / 2;
  int modalY = y_ + (height_ - modalH) / 2;
  dialogRect_ = {modalX, modalY, modalW, modalH};

  int btnW = 100;
  int btnH = 32;
  okRect_ = {modalX + (modalW - btnW) / 2, modalY + modalH - btnH - 20, btnW, btnH};
}

void GreylineModal::setWindow(const GreylineWindow &window, const std::string &dxName) {
  window_ = window;
  dxName_ = dxName;
  active_ = true;
}

static std::string formatTime(std::chrono::system_clock::time_point tp) {
  std::time_t t = std::chrono::system_clock::to_time_t(tp);
  std::tm utc{};
  gmtime_r(&t, &utc);
  std::ostringstream ss;
  ss << std::setfill('0') << std::setw(2) << utc.tm_hour << ":"
     << std::setfill('0') << std::setw(2) << utc.tm_min << "Z";
  return ss.str();
}

void GreylineModal::render(SDL_Renderer *renderer) {
  if (!active_) return;

  ThemeColors themes = getThemeColors(theme_);
  auto *cat = fontMgr_.catalog();

  // Background box
  SDL_SetRenderDrawColor(renderer, themes.bg.r, themes.bg.g, themes.bg.b, 255);
  SDL_RenderFillRect(renderer, &dialogRect_);
  SDL_SetRenderDrawColor(renderer, themes.border.r, themes.border.g, themes.border.b, 255);
  SDL_RenderDrawRect(renderer, &dialogRect_);

  // Title
  cat->drawText(renderer, "Mutual Greyline Window", dialogRect_.x + dialogRect_.w / 2,
                dialogRect_.y + 25, themes.accent, FontStyle::SmallBold, true);

  int curY = dialogRect_.y + 60;
  SDL_Color white = {255, 255, 255, 255};

  if (!window_.valid) {
    cat->drawText(renderer, "No mutual overlap found", dialogRect_.x + dialogRect_.w / 2,
                  curY + 20, themes.textDim, FontStyle::Fast, true);
  } else {
    std::string target = "Target: " + (dxName_.empty() ? "Selected DX" : dxName_);
    cat->drawText(renderer, target.c_str(), dialogRect_.x + 20, curY, themes.text, FontStyle::Fast);
    curY += 22;

    std::string timeRange = "Window: " + formatTime(window_.startTime) + " - " + formatTime(window_.endTime);
    cat->drawText(renderer, timeRange.c_str(), dialogRect_.x + 20, curY, white, FontStyle::Fast);
    curY += 22;

    std::ostringstream dur;
    dur << "Duration: " << static_cast<int>(window_.durationMinutes) << " minutes";
    cat->drawText(renderer, dur.str().c_str(), dialogRect_.x + 20, curY, white, FontStyle::Fast);
    curY += 22;

    std::string type = "DE " + window_.deType + " / DX " + window_.dxType;
    cat->drawText(renderer, type.c_str(), dialogRect_.x + 20, curY, themes.accent, FontStyle::Fast);
  }

  // Done Button
  SDL_SetRenderDrawColor(renderer, themes.success.r, themes.success.g, themes.success.b, 255);
  SDL_RenderFillRect(renderer, &okRect_);
  SDL_SetRenderDrawColor(renderer, 255, 255, 255, 150);
  SDL_RenderDrawRect(renderer, &okRect_);
  cat->drawText(renderer, "Done", okRect_.x + okRect_.w / 2, okRect_.y + okRect_.h / 2,
                {255, 255, 255, 255}, FontStyle::UIBold, true, false, true);
}

bool GreylineModal::onMouseUp(int mx, int my, Uint16 mod, int clicks) {
  if (!active_) return false;

  auto pointInRect = [](int x, int y, const SDL_Rect &r) {
    return x >= r.x && x < r.x + r.w && y >= r.y && y < r.y + r.h;
  };

  if (pointInRect(mx, my, okRect_) || !pointInRect(mx, my, dialogRect_)) {
    active_ = false;
    return true;
  }
  return true;
}

bool GreylineModal::onKeyDown(SDL_Keycode key, Uint16 mod) {
  if (!active_) return false;
  if (key == SDLK_ESCAPE || key == SDLK_RETURN || key == SDLK_KP_ENTER) {
    active_ = false;
    return true;
  }
  return true;
}

} // namespace HamClock
