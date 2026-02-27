#include "UpdateOverlay.h"
#include "../core/Theme.h"
#include "FontCatalog.h"
#include <algorithm>

UpdateOverlay::UpdateOverlay(int x, int y, int w, int h, FontManager &fontMgr,
                             UpdateChecker &updateChecker)
    : Widget(x, y, w, h), fontMgr_(fontMgr), updateChecker_(updateChecker) {
  recalcLayout();
}

UpdateOverlay::~UpdateOverlay() {}

void UpdateOverlay::update() {
  // Check if content needs re-wrapping
  if (lastNotes_ != updateChecker_.releaseNotes() ||
      lastWidth_ != notesArea_.w) {
    lastNotes_ = updateChecker_.releaseNotes();
    lastWidth_ = notesArea_.w;

    wrappedLines_.clear();
    if (lastNotes_.empty())
      return;

    auto *cat = fontMgr_.catalog();
    int bodyPt = cat->ptSize(FontStyle::SmallRegular);
    int titlePt = cat->ptSize(FontStyle::MediumBold);

    // Simple line-based parser for markdown-lite
    std::string line;
    std::stringstream ss(lastNotes_);
    while (std::getline(ss, line)) {
      if (line.empty()) {
        wrappedLines_.push_back({"", bodyPt, {0, 0, 0, 0}, false, 0});
        continue;
      }

      int size = bodyPt;
      SDL_Color col = {220, 220, 220, 255};
      bool bold = false;
      int indent = 0;

      if (line.find("###") == 0) {
        size = titlePt;
        col = {0, 200, 255, 255};
        bold = true;
        line = line.substr(3);
      } else if (line.find("-") == 0 || line.find("*") == 0) {
        indent = 15;
        line = "• " + line.substr(1);
      }

      // Very basic wrapping
      wrappedLines_.push_back({line, size, col, bold, indent});
    }

    // Calculate max scroll
    int totalH = 0;
    for (const auto &wl : wrappedLines_) {
      int lh = static_cast<int>(wl.size * 1.8f);
      totalH += lh;
    }
    maxScroll_ = std::max(0, totalH - notesArea_.h);
    scrollPos_ = 0;
  }
}

void UpdateOverlay::recalcLayout() {
  auto *cat = fontMgr_.catalog();
  int titlePt = cat ? cat->ptSize(FontStyle::MediumBold) : 24;
  int bodyPt = cat ? cat->ptSize(FontStyle::SmallRegular) : 14;
  int btnPt = cat ? cat->ptSize(FontStyle::UI) : 16;

  padding_ = 12; // Aggressive reduction to maximize text space

  int btnW = std::min(120, (width_ - 4 * padding_) / 3);
  int btnH = btnPt + 16;
  int headerH = titlePt + bodyPt + padding_ * 3; // More room for version line
  int footerH = btnH + padding_ * 2;

  notesArea_ = {x_ + padding_, y_ + headerH, width_ - 2 * padding_,
                height_ - headerH - footerH};

  int btnY = y_ + height_ - padding_ - btnH;
  skipBtn_ = {x_ + padding_, btnY, btnW, btnH};
  notNowBtn_ = {x_ + width_ / 2 - btnW / 2, btnY, btnW, btnH};
  updateBtn_ = {x_ + width_ - padding_ - btnW, btnY, btnW, btnH};
}

void UpdateOverlay::render(SDL_Renderer *renderer) {
  ThemeColors themes = getThemeColors(theme_);

  // Semi-transparent dimming of backbuffer
  SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
  SDL_SetRenderDrawColor(renderer, 0, 0, 0, 180);
  SDL_Rect screen = {0, 0, 2048, 2048}; // Sufficiently large
  SDL_RenderFillRect(renderer, &screen);

  // Modal background
  SDL_SetRenderDrawColor(renderer, themes.bg.r, themes.bg.g, themes.bg.b, 255);
  SDL_Rect modal = {x_, y_, width_, height_};
  SDL_RenderFillRect(renderer, &modal);
  SDL_SetRenderDrawColor(renderer, themes.border.r, themes.border.g,
                         themes.border.b, 255);
  SDL_RenderDrawRect(renderer, &modal);

  SDL_Color white = themes.text;
  SDL_Color cyan = themes.accent;
  auto *cat = fontMgr_.catalog();

  // Header
  int ty = y_ + padding_;
  cat->drawText(renderer, "Updates Available", x_ + width_ / 2, ty, cyan,
                FontStyle::MediumBold, true);
  ty += cat->ptSize(FontStyle::MediumBold) + 4;

  std::string info =
      std::string(HAMCLOCK_VERSION) + " -> " + updateChecker_.latestVersion();
  info +=
      " (" + updateChecker_.arch() + " " + updateChecker_.installType() + ")";
  cat->drawText(renderer, info, x_ + width_ / 2, ty, white,
                FontStyle::SmallRegular, true);

  // Release Notes Area
  SDL_RenderSetClipRect(renderer, &notesArea_);
  int ly = notesArea_.y - scrollPos_;
  int bodyPt = cat->ptSize(FontStyle::SmallRegular);
  for (const auto &wl : wrappedLines_) {
    int lh = static_cast<int>(wl.size * 1.8f);
    if (ly + wl.size > notesArea_.y && ly < notesArea_.y + notesArea_.h) {
      // Use dynamic ptSize mapping logic similar to MapWidget
      FontStyle style = FontStyle::SmallRegular;
      if (wl.size > bodyPt)
        style = wl.bold ? FontStyle::MediumBold : FontStyle::MediumRegular;
      else if (wl.bold)
        style = FontStyle::SmallBold;

      cat->drawText(renderer, wl.text, notesArea_.x + wl.indent, ly, wl.color,
                    style);
    }
    ly += lh;
  }
  SDL_RenderSetClipRect(renderer, nullptr);

  // Scrollbar if needed
  if (maxScroll_ > 0) {
    int sbW = 6;
    int sbX = notesArea_.x + notesArea_.w - sbW;
    SDL_Rect track = {sbX, notesArea_.y, sbW, notesArea_.h};
    SDL_SetRenderDrawColor(renderer, 40, 45, 55, 255);
    SDL_RenderFillRect(renderer, &track);

    float visiblePct =
        std::min(1.0f, (float)notesArea_.h / (maxScroll_ + notesArea_.h));
    int thumbH = std::max(20, (int)(notesArea_.h * visiblePct));
    int thumbY = notesArea_.y + (int)((float)scrollPos_ / maxScroll_ *
                                      (notesArea_.h - thumbH));
    SDL_Rect thumb = {sbX, thumbY, sbW, thumbH};
    SDL_SetRenderDrawColor(renderer, 100, 110, 130, 255);
    SDL_RenderFillRect(renderer, &thumb);
  }

  // Buttons
  auto drawBtn = [&](SDL_Rect r, const char *label, SDL_Color bg) {
    SDL_SetRenderDrawColor(renderer, bg.r, bg.g, bg.b, 255);
    SDL_RenderFillRect(renderer, &r);
    SDL_SetRenderDrawColor(renderer, 255, 255, 255, 100);
    SDL_RenderDrawRect(renderer, &r);
    cat->drawText(renderer, label, r.x + r.w / 2, r.y + r.h / 2, white,
                  FontStyle::UI, true, false, true);
  };

  drawBtn(skipBtn_, "Skip Ver", themes.danger);
  drawBtn(notNowBtn_, "Not Now", themes.info);
  drawBtn(updateBtn_, "Update", themes.success);
}

void UpdateOverlay::onResize(int x, int y, int w, int h) {
  Widget::onResize(x, y, w, h);
  recalcLayout();
}

bool UpdateOverlay::onMouseUp(int mx, int my, Uint16, int) {
  if (mx >= skipBtn_.x && mx < skipBtn_.x + skipBtn_.w && my >= skipBtn_.y &&
      my < skipBtn_.y + skipBtn_.h) {
    result_ = Result::Skip;
  } else if (mx >= notNowBtn_.x && mx < notNowBtn_.x + notNowBtn_.w &&
             my >= notNowBtn_.y && my < notNowBtn_.y + notNowBtn_.h) {
    result_ = Result::NotNow;
  } else if (mx >= updateBtn_.x && mx < updateBtn_.x + updateBtn_.w &&
             my >= updateBtn_.y && my < updateBtn_.y + updateBtn_.h) {
    result_ = Result::Update;
  }

  return true; // Modal consumes all clicks
}

bool UpdateOverlay::onMouseWheel(int scrollY) {
  if (maxScroll_ > 0) {
    scrollPos_ = std::clamp(scrollPos_ - scrollY * 30, 0, maxScroll_);
    return true;
  }
  return false;
}
