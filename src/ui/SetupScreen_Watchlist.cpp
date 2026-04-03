#include "SetupScreen.h"
#include "../core/Theme.h"
#include <SDL.h>
#include <algorithm>

void SetupScreen::renderTabWatchlist(SDL_Renderer *renderer, int /*cx*/,
                                     int pad, int fieldW, int fieldH,
                                     int fieldX, int textPad) {
  auto *cat = fontMgr_.catalog();
  int y = modalRect_.y + cat->ptSize(FontStyle::MediumBold) + 2 * pad + fieldH;
  ThemeColors themes = getThemeColors(theme_, colorOverrides_);

  int titleY = y;
  cat->drawText(renderer, "Highlight Callsigns (Watchlist):", fieldX, y, themes.accent,
                FontStyle::SmallBold);
  y += cat->ptSize(FontStyle::SmallBold) + pad / 2;

  // List area: up to 8 visible rows
  const int maxVisible = 5;
  const int rowH = fieldH + 4;
  const int delBtnW = 36;

  watchlistDeleteRects_.clear();
  int visCount = std::min(maxVisible, (int)watchlistEntries_.size() -
                                          watchlistScrollOffset_);
  visCount = std::max(0, visCount);

  for (int i = 0; i < visCount; ++i) {
    int realIdx = watchlistScrollOffset_ + i;
    const std::string &call = watchlistEntries_[realIdx];

    // Row background
    SDL_Rect rowR = {fieldX, y, fieldW, fieldH};
    SDL_SetRenderDrawColor(renderer, themes.rowStripe1.r, themes.rowStripe1.g, themes.rowStripe1.b, 255);
    SDL_RenderFillRect(renderer, &rowR);
    SDL_SetRenderDrawColor(renderer, themes.border.r, themes.border.g, themes.border.b, 255);
    SDL_RenderDrawRect(renderer, &rowR);

    // Callsign text
    cat->drawText(renderer, call.c_str(), fieldX + textPad, y + fieldH / 2,
                  themes.text, FontStyle::SmallRegular, false, false, true);

    // [X] delete button
    SDL_Rect delR = {fieldX + fieldW - delBtnW, y + 2, delBtnW, fieldH - 4};
    SDL_SetRenderDrawColor(renderer, themes.danger.r, themes.danger.g, themes.danger.b, 100);
    SDL_RenderFillRect(renderer, &delR);
    SDL_SetRenderDrawColor(renderer, themes.danger.r, themes.danger.g, themes.danger.b, 200);
    SDL_RenderDrawRect(renderer, &delR);
    cat->drawText(renderer, "X", delR.x + delR.w / 2, delR.y + delR.h / 2, themes.danger,
                  FontStyle::SmallBold, true, false, true);
    watchlistDeleteRects_.push_back(delR);

    y += rowH;
  }

  // Scroll arrows (shown when list is longer than maxVisible)
  watchlistScrollUpRect_ = {0, 0, 0, 0};
  watchlistScrollDownRect_ = {0, 0, 0, 0};
  if ((int)watchlistEntries_.size() > maxVisible) {
    int arrowW = 30;
    int arrowH = fieldH - 4;
    int arrowY = titleY; // align with "Highlight Callsigns" title
    
    // Up arrow
    if (watchlistScrollOffset_ > 0) {
      SDL_Rect upR = {fieldX + fieldW - 2 * arrowW - 10, arrowY, arrowW, arrowH};
      watchlistScrollUpRect_ = upR;
      SDL_SetRenderDrawColor(renderer, themes.rowStripe2.r, themes.rowStripe2.g, themes.rowStripe2.b, 255);
      SDL_RenderFillRect(renderer, &upR);
      SDL_SetRenderDrawColor(renderer, themes.border.r, themes.border.g, themes.border.b, 255);
      SDL_RenderDrawRect(renderer, &upR);
      cat->drawText(renderer, "^", upR.x + upR.w / 2, upR.y + upR.h / 2,
                    themes.text, FontStyle::Fast, true, false, true);
    }
    // Down arrow
    if (watchlistScrollOffset_ + maxVisible < (int)watchlistEntries_.size()) {
      SDL_Rect downR = {fieldX + fieldW - arrowW, arrowY, arrowW, arrowH};
      watchlistScrollDownRect_ = downR;
      SDL_SetRenderDrawColor(renderer, themes.rowStripe2.r, themes.rowStripe2.g, themes.rowStripe2.b, 255);
      SDL_RenderFillRect(renderer, &downR);
      SDL_SetRenderDrawColor(renderer, themes.border.r, themes.border.g, themes.border.b, 255);
      SDL_RenderDrawRect(renderer, &downR);
      cat->drawText(renderer, "v", downR.x + downR.w / 2, downR.y + downR.h / 2,
                    themes.text, FontStyle::Fast, true, false, true);
    }
    // Note: No y increment here, arrows sit on the title line
  }

  y += pad / 2;

  // On The Air filter (ALL / POTA / SOTA)
  cat->drawText(renderer, "On The Air Filter:", fieldX, y, themes.text,
                FontStyle::SmallBold);
  y += cat->ptSize(FontStyle::SmallBold) + 4;

  static const char *kFilterLabels[] = {"All", "POTA", "SOTA"};
  static const char *kFilterValues[] = {"all", "pota", "sota"};
  const int filterBtnW = 56;
  const int filterBtnGap = 6;
  for (int i = 0; i < 3; ++i) {
    SDL_Rect r = {fieldX + i * (filterBtnW + filterBtnGap), y, filterBtnW, fieldH};
    ontaFilterRects_[i] = r;
    bool active = (ontaFilter_ == kFilterValues[i]);
    SDL_Color fill = active ? themes.accent : themes.rowStripe1;
    SDL_Color textCol = active ? themes.bg : themes.text;
    SDL_SetRenderDrawColor(renderer, fill.r, fill.g, fill.b, 255);
    SDL_RenderFillRect(renderer, &r);
    SDL_SetRenderDrawColor(renderer, themes.border.r, themes.border.g,
                           themes.border.b, 255);
    SDL_RenderDrawRect(renderer, &r);
    cat->drawText(renderer, kFilterLabels[i], r.x + r.w / 2, r.y + r.h / 2,
                  textCol, FontStyle::SmallBold, true, false, true);
  }
  y += fieldH + pad / 2;

  // Count hint
  char hint[48];
  std::snprintf(hint, sizeof(hint), "%d callsign(s)",
                (int)watchlistEntries_.size());
  cat->drawText(renderer, hint, fieldX, y, themes.textDim, FontStyle::Fast);
  y += cat->ptSize(FontStyle::Fast) + pad / 2;

  // Input field + Add button
  const int addBtnW = 60;
  int inputW = fieldW - addBtnW - pad / 2;

  cat->drawText(renderer, "Add:", fieldX, y, themes.text, FontStyle::SmallBold);
  y += cat->ptSize(FontStyle::SmallBold) + 4;

  watchlistInputRect_ = {fieldX, y, inputW, fieldH};
  watchlistAddRect_ = {fieldX + inputW + pad / 2, y, addBtnW, fieldH};

  watchlistInputField_.render(
      renderer, fontMgr_, fieldX, y, inputW, fieldH, FontStyle::SmallRegular,
      textPad, activeTab_ == Tab::Watchlist && activeField_ == 0, false, themes.accent,
      themes.textDim, themes.text, themes.text, themes.textDim, "Enter callsign...", &themes.rowStripe1);
  y += fieldH;

  SDL_SetRenderDrawColor(renderer, themes.success.r, themes.success.g, themes.success.b, 100);
  SDL_RenderFillRect(renderer, &watchlistAddRect_);
  SDL_SetRenderDrawColor(renderer, themes.success.r, themes.success.g, themes.success.b, 200);
  SDL_RenderDrawRect(renderer, &watchlistAddRect_);
  cat->drawText(renderer, "Add", watchlistAddRect_.x + watchlistAddRect_.w / 2,
                watchlistAddRect_.y + watchlistAddRect_.h / 2, themes.text,
                FontStyle::SmallBold, true, false, true);
}
