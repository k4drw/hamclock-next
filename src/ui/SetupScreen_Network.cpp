#include "SetupScreen.h"
#include "../core/Theme.h"
#include <SDL.h>

void SetupScreen::renderTabNetwork(SDL_Renderer *renderer, int /*cx*/, int pad,
                                   int fieldW, int fieldH, int fieldX,
                                   int textPad) {
  auto *cat = fontMgr_.catalog();
  int y =
      (modalRect_.y + cat->ptSize(FontStyle::MediumBold) + 2 * pad + fieldH);
  int vSpace = pad / 2;
  ThemeColors themes = getThemeColors(theme_, colorOverrides_);

  cat->drawText(renderer, "--- Local Data Hub ---", fieldX, y, themes.accent,
                FontStyle::SmallBold);
  y += cat->ptSize(FontStyle::SmallBold) + pad;

  // Mode cycle button
  const char *modeLabel = (hubMode_ == HubMode::Master)   ? "Master"
                          : (hubMode_ == HubMode::Client) ? "Client"
                                                          : "Off";
  int btnW = 80;
  hubModeRect_ = {fieldX + fieldW - btnW, y, btnW, fieldH};
  cat->drawText(renderer, "Mode:", fieldX,
                y + (fieldH - cat->ptSize(FontStyle::SmallRegular)) / 2, themes.text,
                FontStyle::SmallRegular);
  SDL_SetRenderDrawColor(renderer, themes.rowStripe1.r, themes.rowStripe1.g, themes.rowStripe1.b, 255);
  SDL_RenderFillRect(renderer, &hubModeRect_);
  SDL_SetRenderDrawColor(renderer, themes.accent.r, themes.accent.g, themes.accent.b, 255);
  SDL_RenderDrawRect(renderer, &hubModeRect_);
  cat->drawText(renderer, modeLabel, hubModeRect_.x + hubModeRect_.w / 2,
                hubModeRect_.y + hubModeRect_.h / 2, themes.accent,
                FontStyle::SmallRegular, true, false, true);
  y += fieldH + vSpace;

  if (hubMode_ == HubMode::Client) {
    int labelH = cat->ptSize(FontStyle::SmallRegular) + 4;
    cat->drawText(renderer, "Hub IP:", fieldX, y, themes.text,
                  FontStyle::SmallRegular);
    hubIpRect_ = {fieldX, y, fieldW, labelH + fieldH};
    y += labelH;
    hubIpInput_.render(renderer, fontMgr_, fieldX, y, fieldW, fieldH,
                       FontStyle::SmallRegular, textPad, activeField_ == 0,
                       true, themes.accent, themes.textDim, themes.text, themes.text, themes.textDim,
                       "e.g. 192.168.1.100");
    y += fieldH + vSpace;

    cat->drawText(renderer, "Hub Port:", fieldX, y, themes.text,
                  FontStyle::SmallRegular);
    hubPortRect_ = {fieldX, y, fieldW, labelH + fieldH};
    y += labelH;
    hubPortInput_.render(renderer, fontMgr_, fieldX, y, fieldW, fieldH,
                         FontStyle::SmallRegular, textPad, activeField_ == 1,
                         true, themes.accent, themes.textDim, themes.text, themes.text, themes.textDim, "8080", &themes.rowStripe1);
    y += fieldH + vSpace;
  } else {
    hubIpRect_ = {0, 0, 0, 0};
    hubPortRect_ = {0, 0, 0, 0};
  }

  if (hubMode_ == HubMode::Master) {
    cat->drawText(renderer, "This instance serves cached data to hub clients.",
                  fieldX, y + vSpace, themes.textDim, FontStyle::Fast);
  } else if (hubMode_ == HubMode::Client) {
    cat->drawText(renderer, "Fetches via hub; falls back to direct after 10s.",
                  fieldX, y + vSpace, themes.textDim, FontStyle::Fast);
  } else {
    cat->drawText(renderer, "Hub mode disabled.", fieldX, y + vSpace, themes.textDim,
                  FontStyle::Fast);
  }
}
