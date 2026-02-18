#include "ContestPanel.h"
#include "../core/Astronomy.h"
#include "../core/Theme.h"
#include "FontCatalog.h"
#include <chrono>
#include <ctime>

ContestPanel::ContestPanel(int x, int y, int w, int h, FontManager &fontMgr,
                           std::shared_ptr<ContestStore> store)
    : Widget(x, y, w, h), fontMgr_(fontMgr), store_(std::move(store)) {}

void ContestPanel::update() {
  currentData_ = store_->get();
  dataValid_ = currentData_.valid;
}

// Format a time_point as "Feb 09 13:00z"
static std::string formatContestTime(std::chrono::system_clock::time_point tp) {
  std::time_t t = std::chrono::system_clock::to_time_t(tp);
  struct tm buf{};
  struct tm *tm = Astronomy::portable_gmtime(&t, &buf);
  char s[32];
  std::strftime(s, sizeof(s), "%b %d %H:%Mz", tm);
  return s;
}

void ContestPanel::render(SDL_Renderer *renderer) {
  if (!fontMgr_.ready())
    return;

  ThemeColors themes = getThemeColors(theme_);

  // Background and border
  SDL_SetRenderDrawBlendMode(
      renderer, (theme_ == "glass") ? SDL_BLENDMODE_BLEND : SDL_BLENDMODE_NONE);
  SDL_SetRenderDrawColor(renderer, themes.bg.r, themes.bg.g, themes.bg.b,
                         themes.bg.a);
  SDL_Rect rect = {x_, y_, width_, height_};
  SDL_RenderFillRect(renderer, &rect);
  SDL_SetRenderDrawColor(renderer, themes.border.r, themes.border.g,
                         themes.border.b, themes.border.a);
  SDL_RenderDrawRect(renderer, &rect);

  if (!dataValid_) {
    fontMgr_.drawText(renderer, "Awaiting Contests...", x_ + 10,
                      y_ + height_ / 2 - 8, themes.textDim, itemFontSize_);
    if (popupOpen_)
      renderPopup(renderer);
    return;
  }

  auto now = std::chrono::system_clock::now();
  int pad = 6;
  int curY = y_ + pad;

  // Header
  fontMgr_.drawText(renderer, "Contests", x_ + pad, curY, themes.accent,
                    labelFontSize_, true);
  curY += labelFontSize_ + 6;

  // Separator line
  SDL_SetRenderDrawColor(renderer, themes.border.r, themes.border.g,
                         themes.border.b, 60);
  SDL_RenderDrawLine(renderer, x_ + pad, curY - 3, x_ + width_ - pad, curY - 3);

  // Rebuild row-tracking arrays each frame
  displayedIndices_.clear();
  rowRects_.clear();

  // List contests
  int count = 0;
  TTF_Font *font = fontMgr_.getFont(itemFontSize_);
  if (!font) {
    if (popupOpen_)
      renderPopup(renderer);
    return;
  }

  for (int i = 0; i < static_cast<int>(currentData_.contests.size()); ++i) {
    if (count >= 7)
      break;
    const auto &c = currentData_.contests[i];
    if (c.endTime < now)
      continue;

    SDL_Color statusColor = themes.text;
    std::string status = "";

    if (now >= c.startTime && now <= c.endTime) {
      statusColor = {0, 255, 0, 255};
      status = "NOW";
    } else {
      auto diff =
          std::chrono::duration_cast<std::chrono::hours>(c.startTime - now);
      if (diff.count() < 24) {
        status = std::to_string(std::max(1, (int)diff.count())) + "h";
      } else {
        status = std::to_string(diff.count() / 24) + "d";
      }
    }

    int rowH = itemFontSize_ + 3;
    SDL_Rect stripeRect = {x_ + 1, curY - 1, width_ - 2, rowH};
    SDL_SetRenderDrawColor(
        renderer, (count % 2 == 0) ? themes.rowStripe1.r : themes.rowStripe2.r,
        (count % 2 == 0) ? themes.rowStripe1.g : themes.rowStripe2.g,
        (count % 2 == 0) ? themes.rowStripe1.b : themes.rowStripe2.b,
        themes.rowStripe1.a);
    SDL_RenderFillRect(renderer, &stripeRect);

    // Status (right-aligned)
    int sw, sh;
    TTF_SizeText(font, status.c_str(), &sw, &sh);
    fontMgr_.drawText(renderer, status, x_ + width_ - pad - sw, curY,
                      statusColor, itemFontSize_);

    // Title (truncated)
    int maxTitleW = width_ - pad * 2 - sw - 10;
    std::string title = c.title;
    int tw, th;
    TTF_SizeText(font, title.c_str(), &tw, &th);
    if (tw > maxTitleW) {
      while (!title.empty() && tw > maxTitleW - 15) {
        title.pop_back();
        TTF_SizeText(font, (title + "..").c_str(), &tw, &th);
      }
      title += "..";
    }
    fontMgr_.drawText(renderer, title, x_ + pad, curY, themes.text,
                      itemFontSize_);

    // Track for hit-testing
    displayedIndices_.push_back(i);
    rowRects_.push_back({x_, curY - 1, width_, rowH});

    curY += itemFontSize_ + 2;
    count++;
  }

  if (popupOpen_)
    renderPopup(renderer);
}

void ContestPanel::renderPopup(SDL_Renderer *renderer) {
  if (selectedContestIdx_ < 0 ||
      selectedContestIdx_ >= static_cast<int>(currentData_.contests.size()))
    return;

  const Contest &c = currentData_.contests[selectedContestIdx_];
  ThemeColors themes = getThemeColors(theme_);

  // Popup covers the entire widget area
  SDL_Rect bg = {x_, y_, width_, height_};

  // Semi-transparent dark background
  SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
  SDL_SetRenderDrawColor(renderer, 0, 0, 0, 210);
  SDL_RenderFillRect(renderer, &bg);

  // Border
  SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_NONE);
  SDL_SetRenderDrawColor(renderer, themes.accent.r, themes.accent.g,
                         themes.accent.b, themes.accent.a);
  SDL_RenderDrawRect(renderer, &bg);

  int pad = 8;
  int curY = y_ + pad;
  int lineH = itemFontSize_ + 4;

  // Clip to widget
  SDL_RenderSetClipRect(renderer, &bg);

  // Title — wrap if too wide
  {
    TTF_Font *font = fontMgr_.getFont(itemFontSize_);
    std::string title = c.title;
    if (font) {
      int maxW = width_ - pad * 2;
      int tw, th;
      TTF_SizeText(font, title.c_str(), &tw, &th);
      if (tw > maxW) {
        // Split at a space near the midpoint
        size_t mid = title.size() / 2;
        size_t spacePos = title.rfind(' ', mid);
        if (spacePos == std::string::npos)
          spacePos = title.find(' ', mid);
        if (spacePos != std::string::npos) {
          std::string line1 = title.substr(0, spacePos);
          std::string line2 = title.substr(spacePos + 1);
          fontMgr_.drawText(renderer, line1, x_ + pad, curY, themes.accent,
                            itemFontSize_);
          curY += lineH;
          fontMgr_.drawText(renderer, line2, x_ + pad, curY, themes.accent,
                            itemFontSize_);
        } else {
          fontMgr_.drawText(renderer, title, x_ + pad, curY, themes.accent,
                            itemFontSize_);
        }
      } else {
        fontMgr_.drawText(renderer, title, x_ + pad, curY, themes.accent,
                          itemFontSize_);
      }
    }
    curY += lineH + 2;
  }

  // Separator
  SDL_SetRenderDrawColor(renderer, themes.border.r, themes.border.g,
                         themes.border.b, 80);
  SDL_RenderDrawLine(renderer, x_ + pad, curY - 2, x_ + width_ - pad, curY - 2);

  // Status line
  {
    auto now = std::chrono::system_clock::now();
    std::string status;
    SDL_Color statusColor = themes.text;
    if (now >= c.startTime && now <= c.endTime) {
      status = "Status: Running Now";
      statusColor = {0, 255, 0, 255};
    } else if (c.startTime > now) {
      auto diff =
          std::chrono::duration_cast<std::chrono::hours>(c.startTime - now);
      if (diff.count() < 24)
        status = "Starts in " + std::to_string(diff.count()) + "h";
      else
        status = "Starts in " + std::to_string(diff.count() / 24) + "d";
    } else {
      status = "Status: Ended";
      statusColor = themes.textDim;
    }
    fontMgr_.drawText(renderer, status, x_ + pad, curY, statusColor,
                      itemFontSize_);
    curY += lineH;
  }

  // Start / End times
  {
    std::string startStr = "Start: " + formatContestTime(c.startTime);
    std::string endStr = "End:   " + formatContestTime(c.endTime);
    fontMgr_.drawText(renderer, startStr, x_ + pad, curY, themes.text,
                      itemFontSize_);
    curY += lineH;
    fontMgr_.drawText(renderer, endStr, x_ + pad, curY, themes.text,
                      itemFontSize_);
    curY += lineH;
  }

  // Raw date desc if available (compact)
  if (!c.dateDesc.empty()) {
    fontMgr_.drawText(renderer, c.dateDesc, x_ + pad, curY, themes.textDim,
                      std::max(8, itemFontSize_ - 2));
    curY += lineH;
  }

  // URL (truncated to fit)
  if (!c.url.empty()) {
    TTF_Font *font = fontMgr_.getFont(std::max(8, itemFontSize_ - 2));
    std::string url = c.url;
    if (font) {
      int maxW = width_ - pad * 2;
      int tw, th;
      TTF_SizeText(font, url.c_str(), &tw, &th);
      while (!url.empty() && tw > maxW) {
        url.pop_back();
        TTF_SizeText(font, (url + "..").c_str(), &tw, &th);
      }
      if (url.size() < c.url.size())
        url += "..";
    }
    fontMgr_.drawText(renderer, url, x_ + pad, curY, themes.textDim,
                      std::max(8, itemFontSize_ - 2));
    curY += lineH;
  }

  // Dismiss hint at bottom
  fontMgr_.drawText(renderer, "Tap to dismiss",
                    x_ + width_ / 2, y_ + height_ - pad - itemFontSize_,
                    themes.textDim, std::max(8, itemFontSize_ - 2), false, true);

  SDL_RenderSetClipRect(renderer, nullptr);
}

void ContestPanel::onResize(int x, int y, int w, int h) {
  Widget::onResize(x, y, w, h);
  auto *cat = fontMgr_.catalog();
  labelFontSize_ = cat->ptSize(FontStyle::FastBold);
  itemFontSize_ = cat->ptSize(FontStyle::Fast);

  if (h > 150) {
    labelFontSize_ = cat->ptSize(FontStyle::SmallBold);
    itemFontSize_ = cat->ptSize(FontStyle::SmallRegular);
  }
}

bool ContestPanel::onMouseUp(int mx, int my, Uint16 mod) {
  (void)mod;

  // Bounds check
  if (mx < x_ || mx >= x_ + width_ || my < y_ || my >= y_ + height_)
    return false;

  // Any click dismisses the popup
  if (popupOpen_) {
    popupOpen_ = false;
    selectedContestIdx_ = -1;
    return true;
  }

  // Hit-test rows
  for (int i = 0; i < static_cast<int>(rowRects_.size()); ++i) {
    const SDL_Rect &r = rowRects_[i];
    if (mx >= r.x && mx < r.x + r.w && my >= r.y && my < r.y + r.h) {
      selectedContestIdx_ = displayedIndices_[i];
      popupOpen_ = true;
      return true;
    }
  }

  return true;
}

bool ContestPanel::onKeyDown(SDL_Keycode key, Uint16 mod) {
  (void)mod;
  if (popupOpen_ && key == SDLK_ESCAPE) {
    popupOpen_ = false;
    selectedContestIdx_ = -1;
    return true;
  }
  return false;
}
