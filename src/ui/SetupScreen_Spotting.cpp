#include "SetupScreen.h"
#include "../core/Theme.h"
#include <SDL.h>
#include <algorithm>

void SetupScreen::renderTabDXCluster(SDL_Renderer *renderer, int cx, int pad,
                                     int fieldW, int fieldH, int fieldX,
                                     int textPad) {
  auto *cat = fontMgr_.catalog();
  int y =
      (modalRect_.y + cat->ptSize(FontStyle::MediumBold) + 2 * pad + fieldH);
  int vSpace = 5;
  ThemeColors themes = getThemeColors(theme_, colorOverrides_);

  // --- DX CLUSTER SECTION ---
  cat->drawText(renderer, "--- DX Cluster ---", cx, y, themes.accent,
                FontStyle::SmallBold, true);
  y += cat->ptSize(FontStyle::SmallBold) + vSpace;

  cat->drawText(renderer, "Host:", fieldX, y, themes.text, FontStyle::SmallBold);
  cat->drawText(renderer, "Port:", fieldX + fieldW / 2 + pad, y, themes.text,
                FontStyle::SmallBold);
  y += cat->ptSize(FontStyle::SmallBold) + 4;

  int halfW = (fieldW - pad) / 2;
  int hostY = y;
  clusterHostInput_.render(renderer, fontMgr_, fieldX, hostY, halfW, fieldH,
                           FontStyle::SmallRegular, textPad, activeField_ == 0,
                           !clusterHostInput_.getValue().empty(), themes.accent, themes.textDim,
                           themes.text, themes.text, themes.textDim, "dxusa.net", &themes.rowStripe1);
  clusterHostRect_ = {fieldX, hostY, halfW, fieldH};
  int portY = y;
  clusterPortInput_.render(renderer, fontMgr_, fieldX + halfW + pad, portY,
                           halfW, fieldH, FontStyle::SmallRegular, textPad,
                           activeField_ == 1,
                           !clusterPortInput_.getValue().empty(), themes.accent, themes.textDim,
                           themes.text, themes.text, themes.textDim, "7300", &themes.rowStripe1);
  clusterPortRect_ = {fieldX + halfW + pad, portY, halfW, fieldH};
  y += fieldH + vSpace;

  cat->drawText(renderer, "Login:", fieldX, y, themes.text, FontStyle::SmallBold);
  y += cat->ptSize(FontStyle::SmallBold) + 4;
  clusterLoginInput_.render(renderer, fontMgr_, fieldX, y, fieldW, fieldH,
                            FontStyle::SmallRegular, textPad, activeField_ == 2,
                            !clusterLoginInput_.getValue().empty(), themes.accent,
                            themes.textDim, themes.text, themes.text, themes.textDim, "NOCALL", &themes.rowStripe1);
  clusterLoginRect_ = {fieldX, y, fieldW, fieldH};
  y += fieldH + vSpace;

  // Toggles row 1
  int clusterLabelW = fontMgr_.getLogicalWidth("Enable DX Cluster", cat->ptSize(FontStyle::SmallRegular));
  clusterToggleRect_ = {fieldX, y, 30 + clusterLabelW, 20};
  SDL_Rect clusterBox = {fieldX, y, 20, 20};
  SDL_SetRenderDrawColor(renderer, themes.rowStripe1.r, themes.rowStripe1.g, themes.rowStripe1.b, 255);
  SDL_RenderFillRect(renderer, &clusterBox);
  SDL_SetRenderDrawColor(renderer, themes.border.r, themes.border.g, themes.border.b, 255);
  SDL_RenderDrawRect(renderer, &clusterBox);
  if (clusterEnabled_) {
    SDL_SetRenderDrawColor(renderer, themes.success.r, themes.success.g, themes.success.b, 255);
    SDL_Rect check = {fieldX + 4, y + 4, 12, 12};
    SDL_RenderFillRect(renderer, &check);
  }
  cat->drawText(renderer, "Enable DX Cluster", fieldX + 30, y + 10, themes.text,
                FontStyle::SmallRegular, false, false, true);

  y += 24;

  int wsjtxLabelW = fontMgr_.getLogicalWidth("Use WSJT-X (UDP)", cat->ptSize(FontStyle::SmallRegular));
  toggleRect_ = {fieldX, y, 30 + wsjtxLabelW, 20};
  SDL_Rect wsjtxBox = {fieldX, y, 20, 20};
  SDL_SetRenderDrawColor(renderer, themes.rowStripe1.r, themes.rowStripe1.g, themes.rowStripe1.b, 255);
  SDL_RenderFillRect(renderer, &wsjtxBox);
  SDL_SetRenderDrawColor(renderer, themes.border.r, themes.border.g, themes.border.b, 255);
  SDL_RenderDrawRect(renderer, &wsjtxBox);
  if (clusterWSJTX_) {
    SDL_SetRenderDrawColor(renderer, themes.success.r, themes.success.g, themes.success.b, 255);
    SDL_Rect check = {fieldX + 4, y + 4, 12, 12};
    SDL_RenderFillRect(renderer, &check);
  }
  cat->drawText(renderer, "Use WSJT-X (UDP)", fieldX + 30, y + 10, themes.text,
                FontStyle::SmallRegular, false, false, true);
  if (clusterWSJTX_) {
    int halfW = (fieldW - pad) / 2;
    int wPortW = std::min(120, halfW);
    int portX = fieldX + halfW + pad;
    wsjtxPortRect_ = {portX, y, wPortW, fieldH};
    wsjtxPortInput_.render(renderer, fontMgr_, portX, y, wPortW, fieldH,
                           FontStyle::SmallRegular, textPad, activeField_ == 3,
                           !wsjtxPortInput_.getValue().empty(), themes.accent, themes.textDim,
                           themes.text, themes.text, themes.textDim, "2237", &themes.rowStripe1);
  } else {
    wsjtxPortRect_ = {0, 0, 0, 0};
  }
  y += 24;

  int dupeLabelW = fontMgr_.getLogicalWidth("Hide duplicates (one per call/band)", cat->ptSize(FontStyle::SmallRegular));
  clusterHideDuplicatesRect_ = {fieldX, y, 30 + dupeLabelW, 20};
  SDL_Rect dupeBox = {fieldX, y, 20, 20};
  SDL_SetRenderDrawColor(renderer, themes.rowStripe1.r, themes.rowStripe1.g, themes.rowStripe1.b, 255);
  SDL_RenderFillRect(renderer, &dupeBox);
  SDL_SetRenderDrawColor(renderer, themes.border.r, themes.border.g, themes.border.b, 255);
  SDL_RenderDrawRect(renderer, &dupeBox);
  if (clusterHideDuplicates_) {
    SDL_SetRenderDrawColor(renderer, themes.success.r, themes.success.g, themes.success.b, 255);
    SDL_Rect check = {fieldX + 4, y + 4, 12, 12};
    SDL_RenderFillRect(renderer, &check);
  }
  cat->drawText(renderer, "Hide duplicates (one per call/band)", fieldX + 30, y + 10, themes.text,
                FontStyle::SmallRegular, false, false, true);
  y += 30;

  // --- RBN SECTION ---
  cat->drawText(renderer, "--- Reverse Beacon Network ---", cx, y, themes.accent,
                FontStyle::SmallBold, true);
  y += cat->ptSize(FontStyle::SmallBold) + vSpace;

  int rbnLabelW = fontMgr_.getLogicalWidth("Enable RBN (feeds DX Cluster panel)", cat->ptSize(FontStyle::SmallRegular));
  rbnToggleRect_ = {fieldX, y, 30 + rbnLabelW, 20};
  SDL_Rect rbnBox = {fieldX, y, 20, 20};
  SDL_SetRenderDrawColor(renderer, themes.rowStripe1.r, themes.rowStripe1.g, themes.rowStripe1.b, 255);
  SDL_RenderFillRect(renderer, &rbnBox);
  SDL_SetRenderDrawColor(renderer, themes.border.r, themes.border.g, themes.border.b, 255);
  SDL_RenderDrawRect(renderer, &rbnBox);
  if (rbnEnabled_) {
    SDL_SetRenderDrawColor(renderer, themes.success.r, themes.success.g, themes.success.b, 255);
    SDL_Rect check = {fieldX + 4, y + 4, 12, 12};
    SDL_RenderFillRect(renderer, &check);
  }
  cat->drawText(renderer, "Enable RBN (feeds DX Cluster panel)", fieldX + 30,
                y + 10, themes.text, FontStyle::SmallRegular, false, false, true);
  y += 24;
}
