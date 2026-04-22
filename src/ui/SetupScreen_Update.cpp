#include "SetupScreen.h"
#include "../core/Theme.h"
#include "../services/UpdateChecker.h"
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
  // Show INSTALL_TYPE; append variant in parens if known (e.g. "DEB (fb0)")
  std::string installLabel = HAMCLOCK_INSTALL_TYPE;
  std::string variant = HAMCLOCK_BUILD_VARIANT;
  if (!variant.empty())
    installLabel += " (" + variant + ")";
  int lenI = fontMgr_.getLogicalWidth(installLabel,
                                      cat->ptSize(FontStyle::SmallRegular));
  cat->drawText(renderer, installLabel.c_str(), fieldX + fieldW - lenI, y,
                themes.textDim, FontStyle::SmallRegular);
  y += cat->ptSize(FontStyle::SmallRegular) + 8;

  cat->drawText(renderer, "Build Date:", fieldX, y, themes.text,
                FontStyle::SmallRegular);
  int lenB = fontMgr_.getLogicalWidth(HAMCLOCK_BUILD_DATETIME,
                                      cat->ptSize(FontStyle::SmallRegular));
  cat->drawText(renderer, HAMCLOCK_BUILD_DATETIME, fieldX + fieldW - lenB, y,
                themes.textDim, FontStyle::SmallRegular);
  y += cat->ptSize(FontStyle::SmallRegular) + pad * 2;

  // --- Static instructions (always shown) ---
  std::string instr;
  std::string cmd;
  std::string type = HAMCLOCK_INSTALL_TYPE;

  if (type == "RPM") {
    instr = "To update via DNF, run:";
    cmd = "sudo dnf update hamclock-next";
  } else if (type == "DEB") {
    instr = "To update, download and install the .deb below:";
    cmd = "sudo apt install ./hamclock-next-update.deb";
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
  y += cat->ptSize(FontStyle::SmallBold) + pad * 2;

  // --- Download UI (only when updateChecker is wired in) ---
  downloadBtnRect_ = {0, 0, 0, 0};  // reset each frame

#ifndef __EMSCRIPTEN__
  if (!updateChecker_ || type == "WASM")
    return;

  if (!updateChecker_->updateAvailable())
    return;

  const auto dlState = updateChecker_->downloadState();

  if (dlState == UpdateChecker::DownloadState::Idle ||
      dlState == UpdateChecker::DownloadState::Failed) {

    const std::string dlUrl = updateChecker_->downloadUrl();
    if (dlUrl.empty())
      return;  // no matching asset for this platform

    if (dlState == UpdateChecker::DownloadState::Failed) {
      cat->drawText(renderer, "Download failed — check network and try again.",
                    cx, y, themes.danger, FontStyle::SmallRegular, true);
      y += cat->ptSize(FontStyle::SmallRegular) + pad;
    }

    // Download button
    const std::string latest = updateChecker_->latestVersion();
    std::string btnLabel = "Download " + latest;
    int btnW = fontMgr_.getLogicalWidth(btnLabel,
                                        cat->ptSize(FontStyle::SmallBold)) +
               pad * 4;
    int btnX = cx - btnW / 2;
    downloadBtnRect_ = {btnX, y, btnW, fieldH};

    SDL_SetRenderDrawColor(renderer, themes.rowStripe1.r, themes.rowStripe1.g,
                           themes.rowStripe1.b, 255);
    SDL_RenderFillRect(renderer, &downloadBtnRect_);
    SDL_SetRenderDrawColor(renderer, themes.accent.r, themes.accent.g,
                           themes.accent.b, 255);
    SDL_RenderDrawRect(renderer, &downloadBtnRect_);
    cat->drawText(renderer, btnLabel.c_str(), cx,
                  y + fieldH / 2, themes.accent, FontStyle::SmallBold,
                  true, false, true);

  } else if (dlState == UpdateChecker::DownloadState::InProgress) {

    float pct = updateChecker_->downloadProgress();
    int barW = fieldW;
    int barH = fieldH / 2;
    int filled = static_cast<int>(barW * pct);

    // Progress bar background
    SDL_Rect barBg = {fieldX, y, barW, barH};
    SDL_SetRenderDrawColor(renderer, themes.rowStripe1.r, themes.rowStripe1.g,
                           themes.rowStripe1.b, 255);
    SDL_RenderFillRect(renderer, &barBg);

    // Progress bar fill
    if (filled > 0) {
      SDL_Rect barFill = {fieldX, y, filled, barH};
      SDL_SetRenderDrawColor(renderer, themes.accent.r, themes.accent.g,
                             themes.accent.b, 255);
      SDL_RenderFillRect(renderer, &barFill);
    }

    // Border
    SDL_SetRenderDrawColor(renderer, themes.border.r, themes.border.g,
                           themes.border.b, 255);
    SDL_RenderDrawRect(renderer, &barBg);

    y += barH + 6;
    std::string pctLabel = "Downloading\u2026 " +
                           std::to_string(static_cast<int>(pct * 100.0f)) + "%";
    cat->drawText(renderer, pctLabel.c_str(), cx, y, themes.text,
                  FontStyle::SmallRegular, true);

  } else if (dlState == UpdateChecker::DownloadState::Complete) {

    const std::string path = updateChecker_->downloadedPath();

    cat->drawText(renderer, "Download complete!", cx, y, themes.accent,
                  FontStyle::SmallBold, true);
    y += cat->ptSize(FontStyle::SmallBold) + 6;

    // Show where the file was saved
    std::string savedLabel = "Saved: " + path;
    cat->drawText(renderer, savedLabel.c_str(), cx, y, themes.textDim,
                  FontStyle::SmallRegular, true);
    y += cat->ptSize(FontStyle::SmallRegular) + pad;

    // Platform-specific install command
    std::string installCmd;
    if (type == "DEB")
      installCmd = "sudo apt install " + path;
    else if (type == "RPM")
      installCmd = "sudo rpm -Uvh " + path;
    else if (type == "BIN")
      installCmd = "Replace current binary with " + path + " and restart.";
    else
      installCmd = "Run the downloaded installer: " + path;

    cat->drawText(renderer, "To install:", cx, y, themes.text,
                  FontStyle::SmallRegular, true);
    y += cat->ptSize(FontStyle::SmallRegular) + 6;
    cat->drawText(renderer, installCmd.c_str(), cx, y, themes.accent,
                  FontStyle::SmallBold, true);
  }
#endif  // __EMSCRIPTEN__
}
