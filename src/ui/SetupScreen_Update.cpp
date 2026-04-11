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
  cat->drawText(renderer, HAMCLOCK_VERSION, fieldX + fieldW - lenV, y,
                themes.textDim, FontStyle::SmallRegular);
  y += cat->ptSize(FontStyle::SmallRegular) + 8;

  cat->drawText(renderer, "Platform Architecture:", fieldX, y, themes.text,
                FontStyle::SmallRegular);
  int lenA = fontMgr_.getLogicalWidth(HAMCLOCK_ARCH,
                                      cat->ptSize(FontStyle::SmallRegular));
  cat->drawText(renderer, HAMCLOCK_ARCH, fieldX + fieldW - lenA, y,
                themes.textDim, FontStyle::SmallRegular);
  y += cat->ptSize(FontStyle::SmallRegular) + 8;

  cat->drawText(renderer, "Installation Type:", fieldX, y, themes.text,
                FontStyle::SmallRegular);
  int lenI = fontMgr_.getLogicalWidth(HAMCLOCK_INSTALL_TYPE,
                                      cat->ptSize(FontStyle::SmallRegular));
  cat->drawText(renderer, HAMCLOCK_INSTALL_TYPE, fieldX + fieldW - lenI, y,
                themes.textDim, FontStyle::SmallRegular);
  y += cat->ptSize(FontStyle::SmallRegular) + 8;

  cat->drawText(renderer, "Build Date:", fieldX, y, themes.text,
                FontStyle::SmallRegular);
  int lenB = fontMgr_.getLogicalWidth(HAMCLOCK_BUILD_DATETIME,
                                      cat->ptSize(FontStyle::SmallRegular));
  cat->drawText(renderer, HAMCLOCK_BUILD_DATETIME, fieldX + fieldW - lenB, y,
                themes.textDim, FontStyle::SmallRegular);
  y += cat->ptSize(FontStyle::SmallRegular) + pad * 2;
  // Implementation-specific instructions
  std::string instr;
  std::string cmd;
  std::string type = HAMCLOCK_INSTALL_TYPE;

  if (type == "RPM") {
    instr = "To update via DNF, run:";
    cmd = "sudo dnf update hamclock-next";
  } else if (type == "DEB") {
    instr = "To update, download the latest .deb and install it:";
    cmd = "sudo apt install ./hamclock-next.deb";
  } else if (type == "WASM") {
    instr = "To update HamClock-Next in the browser:";
    cmd = "Please reload the page to update.";
  } else {
    instr = "To update the binary installation:";
    cmd = "Download the latest release from GitHub.";
  }

  cat->drawText(renderer, instr, cx, y, themes.text, FontStyle::SmallRegular,
                true);
  y += cat->ptSize(FontStyle::SmallRegular) + 8;
  cat->drawText(renderer, cmd, cx, y, themes.accent, FontStyle::SmallBold,
                true);
}
