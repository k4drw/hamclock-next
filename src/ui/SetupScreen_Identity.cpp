#include "SetupScreen.h"
#include "../core/Astronomy.h"
#include "../core/Theme.h"
#include <SDL.h>
#include <algorithm>
#include <ctime>

void SetupScreen::renderTabIdentity(SDL_Renderer *renderer, int cx, int pad,
                                    int fieldW, int fieldH, int fieldX,
                                    int textPad) {
  auto *cat = fontMgr_.catalog();
  int y =
      (modalRect_.y + cat->ptSize(FontStyle::MediumBold) + 2 * pad + fieldH);
  int vSpace = pad / 2;
  ThemeColors themes = getThemeColors(theme_, colorOverrides_);

  int labelH = cat->ptSize(FontStyle::SmallBold) + 4;

  cat->drawText(renderer, "Callsign:", fieldX, y, themes.text, FontStyle::SmallBold);
  y += labelH;
  callsignRect_ = {fieldX, y, fieldW, fieldH};
  callsignInput_.render(renderer, fontMgr_, fieldX, y, fieldW, fieldH,
                        FontStyle::SmallRegular, textPad, activeField_ == 0,
                        !callsignInput_.getValue().empty(), themes.accent, themes.textDim, themes.success, themes.text, themes.textDim, "e.g. K4DRW", &themes.rowStripe1);
  y += fieldH + vSpace;

  cat->drawText(renderer, "Grid Square:", fieldX, y, themes.text,
                FontStyle::SmallBold);
  y += labelH;
  gridRect_ = {fieldX, y, fieldW, fieldH};
  gridInput_.render(renderer, fontMgr_, fieldX, y, fieldW, fieldH,
                    FontStyle::SmallRegular, textPad, activeField_ == 1,
                    gridValid_, themes.accent, themes.textDim, themes.success, themes.text, themes.textDim,
                    "e.g. EL87qr", &themes.rowStripe1);
  y += fieldH + vSpace;

  int halfFieldW = (fieldW - pad) / 2;
  int labelY = y;
  cat->drawText(renderer, "Latitude:", fieldX, labelY, themes.text,
                FontStyle::SmallBold);
  cat->drawText(renderer, "Longitude:", fieldX + halfFieldW + pad, labelY,
                themes.text, FontStyle::SmallBold);

  // Mismatch or Auto-calc status: right-aligned to the Latitude input's right
  // edge
  if (mismatchWarning_) {
    cat->drawText(renderer, "Warning: Lat/Lon mismatch!",
                  fieldX + halfFieldW / 2, labelY + 2, themes.danger, FontStyle::Fast,
                  true, false, true);
  } else if (gridValid_ && !latLonManual_) {
    cat->drawText(renderer, "Auto-calculated", fieldX + halfFieldW / 2,
                  labelY + 2, themes.textDim, FontStyle::Fast, true, false, true);
  }

  latRect_ = {fieldX, y, halfFieldW, labelH + fieldH};
  lonRect_ = {fieldX + halfFieldW + pad, y, halfFieldW, labelH + fieldH};
  y += labelH;

  int latY = y;
  latInput_.render(renderer, fontMgr_, fieldX, latY, halfFieldW, fieldH,
                   FontStyle::SmallRegular, textPad, activeField_ == 2,
                   !latInput_.getValue().empty(), themes.accent, themes.textDim, themes.text, themes.text,
                   themes.textDim, "e.g. 27.76", &themes.rowStripe1);

  int lonY = y;
  lonInput_.render(renderer, fontMgr_, fieldX + halfFieldW + pad, lonY,
                   halfFieldW, fieldH, FontStyle::SmallRegular, textPad,
                   activeField_ == 3, !lonInput_.getValue().empty(), themes.accent,
                   themes.textDim, themes.text, themes.text, themes.textDim, "e.g. -82.64", &themes.rowStripe1);
  y = std::max(latY, lonY) + pad / 2 + pad;

  int gpsLabelW = fontMgr_.getLogicalWidth("Synchronize with GPS (gpsd)", cat->ptSize(FontStyle::SmallRegular));
  gpsToggleRect_ = {fieldX, y, 30 + gpsLabelW, 20};
  SDL_Rect gpsBox = {fieldX, y, 20, 20};
  SDL_SetRenderDrawColor(renderer, themes.rowStripe1.r, themes.rowStripe1.g, themes.rowStripe1.b, 255);
  SDL_RenderFillRect(renderer, &gpsBox);
  SDL_SetRenderDrawColor(renderer, themes.border.r, themes.border.g, themes.border.b, 255);
  SDL_RenderDrawRect(renderer, &gpsBox);
  if (gpsEnabled_) {
    SDL_SetRenderDrawColor(renderer, themes.success.r, themes.success.g, themes.success.b, 255);
    SDL_Rect check = {fieldX + 4, y + 4, 12, 12};
    SDL_RenderFillRect(renderer, &check);
  }
  cat->drawText(renderer, "Synchronize with GPS (gpsd)", fieldX + 30, y + 10,
                themes.text, FontStyle::SmallRegular, false, false, true);
  y += 28;

  int muteLabelW = fontMgr_.getLogicalWidth("Mute all audio (TTS + alarm)", cat->ptSize(FontStyle::SmallRegular));
  audioMuteToggleRect_ = {fieldX, y, 30 + muteLabelW, 20};
  SDL_Rect muteBox = {fieldX, y, 20, 20};
  SDL_SetRenderDrawColor(renderer, themes.rowStripe1.r, themes.rowStripe1.g, themes.rowStripe1.b, 255);
  SDL_RenderFillRect(renderer, &muteBox);
  SDL_SetRenderDrawColor(renderer, themes.border.r, themes.border.g, themes.border.b, 255);
  SDL_RenderDrawRect(renderer, &muteBox);
  if (audioMuted_) {
    SDL_SetRenderDrawColor(renderer, themes.success.r, themes.success.g, themes.success.b, 255);
    SDL_Rect check = {fieldX + 4, y + 4, 12, 12};
    SDL_RenderFillRect(renderer, &check);
  }
  cat->drawText(renderer, "Mute all audio (TTS + alarm)", fieldX + 30, y + 10,
                themes.text, FontStyle::SmallRegular, false, false, true);
  y += 28;

  cat->drawText(renderer, "Volume:", fieldX, y + 10, themes.text, FontStyle::SmallRegular, false, false, true);
  int volLabelW = fontMgr_.getLogicalWidth("Volume:", cat->ptSize(FontStyle::SmallRegular));
  int sliderX = fieldX + volLabelW + 10;
  int sliderW = fieldW - (volLabelW + 10) - 50;
  audioVolumeSliderRect_ = {sliderX, y, sliderW, 20};
  
  // Slider track
  SDL_Rect track = {sliderX, y + 8, sliderW, 4};
  SDL_SetRenderDrawColor(renderer, themes.rowStripe1.r, themes.rowStripe1.g, themes.rowStripe1.b, 255);
  SDL_RenderFillRect(renderer, &track);
  SDL_SetRenderDrawColor(renderer, themes.border.r, themes.border.g, themes.border.b, 255);
  SDL_RenderDrawRect(renderer, &track);
  
  // Slider thumb
  int thumbX = sliderX + (audioVolume_ * sliderW / 100);
  SDL_Rect thumb = {thumbX - 5, y, 10, 20};
  SDL_SetRenderDrawColor(renderer, themes.accent.r, themes.accent.g, themes.accent.b, 255);
  SDL_RenderFillRect(renderer, &thumb);
  SDL_SetRenderDrawColor(renderer, themes.border.r, themes.border.g, themes.border.b, 255);
  SDL_RenderDrawRect(renderer, &thumb);
  
  // Percentage text
  char volBuf[8];
  std::snprintf(volBuf, sizeof(volBuf), "%d%%", audioVolume_);
  cat->drawText(renderer, volBuf, sliderX + sliderW + 10, y + 10, themes.text, FontStyle::SmallRegular, false, false, true);

  y += 28 + vSpace;

  cat->drawText(renderer, "Default Timezone:", fieldX, y, themes.text, FontStyle::SmallBold);
  y += labelH;
  defaultTzRect_ = {fieldX, y, fieldW, fieldH};
  SDL_SetRenderDrawColor(renderer, themes.rowStripe1.r, themes.rowStripe1.g, themes.rowStripe1.b, 255);
  SDL_RenderFillRect(renderer, &defaultTzRect_);
  SDL_SetRenderDrawColor(renderer, themes.border.r, themes.border.g, themes.border.b, 255);
  SDL_RenderDrawRect(renderer, &defaultTzRect_);
  
  char tzBuf[64];
  if (defaultTzOffset_ == 999) {
    std::time_t now = std::time(nullptr);
    struct tm local{};
    Astronomy::portable_localtime(&now, &local);
    std::string abbr = Astronomy::portable_tzabbr(local);
    std::snprintf(tzBuf, sizeof(tzBuf), "%s", abbr.c_str());
  } else std::snprintf(tzBuf, sizeof(tzBuf), "%s (UTC%+d)", defaultTzLabel_.c_str(), defaultTzOffset_);
  cat->drawText(renderer, tzBuf, fieldX + textPad, y + fieldH / 2, themes.text, FontStyle::SmallRegular, false, false, true);
  
  // "Change" hint
  int hintW = fontMgr_.getLogicalWidth("Change...", cat->ptSize(FontStyle::Tiny));
  cat->drawText(renderer, "Change...", fieldX + fieldW - hintW - textPad, y + fieldH / 2, themes.accent, FontStyle::Tiny, false, false, true);

  y += fieldH + vSpace;
}
