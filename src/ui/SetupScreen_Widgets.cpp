#include "SetupScreen.h"
#include "WidgetRegistry.h"
#include "../core/Theme.h"
#include <SDL.h>
#include <algorithm>

void SetupScreen::renderTabWidgets(SDL_Renderer *renderer, int cx, int pad,
                                   int fieldW, int fieldH, int fieldX,
                                   int /*textPad*/) {
  auto *cat = fontMgr_.catalog();
  // Reset click targets that are conditionally rendered
  fullHeightCheckRect_ = {0, 0, 0, 0};

  int y =
      (modalRect_.y + cat->ptSize(FontStyle::MediumBold) + 2 * pad + fieldH);
  ThemeColors themes = getThemeColors(theme_, colorOverrides_);

  cat->drawText(renderer, "Select Active Widgets for Each Pane:", fieldX, y,
                themes.text, FontStyle::SmallBold);
  y += cat->ptSize(FontStyle::SmallBold) + pad / 2;

  // --- Miniature pane layout diagram ---
  // Reflects actual layout: Time + Panes 1-4 across top; Side panes 5/6 on the
  // LEFT; Map on the RIGHT. Click a top pane (1-4) to configure its widgets.
  const int diagH    = 56;
  const int topH     = 20;
  const int leftW    = fieldW * 20 / 100;  // left column: Time (top) + Pane 5/6 (bottom)
  const int rightW   = fieldW - leftW - 1; // right area: panes 1-4 (top) + Map (bottom)
  const int botH     = diagH - topH - 2;
  const int topPaneW = rightW / 4;
  const int sidePaneH = botH / 2;

  // Time panel slot (top-left, decorative)
  {
    SDL_Rect timeRect = {fieldX, y, leftW - 1, topH};
    SDL_SetRenderDrawColor(renderer, themes.rowStripe1.r, themes.rowStripe1.g, themes.rowStripe1.b, 255);
    SDL_RenderFillRect(renderer, &timeRect);
    SDL_SetRenderDrawColor(renderer, themes.border.r, themes.border.g, themes.border.b, 255);
    SDL_RenderDrawRect(renderer, &timeRect);
    cat->drawText(renderer, "Time", timeRect.x + timeRect.w / 2, timeRect.y + timeRect.h / 2,
                  themes.textDim, FontStyle::Micro, true, false, true);
  }

  // Side panes 5 and 6 (bottom-left, now clickable)
  {
    SDL_Rect sp5 = {fieldX, y + topH + 1, leftW - 1, sidePaneH};
    SDL_Rect sp6 = {fieldX, y + topH + 2 + sidePaneH, leftW - 1, botH - sidePaneH - 1};
    SDL_Rect sidePanes[2] = {sp5, sp6};
    paneDiagramRects_[4] = sp5;
    paneDiagramRects_[5] = sp6;
    const char *kSideNums[2] = {"5", "6"};
    for (int i = 0; i < 2; ++i) {
      bool active = (activePane_ == (i + 4));
      SDL_SetRenderDrawColor(renderer,
        active ? themes.accent.r : themes.rowStripe1.r,
        active ? themes.accent.g : themes.rowStripe1.g,
        active ? themes.accent.b : themes.rowStripe1.b, 255);
      SDL_RenderFillRect(renderer, &sidePanes[i]);
      SDL_SetRenderDrawColor(renderer, themes.border.r, themes.border.g, themes.border.b, 255);
      SDL_RenderDrawRect(renderer, &sidePanes[i]);
      cat->drawText(renderer, kSideNums[i],
                    sidePanes[i].x + sidePanes[i].w / 2,
                    sidePanes[i].y + sidePanes[i].h / 2,
                    active ? themes.bg : themes.textDim, FontStyle::Micro, true, false, true);
    }
  }

  // Map placeholder (bottom-right, decorative)
  {
    SDL_Rect mapRect = {fieldX + leftW, y + topH + 1, rightW, botH};
    SDL_SetRenderDrawColor(renderer, 10, 30, 60, 255);
    SDL_RenderFillRect(renderer, &mapRect);
    SDL_SetRenderDrawColor(renderer, themes.border.r, themes.border.g, themes.border.b, 255);
    SDL_RenderDrawRect(renderer, &mapRect);
    cat->drawText(renderer, "Map", mapRect.x + mapRect.w / 2, mapRect.y + mapRect.h / 2,
                  themes.textDim, FontStyle::Micro, true, false, true);
  }

  // Top panes 1-4 (clickable, top-right area)
  static const char *kPaneNums[] = {"1", "2", "3", "4"};
  for (int i = 0; i < 4; ++i) {
    bool active = (activePane_ == i);
    SDL_Rect pr = {fieldX + leftW + i * topPaneW, y, topPaneW - 1, topH};
    paneDiagramRects_[i] = pr;
    SDL_SetRenderDrawColor(renderer,
      active ? themes.accent.r : themes.rowStripe1.r,
      active ? themes.accent.g : themes.rowStripe1.g,
      active ? themes.accent.b : themes.rowStripe1.b, 255);
    SDL_RenderFillRect(renderer, &pr);
    SDL_SetRenderDrawColor(renderer, themes.border.r, themes.border.g, themes.border.b, 255);
    SDL_RenderDrawRect(renderer, &pr);
    cat->drawText(renderer, kPaneNums[i], pr.x + pr.w / 2, pr.y + pr.h / 2,
                  active ? themes.bg : themes.textDim, FontStyle::Fast, true, false, true);
  }

  y += diagH + 4;

  // Sync rotation checkbox and Interval
  syncRotationRect_ = {fieldX, y, 16, 16};
  SDL_SetRenderDrawColor(renderer, themes.rowStripe1.r, themes.rowStripe1.g, themes.rowStripe1.b, 255);
  SDL_RenderFillRect(renderer, &syncRotationRect_);
  SDL_SetRenderDrawColor(renderer, themes.border.r, themes.border.g, themes.border.b, 255);
  SDL_RenderDrawRect(renderer, &syncRotationRect_);
  if (syncRotation_) {
    SDL_SetRenderDrawColor(renderer, themes.success.r, themes.success.g, themes.success.b, 255);
    SDL_Rect check = {syncRotationRect_.x + 3, syncRotationRect_.y + 3, 10, 10};
    SDL_RenderFillRect(renderer, &check);
  }
  cat->drawText(renderer, "Sync pane rotation", fieldX + 22, y + 8, themes.textDim,
                FontStyle::Fast, false, false, true);

  cat->drawText(renderer, "Interval (s):", cx + 20, y + 8, themes.textDim,
                FontStyle::Fast, false, false, true);
  rotationToggleRect_ = {cx + 105, y, 40, 18};
  SDL_SetRenderDrawColor(renderer, themes.rowStripe1.r, themes.rowStripe1.g, themes.rowStripe1.b, 255);
  SDL_RenderFillRect(renderer, &rotationToggleRect_);
  SDL_SetRenderDrawColor(
      renderer, (activeTab_ == Tab::Widgets && activeField_ == 0) ? themes.accent.r : themes.border.r,
      (activeTab_ == Tab::Widgets && activeField_ == 0) ? themes.accent.g : themes.border.g,
      (activeTab_ == Tab::Widgets && activeField_ == 0) ? themes.accent.b : themes.border.b, 255);
  SDL_RenderDrawRect(renderer, &rotationToggleRect_);
  cat->drawText(renderer, std::to_string(rotationInterval_),
                rotationToggleRect_.x + rotationToggleRect_.w / 2,
                rotationToggleRect_.y + rotationToggleRect_.h / 2, themes.text,
                FontStyle::Fast, true, false, true);
  y += 20;

  // --- Contest Mode toggle button ---
  {
    const int cmBtnH = 22;
    const int cmBtnW = 120;
    contestModeBtn_ = {fieldX, y, cmBtnW, cmBtnH};
    SDL_Color btnFill = contestModeActive_ ? themes.warning : themes.rowStripe1;
    SDL_SetRenderDrawColor(renderer, btnFill.r, btnFill.g, btnFill.b, 255);
    SDL_RenderFillRect(renderer, &contestModeBtn_);
    SDL_SetRenderDrawColor(renderer, themes.border.r, themes.border.g,
                           themes.border.b, 255);
    SDL_RenderDrawRect(renderer, &contestModeBtn_);
    const char *label = contestModeActive_ ? "Contest Mode: ON" : "Contest Mode";
    cat->drawText(renderer, label,
                  contestModeBtn_.x + contestModeBtn_.w / 2,
                  contestModeBtn_.y + contestModeBtn_.h / 2,
                  contestModeActive_ ? themes.bg : themes.text,
                  FontStyle::Fast, true, false, true);
    y += cmBtnH + 4;
  }

  // --- Widget checklist (scrollable) ---
  widgetRects_.clear();
  int rowH = cat->ptSize(FontStyle::Fast) + 4;

  int footerY = modalRect_.y + modalRect_.h - 52;
  int listEndY = footerY - pad / 2;
  int listAvailH = listEndY - y;

  // Build full type list from registry
  std::vector<std::string> kAllTypesVec;
  for (auto *d : WidgetRegistry::instance().getAll(false))
    kAllTypesVec.push_back(d->typeId);

  // Alphabetize by display name
  std::sort(kAllTypesVec.begin(), kAllTypesVec.end(),
            [](const std::string &a, const std::string &b) {
              auto *da = WidgetRegistry::instance().find(a);
              auto *db = WidgetRegistry::instance().find(b);
              const char *na = da ? da->displayName : a.c_str();
              const char *nb = db ? db->displayName : b.c_str();
              return std::string(na) < std::string(nb);
            });

  // When pane 5 or 6 is active, reserve one row above the list for the Full Height checkbox
  if (activePane_ == 4 || activePane_ == 5)
    listAvailH = std::max(rowH, listAvailH - rowH);

  // Full height checkbox for pane 5 & 6: limits widget selection to scrollable widgets only
  if (activePane_ == 4 || activePane_ == 5) {
    SDL_Rect cb = {fieldX, y, 16, 16};
    SDL_SetRenderDrawColor(renderer, themes.rowStripe1.r, themes.rowStripe1.g,
                           themes.rowStripe1.b, 255);
    SDL_RenderFillRect(renderer, &cb);
    SDL_SetRenderDrawColor(renderer, themes.border.r, themes.border.g,
                           themes.border.b, 255);
    SDL_RenderDrawRect(renderer, &cb);
    if (pane5FullHeight_) {
      SDL_SetRenderDrawColor(renderer, themes.accent.r, themes.accent.g,
                             themes.accent.b, 255);
      SDL_Rect inner = {cb.x + 3, cb.y + 3, 10, 10};
      SDL_RenderFillRect(renderer, &inner);
    }
    cat->drawText(renderer, "Full height (scrollable list widgets only)",
                  cb.x + 22, cb.y + 8,
                  pane5FullHeight_ ? themes.text : themes.textDim,
                  FontStyle::Fast, false, false, true);
    fullHeightCheckRect_ = {fieldX, y, fieldW, rowH};
    y += rowH;
  }

  // When full height active, only show scrollable widgets for both side panes
  std::vector<std::string> filteredTypes;
  if ((activePane_ == 4 || activePane_ == 5) && pane5FullHeight_) {
    for (const auto &t : kAllTypesVec) {
      auto *d = WidgetRegistry::instance().find(t);
      if (d && d->isScrollable)
        filteredTypes.push_back(t);
    }
  } else {
    filteredTypes = kAllTypesVec;
  }
  const int kFilteredWidgets = static_cast<int>(filteredTypes.size());
  const int kColItems = (kFilteredWidgets + 2) / 3;

  int visRows = std::max(1, std::min(kColItems, listAvailH / rowH));
  widgetListMaxScroll_ = std::max(0, kColItems - visRows);
  widgetListScrollOffset_ =
      std::max(0, std::min(widgetListScrollOffset_, widgetListMaxScroll_));

  widgetListStartY_ = y;
  widgetListEndY_ = y + visRows * rowH;

  // Clip widget list to its allotted area
  SDL_Rect prevClip = {0, 0, 0, 0};
  SDL_RenderGetClipRect(renderer, &prevClip);
  SDL_Rect listClip = {modalRect_.x + 2, y, modalRect_.w - 4, visRows * rowH};
  SDL_RenderSetClipRect(renderer, &listClip);

  int colW = fieldW / 3;
  const auto &currentPane = paneRotations_[activePane_];

  for (int col = 0; col < 3; ++col) {
    for (int row = 0; row < visRows; ++row) {
      int idx = col * kColItems + widgetListScrollOffset_ + row;
      if (idx >= kFilteredWidgets)
        break;
      const std::string &t = filteredTypes[idx];
      auto *desc = WidgetRegistry::instance().find(t);
      const char *label = desc ? desc->displayName : t.c_str();

      bool allowed = true;
      if (activePane_ == 3) {
        allowed = (t == "ncdxf" || t == "solar" ||
                   t == "dx_weather" || t == "de_weather" ||
                   t == "band_conditions");
      }

      int drawX = fieldX + col * colW;
      int drawY = y + row * rowH;
      SDL_Rect r = {drawX, drawY, 16, 16};
      SDL_SetRenderDrawColor(renderer, themes.rowStripe1.r, themes.rowStripe1.g, themes.rowStripe1.b, 255);
      SDL_RenderFillRect(renderer, &r);
      SDL_SetRenderDrawColor(renderer, themes.border.r, themes.border.g, themes.border.b, 255);
      SDL_RenderDrawRect(renderer, &r);

      if (allowed) {
        bool selected = std::find(currentPane.begin(), currentPane.end(), t) !=
                        currentPane.end();
        if (selected) {
          SDL_SetRenderDrawColor(renderer, themes.success.r, themes.success.g, themes.success.b, 255);
          SDL_Rect check = {r.x + 3, r.y + 3, 10, 10};
          SDL_RenderFillRect(renderer, &check);
        }
        cat->drawText(renderer, label, r.x + 22, r.y + 8,
                      themes.text, FontStyle::Fast, false, false, true);
        widgetRects_.push_back({t, r});
      } else {
        cat->drawText(renderer, label, r.x + 22, r.y + 8,
                      themes.textDim, FontStyle::Fast, false, false, true);
      }
    }
  }

  // Restore previous clip
  if (prevClip.w > 0 && prevClip.h > 0)
    SDL_RenderSetClipRect(renderer, &prevClip);
  else
    SDL_RenderSetClipRect(renderer, nullptr);

  // Scroll arrows when list overflows
  if (widgetListMaxScroll_ > 0) {
    int arrowX = modalRect_.x + modalRect_.w - 18;
    if (widgetListScrollOffset_ > 0)
      cat->drawText(renderer, "^", arrowX, y + 2, themes.textDim, FontStyle::Fast, true);
    if (widgetListScrollOffset_ < widgetListMaxScroll_)
      cat->drawText(renderer, "v", arrowX, widgetListEndY_ - rowH, themes.textDim,
                    FontStyle::Fast, true);
  }

}
