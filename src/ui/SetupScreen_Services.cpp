#include "SetupScreen.h"
#include "../core/Theme.h"
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

  cat->drawText(renderer, "QRZ Username:", fieldX, y, themes.text,
                FontStyle::SmallBold);
  int labelH = cat->ptSize(FontStyle::SmallBold) + 4;
  qrzUsernameRect_ = {fieldX, y, fieldW, labelH + fieldH};
  y += labelH;
  qrzUsernameInput_.render(renderer, fontMgr_, fieldX, y, fieldW, fieldH,
                           FontStyle::SmallRegular, textPad, activeField_ == 0,
                           true, themes.accent, themes.textDim, themes.text, themes.text, themes.textDim,
                           "e.g. K4DRW");
  y += fieldH + vSpace;

  cat->drawText(renderer, "QRZ Password:", fieldX, y, themes.text,
                FontStyle::SmallBold);
  qrzPasswordRect_ = {fieldX, y, fieldW, labelH + fieldH};
  y += labelH;
  {
    std::string passMask(qrzPasswordInput_.getValue().length(), '*');
    TextInput tmpPwd;
    tmpPwd.setValue(passMask);
    if (activeField_ == 1) {
      tmpPwd.setCursorPos(qrzPasswordInput_.getCursorPos());
      tmpPwd.setSelectionAnchor(qrzPasswordInput_.getSelectionAnchor());
    }
    tmpPwd.render(renderer, fontMgr_, fieldX, y, fieldW, fieldH,
                  FontStyle::SmallRegular, textPad, activeField_ == 1, true,
                  themes.accent, themes.textDim, themes.text, themes.text, themes.textDim, "********", &themes.rowStripe1);
  }
  y += fieldH + vSpace;

  cat->drawText(renderer, "RepeaterBook Key:", fieldX, y, themes.text,
                FontStyle::SmallBold);
  repeaterBookRect_ = {fieldX, y, fieldW, labelH + fieldH};
  y += labelH;
  repeaterBookInput_.render(renderer, fontMgr_, fieldX, y, fieldW, fieldH,
                            FontStyle::SmallRegular, textPad, activeField_ == 2,
                            true, themes.accent, themes.textDim, themes.text, themes.text, themes.textDim, "Key", &themes.rowStripe1);
  y += fieldH + vSpace;

  cat->drawText(renderer, "Winlink Key:", fieldX, y, themes.text,
                FontStyle::SmallBold);
  winlinkRect_ = {fieldX, y, fieldW, labelH + fieldH};
  y += labelH;
  winlinkInput_.render(renderer, fontMgr_, fieldX, y, fieldW, fieldH,
                       FontStyle::SmallRegular, textPad, activeField_ == 3,
                       true, themes.accent, themes.textDim, themes.text, themes.text, themes.textDim, "Key", &themes.rowStripe1);
  y += fieldH + vSpace;
}
