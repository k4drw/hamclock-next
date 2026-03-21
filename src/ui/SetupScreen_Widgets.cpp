#include "SetupScreen.h"
#include "../core/Theme.h"
#include <SDL.h>
#include <algorithm>

void SetupScreen::renderTabWidgets(SDL_Renderer *renderer, int cx, int pad,
                                   int fieldW, int fieldH, int fieldX,
                                   int /*textPad*/) {
  auto *cat = fontMgr_.catalog();
  // Clamp activePane_ to top-bar panes only (0-3)
  if (activePane_ > 3)
    activePane_ = 0;

  int y =
      (modalRect_.y + cat->ptSize(FontStyle::MediumBold) + 2 * pad + fieldH);
  ThemeColors themes = getThemeColors(theme_, colorOverrides_);

  cat->drawText(renderer, "Select Active Widgets for Each Pane:", fieldX, y,
                themes.text, FontStyle::SmallBold);
  y += cat->ptSize(FontStyle::SmallBold) + pad / 2;

  // 4 top-bar pane buttons in one compact row
  const int btnH = 22;
  const int btnGap = 4;
  int paneW = fieldW / 4;
  static const char *kTopLabels[] = {"Top 1", "Top 2", "Top 3", "Top 4"};
  for (int i = 0; i < 4; ++i) {
    SDL_Rect pr = {fieldX + i * paneW, y, paneW - btnGap, btnH};
    bool active = activePane_ == i;
    if (active) {
      SDL_SetRenderDrawColor(renderer, themes.accent.r, themes.accent.g, themes.accent.b, 255);
    } else {
      SDL_SetRenderDrawColor(renderer, themes.rowStripe1.r, themes.rowStripe1.g, themes.rowStripe1.b, 255);
    }
    SDL_RenderFillRect(renderer, &pr);
    SDL_SetRenderDrawColor(renderer, themes.border.r, themes.border.g, themes.border.b, 255);
    SDL_RenderDrawRect(renderer, &pr);
    cat->drawText(renderer, kTopLabels[i], pr.x + pr.w / 2, pr.y + pr.h / 2,
                  active ? themes.bg : themes.textDim, FontStyle::Fast, true, false, true);
  }
  y += btnH + btnGap;

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

  // Side panel section height: header + 4 radio options
  int sideSecH = cat->ptSize(FontStyle::SmallBold) + 4 + 4 * (rowH + 2) + 4;
  int footerY = modalRect_.y + modalRect_.h - 52;
  int sideSecY = footerY - sideSecH;
  int listEndY = sideSecY - pad / 2;
  int listAvailH = listEndY - y;

  auto kAllTypesVec = getAllBaseWidgetTypes();
  const int kNWidgets = static_cast<int>(kAllTypesVec.size());
  const int kColItems = (kNWidgets + 2) / 3;

  int visRows = std::max(1, listAvailH / rowH);
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
      if (idx >= kNWidgets)
        break;
      WidgetType t = kAllTypesVec[idx];

      bool allowed = true;
      if (activePane_ == 3) {
        allowed = (t == WidgetType::NCDXF || t == WidgetType::SOLAR ||
                   t == WidgetType::DX_WEATHER || t == WidgetType::DE_WEATHER ||
                   t == WidgetType::BAND_CONDITIONS);
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
        cat->drawText(renderer, widgetTypeDisplayName(t), r.x + 22, r.y + 8,
                      themes.text, FontStyle::Fast, false, false, true);
        widgetRects_.push_back({t, r});
      } else {
        cat->drawText(renderer, widgetTypeDisplayName(t), r.x + 22, r.y + 8,
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

  // --- Side Panel section ---
  y = sideSecY;
  cat->drawText(renderer, "--- Side Panel ---", cx, y, themes.accent,
                FontStyle::SmallBold, true);
  y += cat->ptSize(FontStyle::SmallBold) + 4;

  // Determine current side-panel mode from paneRotations_[4/5]
  int curMode = 0;
  if (!paneRotations_[4].empty()) {
    WidgetType t5 = paneRotations_[4][0];
    if (t5 == WidgetType::DX_CLUSTER)
      curMode = 1;
    else if (t5 == WidgetType::ON_THE_AIR)
      curMode = 2;
    else if (t5 == WidgetType::LIVE_SPOTS)
      curMode = 3;
    // else curMode = 0 (DE_INFO + DX_INFO or default)
  }

  static const char *kSideLabels[] = {
      "DE Info + DX/Sat (2 panes, original)",
      "DX Cluster (full height)",
      "On The Air (full height)",
      "Live Spots (full height)",
  };
  for (int i = 0; i < 4; ++i) {
    SDL_Rect rr = {fieldX, y, 14, 14};
    sidePanelModeRects_[i] = {fieldX, y, fieldW, rowH};
    SDL_SetRenderDrawColor(renderer, themes.rowStripe1.r, themes.rowStripe1.g, themes.rowStripe1.b, 255);
    SDL_RenderFillRect(renderer, &rr);
    SDL_SetRenderDrawColor(renderer, themes.border.r, themes.border.g, themes.border.b, 255);
    SDL_RenderDrawRect(renderer, &rr);
    if (curMode == i) {
      SDL_SetRenderDrawColor(renderer, themes.accent.r, themes.accent.g, themes.accent.b, 255);
      SDL_Rect inner = {rr.x + 3, rr.y + 3, 8, 8};
      SDL_RenderFillRect(renderer, &inner);
    }
    cat->drawText(renderer, kSideLabels[i], rr.x + 22, rr.y + 7,
                  curMode == i ? themes.text : themes.textDim, FontStyle::Fast, false, false,
                  true);
    y += rowH + 2;
  }
}
