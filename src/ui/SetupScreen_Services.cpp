#include "SetupScreen.h"
#include "../core/Theme.h"
#include "../core/MemoryMonitor.h"
#include <SDL.h>
#include <string>

void SetupScreen::renderTabServices(SDL_Renderer *renderer, int cx, int pad,
                                    int fieldW, int fieldH, int fieldX,
                                    int textPad) {
  auto *cat = fontMgr_.catalog();
  int y =
      (modalRect_.y + cat->ptSize(FontStyle::MediumBold) + 2 * pad + fieldH);
  int vSpace = pad / 2;
  ThemeColors themes = getThemeColors(theme_, colorOverrides_);
  int labelH = cat->ptSize(FontStyle::SmallBold) + 4;

  // 2-column layout: col1 (left), col2 (right)
  int colW = (fieldW - pad / 2) / 2;
  int col1X = fieldX;
  int col2X = fieldX + colW + pad / 2;

  // Row 1: QRZ Username | QRZ Password
  cat->drawText(renderer, "QRZ Username:", col1X, y, themes.text,
                FontStyle::SmallBold);
  qrzUsernameRect_ = {col1X, y, colW, labelH + fieldH};
  int yField = y + labelH;
  qrzUsernameInput_.render(renderer, fontMgr_, col1X, yField, colW, fieldH,
                           FontStyle::SmallRegular, textPad, activeField_ == 0,
                           true, themes.accent, themes.textDim, themes.text, themes.text, themes.textDim,
                           "e.g. K4DRW");

  cat->drawText(renderer, "QRZ Password:", col2X, y, themes.text,
                FontStyle::SmallBold);
  qrzPasswordRect_ = {col2X, y, colW, labelH + fieldH};
  {
    std::string passMask(qrzPasswordInput_.getValue().length(), '*');
    TextInput tmpPwd;
    tmpPwd.setValue(passMask);
    if (activeField_ == 1) {
      tmpPwd.setCursorPos(qrzPasswordInput_.getCursorPos());
      tmpPwd.setSelectionAnchor(qrzPasswordInput_.getSelectionAnchor());
    }
    tmpPwd.render(renderer, fontMgr_, col2X, yField, colW, fieldH,
                  FontStyle::SmallRegular, textPad, activeField_ == 1, true,
                  themes.accent, themes.textDim, themes.text, themes.text, themes.textDim, "********", &themes.rowStripe1);
  }
  y += labelH + fieldH + vSpace;

  // Row 2: LoTW Callsign | LoTW Password
  cat->drawText(renderer, "LoTW Callsign:", col1X, y, themes.text,
                FontStyle::SmallBold);
  lotwCallRect_ = {col1X, y, colW, labelH + fieldH};
  yField = y + labelH;
  lotwCallInput_.render(renderer, fontMgr_, col1X, yField, colW, fieldH,
                        FontStyle::SmallRegular, textPad, activeField_ == 2,
                        true, themes.accent, themes.textDim, themes.text, themes.text, themes.textDim,
                        "e.g. K4DRW");

  cat->drawText(renderer, "LoTW Password:", col2X, y, themes.text,
                FontStyle::SmallBold);
  lotwPasswordRect_ = {col2X, y, colW, labelH + fieldH};
  {
    std::string passMask(lotwPasswordInput_.getValue().length(), '*');
    TextInput tmpPwd;
    tmpPwd.setValue(passMask);
    if (activeField_ == 3) {
      tmpPwd.setCursorPos(lotwPasswordInput_.getCursorPos());
      tmpPwd.setSelectionAnchor(lotwPasswordInput_.getSelectionAnchor());
    }
    tmpPwd.render(renderer, fontMgr_, col2X, yField, colW, fieldH,
                  FontStyle::SmallRegular, textPad, activeField_ == 3, true,
                  themes.accent, themes.textDim, themes.text, themes.text, themes.textDim, "********", &themes.rowStripe1);
  }
  y += labelH + fieldH + vSpace;

  // Row 3: RepeaterBook Key | Winlink Key
  cat->drawText(renderer, "RepeaterBook Key:", col1X, y, themes.text,
                FontStyle::SmallBold);
  repeaterBookRect_ = {col1X, y, colW, labelH + fieldH};
  yField = y + labelH;
  repeaterBookInput_.render(renderer, fontMgr_, col1X, yField, colW, fieldH,
                            FontStyle::SmallRegular, textPad, activeField_ == 4,
                            true, themes.accent, themes.textDim, themes.text, themes.text, themes.textDim, "Key", &themes.rowStripe1);

  cat->drawText(renderer, "Winlink Key:", col2X, y, themes.text,
                FontStyle::SmallBold);
  winlinkRect_ = {col2X, y, colW, labelH + fieldH};
  winlinkInput_.render(renderer, fontMgr_, col2X, yField, colW, fieldH,
                       FontStyle::SmallRegular, textPad, activeField_ == 5,
                       true, themes.accent, themes.textDim, themes.text, themes.text, themes.textDim, "Key", &themes.rowStripe1);
  y += labelH + fieldH + vSpace;

  // Row 4: Clublog API Key (full width)
  cat->drawText(renderer, "Clublog API Key:", fieldX, y, themes.text,
                FontStyle::SmallBold);
  int clublogFieldW = fieldW;
  clublogApiKeyRect_ = {fieldX, y, clublogFieldW, labelH + fieldH};
  yField = y + labelH;
  // Render the input field for Clublog API Key
  clublogApiKeyInput_.render(renderer, fontMgr_, fieldX, yField, clublogFieldW, fieldH,
                             FontStyle::SmallRegular, textPad, activeField_ == 6,
                             true, themes.accent, themes.textDim, themes.text, themes.text, themes.textDim, "Key");
}
