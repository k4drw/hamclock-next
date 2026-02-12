#include "ContestPanel.h"
#include "../core/Theme.h"
#include "FontCatalog.h"
#include <chrono>

ContestPanel::ContestPanel(int x, int y, int w, int h, FontManager &fontMgr,
                           std::shared_ptr<ContestStore> store)
    : Widget(x, y, w, h), fontMgr_(fontMgr), store_(std::move(store)) {}

void ContestPanel::update() {
  currentData_ = store_->get();
  dataValid_ = currentData_.valid;
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

  // List contests
  int count = 0;
  TTF_Font *font = fontMgr_.getFont(itemFontSize_);
  if (!font)
    return;

  for (const auto &c : currentData_.contests) {
    if (count >= 7)
      break; // Show up to 7
    if (c.endTime < now)
      continue; // Skip past ones

    SDL_Color statusColor = themes.text;
    std::string status = "";

    if (now >= c.startTime && now <= c.endTime) {
      statusColor = {0, 255, 0, 255}; // Running
      status = "NOW";
    } else {
      // Future
      auto diff =
          std::chrono::duration_cast<std::chrono::hours>(c.startTime - now);
      if (diff.count() < 24) {
        status = std::to_string(std::max(1, (int)diff.count())) + "h";
      } else {
        status = std::to_string(diff.count() / 24) + "d";
      }
    }

    // Row stripe
    SDL_Rect stripeRect = {x_ + 1, curY - 1, width_ - 2, itemFontSize_ + 3};
    SDL_SetRenderDrawColor(
        renderer, (count % 2 == 0) ? themes.rowStripe1.r : themes.rowStripe2.r,
        (count % 2 == 0) ? themes.rowStripe1.g : themes.rowStripe2.g,
        (count % 2 == 0) ? themes.rowStripe1.b : themes.rowStripe2.b,
        themes.rowStripe1.a);
    SDL_RenderFillRect(renderer, &stripeRect);

    // Draw Status (right aligned)
    int sw, sh;
    TTF_SizeText(font, status.c_str(), &sw, &sh);
    fontMgr_.drawText(renderer, status, x_ + width_ - pad - sw, curY,
                      statusColor, itemFontSize_);

    // Draw Title (truncated to avoid overlap with status)
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

    curY += itemFontSize_ + 2;
    count++;
  }
}

void ContestPanel::onResize(int x, int y, int w, int h) {
  Widget::onResize(x, y, w, h);
  auto *cat = fontMgr_.catalog();
  labelFontSize_ = cat->ptSize(FontStyle::FastBold);
  itemFontSize_ = cat->ptSize(FontStyle::Fast);

  // If we have enough vertical space, use larger font
  if (h > 150) {
    labelFontSize_ = cat->ptSize(FontStyle::SmallBold);
    itemFontSize_ = cat->ptSize(FontStyle::SmallRegular);
  }
}
