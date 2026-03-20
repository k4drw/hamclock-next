#include "SetupScreen.h"
#include "../core/Theme.h"
#include <SDL.h>
#include <string>

void SetupScreen::renderTabUpdate(SDL_Renderer *renderer, int cx, int pad,
                                   int fieldW, int fieldH, int fieldX, int) {
  auto *cat = fontMgr_.catalog();
  int y =
      (modalRect_.y + cat->ptSize(FontStyle::MediumBold) + 2 * pad + fieldH);
  ThemeColors themes = getThemeColors(theme_, colorOverrides_);

  cat->drawText(renderer, "--- Update Status ---", cx, y, themes.accent,
                FontStyle::SmallBold, true);
  y += cat->ptSize(FontStyle::SmallBold) + pad;

  cat->drawText(renderer, "Current Version:", fieldX, y, themes.text,
                FontStyle::SmallRegular);
  int lenV = fontMgr_.getLogicalWidth(HAMCLOCK_VERSION,
                                      cat->ptSize(FontStyle::SmallRegular));
  cat->drawText(renderer, HAMCLOCK_VERSION, fieldX + fieldW - lenV, y, themes.textDim,
                FontStyle::SmallRegular);
  y += cat->ptSize(FontStyle::SmallRegular) + 8;

  cat->drawText(renderer, "Platform Architecture:", fieldX, y, themes.text,
                FontStyle::SmallRegular);
  int lenA = fontMgr_.getLogicalWidth(HAMCLOCK_ARCH,
                                      cat->ptSize(FontStyle::SmallRegular));
  cat->drawText(renderer, HAMCLOCK_ARCH, fieldX + fieldW - lenA, y, themes.textDim,
                FontStyle::SmallRegular);
  y += cat->ptSize(FontStyle::SmallRegular) + 8;

  cat->drawText(renderer, "Installation Type:", fieldX, y, themes.text,
                FontStyle::SmallRegular);
  int lenI = fontMgr_.getLogicalWidth(HAMCLOCK_INSTALL_TYPE,
                                      cat->ptSize(FontStyle::SmallRegular));
  cat->drawText(renderer, HAMCLOCK_INSTALL_TYPE, fieldX + fieldW - lenI, y,
                themes.textDim, FontStyle::SmallRegular);
  y += cat->ptSize(FontStyle::SmallRegular) + pad * 2;
  // Implementation-specific instructions
  std::string instr;
  std::string cmd;
  std::string type = HAMCLOCK_INSTALL_TYPE;

  if (type == "RPM") {
    instr = "An update is available via DNF.";
    cmd = "sudo dnf update hamclock-next";
  } else if (type == "DEB") {
    instr = "Download the latest .deb and install it.";
    cmd = "sudo apt install ./hamclock-next.deb";
  } else if (type == "WASM") {
    instr = "A new version of HamClock-Next is available.";
    cmd = "Please reload the page to update.";
  } else {
    instr = "Binary update available.";
    cmd = "Download the latest release from GitHub.";
  }

  cat->drawText(renderer, instr, cx, y, themes.text, FontStyle::SmallRegular, true);
  y += cat->ptSize(FontStyle::SmallRegular) + 8;
  cat->drawText(renderer, cmd, cx, y, themes.accent, FontStyle::SmallBold, true);
}
