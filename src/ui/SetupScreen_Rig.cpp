#include "SetupScreen.h"
#include "../core/Theme.h"
#include <SDL.h>

void SetupScreen::renderTabRig(SDL_Renderer *renderer, int cx, int pad,
                               int fieldW, int fieldH, int fieldX,
                               int textPad) {
  auto *cat = fontMgr_.catalog();
  int yBase =
      (modalRect_.y + cat->ptSize(FontStyle::MediumBold) + 2 * pad + fieldH);
  int y = yBase;
  int vSpace = pad / 2;
  int colW = fieldW / 2 - pad;
  int rightX = cx + pad / 2;

  ThemeColors themes = getThemeColors(theme_, colorOverrides_);

  // --- Rig Section (Left Column) ---
  cat->drawText(renderer, "Rig / CAT Control:", fieldX, y, themes.text,
                FontStyle::SmallBold);
  y += cat->ptSize(FontStyle::SmallBold) + vSpace;

  int rigLabelH = cat->ptSize(FontStyle::SmallRegular) + 4;
  cat->drawText(renderer, "rigctld Host:", fieldX, y, themes.text,
                FontStyle::SmallRegular);
  rigHostRect_ = {fieldX, y, colW, rigLabelH + fieldH};
  y += rigLabelH;
  rigHostInput_.render(renderer, fontMgr_, fieldX, y, colW, fieldH,
                       FontStyle::SmallRegular, textPad,
                       (activeTab_ == Tab::Rig && activeField_ == 0), true,
                       themes.accent, themes.textDim, themes.text, themes.text, themes.textDim, "localhost", &themes.rowStripe1);
  y += fieldH + vSpace;

  cat->drawText(renderer, "rigctld Port:", fieldX, y, themes.text,
                FontStyle::SmallRegular);
  rigPortRect_ = {fieldX, y, colW, rigLabelH + fieldH};
  y += rigLabelH;
  rigPortInput_.render(renderer, fontMgr_, fieldX, y, colW, fieldH,
                       FontStyle::SmallRegular, textPad,
                       (activeTab_ == Tab::Rig && activeField_ == 1), true,
                       themes.accent, themes.textDim, themes.text, themes.text, themes.textDim, "4532", &themes.rowStripe1);
  y += fieldH + vSpace;

  // Auto-tune Toggle
  {
    SDL_Rect r = {fieldX, y, 20, 20};
    toggleRect_ = r;
    SDL_SetRenderDrawColor(renderer, themes.rowStripe1.r, themes.rowStripe1.g, themes.rowStripe1.b, 255);
    SDL_RenderFillRect(renderer, &r);
    SDL_SetRenderDrawColor(renderer, themes.border.r, themes.border.g, themes.border.b, 255);
    SDL_RenderDrawRect(renderer, &r);
    if (rigAutoTune_) {
      SDL_SetRenderDrawColor(renderer, themes.success.r, themes.success.g, themes.success.b, 255);
      SDL_Rect check = {r.x + 4, r.y + 4, 12, 12};
      SDL_RenderFillRect(renderer, &check);
    }
    cat->drawText(renderer, "Auto-Tune on Spot click", fieldX + 30, y + 10,
                  themes.text, FontStyle::SmallRegular, false, false, true);
  }

  // --- Rotator Section (Right Column) ---
  y = yBase;
  cat->drawText(renderer, "Rotator Control:", rightX, y, themes.text,
                FontStyle::SmallBold);
  y += cat->ptSize(FontStyle::SmallBold) + vSpace;

  cat->drawText(renderer, "rotctld Host:", rightX, y, themes.text,
                FontStyle::SmallRegular);
  rotatorHostRect_ = {rightX, y, colW, rigLabelH + fieldH};
  y += rigLabelH;
  rotatorHostInput_.render(renderer, fontMgr_, rightX, y, colW, fieldH,
                           FontStyle::SmallRegular, textPad,
                           (activeTab_ == Tab::Rig && activeField_ == 2), true,
                           themes.accent, themes.textDim, themes.text, themes.text, themes.textDim, "localhost", &themes.rowStripe1);
  y += fieldH + vSpace;

  cat->drawText(renderer, "rotctld Port:", rightX, y, themes.text,
                FontStyle::SmallRegular);
  rotatorPortRect_ = {rightX, y, colW, rigLabelH + fieldH};
  y += rigLabelH;
  rotatorPortInput_.render(renderer, fontMgr_, rightX, y, colW, fieldH,
                           FontStyle::SmallRegular, textPad,
                           (activeTab_ == Tab::Rig && activeField_ == 3), true,
                           themes.accent, themes.textDim, themes.text, themes.text, themes.textDim, "4533", &themes.rowStripe1);
  y += fieldH + vSpace;

  // Auto-track and Upover Toggles
  {
    // Auto-track
    SDL_Rect r1 = {rightX, y, 20, 20};
    rotatorAutoTrackRect_ = r1;
    SDL_SetRenderDrawColor(renderer, themes.rowStripe1.r, themes.rowStripe1.g, themes.rowStripe1.b, 255);
    SDL_RenderFillRect(renderer, &r1);
    SDL_SetRenderDrawColor(renderer, themes.border.r, themes.border.g, themes.border.b, 255);
    SDL_RenderDrawRect(renderer, &r1);
    if (rotatorAutoTrack_) {
      SDL_SetRenderDrawColor(renderer, themes.success.r, themes.success.g, themes.success.b, 255);
      SDL_Rect check = {r1.x + 4, r1.y + 4, 12, 12};
      SDL_RenderFillRect(renderer, &check);
    }
    cat->drawText(renderer, "Auto-track Satellite", rightX + 30, y + 10, themes.text,
                  FontStyle::SmallRegular, false, false, true);
    y += 24 + vSpace;

    // Upover
    SDL_Rect r2 = {rightX, y, 20, 20};
    rotatorUpoverRect_ = r2;
    SDL_SetRenderDrawColor(renderer, themes.rowStripe1.r, themes.rowStripe1.g, themes.rowStripe1.b, 255);
    SDL_RenderFillRect(renderer, &r2);
    SDL_SetRenderDrawColor(renderer, themes.border.r, themes.border.g, themes.border.b, 255);
    SDL_RenderDrawRect(renderer, &r2);
    if (rotatorUpover_) {
      SDL_SetRenderDrawColor(renderer, themes.success.r, themes.success.g, themes.success.b, 255);
      SDL_Rect check = {r2.x + 4, r2.y + 4, 12, 12};
      SDL_RenderFillRect(renderer, &check);
    }
    cat->drawText(renderer, "Upover Mode (Az flip)", rightX + 30, y + 10, themes.text,
                  FontStyle::SmallRegular, false, false, true);
  }

  y = yBase + 210; // Bottom of tab area, adjusted for clarity
  cat->drawText(renderer, "Requires 'rigctld'/'rotctld' (Hamlib) daemon.", cx,
                y, themes.textDim, FontStyle::Fast, true);
}
