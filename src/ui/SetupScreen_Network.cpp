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

  // Measure title to place description next to it
  int titleW = fontMgr_.getLogicalWidth("--- Local Data Hub ---", cat->ptSize(FontStyle::SmallBold), true);
  
  if (hubMode_ == HubMode::Master) {
    cat->drawText(renderer, "This instance serves cached data to hub clients.",
                  fieldX + titleW + pad, y + (cat->ptSize(FontStyle::SmallBold) - cat->ptSize(FontStyle::Fast))/2, themes.textDim, FontStyle::Fast);
  } else if (hubMode_ == HubMode::Client) {
    cat->drawText(renderer, "Fetches via hub; falls back to direct after 10s.",
                  fieldX + titleW + pad, y + (cat->ptSize(FontStyle::SmallBold) - cat->ptSize(FontStyle::Fast))/2, themes.textDim, FontStyle::Fast);
  } else {
    cat->drawText(renderer, "Hub mode disabled.", fieldX + titleW + pad, y + (cat->ptSize(FontStyle::SmallBold) - cat->ptSize(FontStyle::Fast))/2, themes.textDim,
                  FontStyle::Fast);
  }

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
    int col1X = fieldX;
    int colW = (fieldW - pad / 2) / 2;
    int col2X = fieldX + colW + pad / 2;

    cat->drawText(renderer, "Hub IP:", col1X, y, themes.text,
                  FontStyle::SmallRegular);
    hubIpRect_ = {col1X, y, colW, labelH + fieldH};
    hubIpInput_.render(renderer, fontMgr_, col1X, y + labelH, colW, fieldH,
                       FontStyle::SmallRegular, textPad, activeField_ == 0,
                       true, themes.accent, themes.textDim, themes.text, themes.text, themes.textDim,
                       "e.g. 192.168.1.100");

    cat->drawText(renderer, "Hub Port:", col2X, y, themes.text,
                  FontStyle::SmallRegular);
    hubPortRect_ = {col2X, y, colW, labelH + fieldH};
    hubPortInput_.render(renderer, fontMgr_, col2X, y + labelH, colW, fieldH,
                         FontStyle::SmallRegular, textPad, activeField_ == 1,
                         true, themes.accent, themes.textDim, themes.text, themes.text, themes.textDim, "8080", &themes.rowStripe1);
                         
    y += labelH + fieldH + vSpace;
  } else {
    hubIpRect_ = {0, 0, 0, 0};
    hubPortRect_ = {0, 0, 0, 0};
  }
  
  y += pad; // extra breathing room

  // --- MQTT ---
  cat->drawText(renderer, "--- MQTT Configuration ---", fieldX, y, themes.accent, FontStyle::SmallBold);
  y += cat->ptSize(FontStyle::SmallBold) + pad;

  mqttEnabledRect_ = {fieldX, y, fieldW, cat->ptSize(FontStyle::SmallBold)};
  cat->drawText(renderer, mqttEnabled_ ? "[X] Enable MQTT" : "[ ] Enable MQTT",
                fieldX, y, themes.text, FontStyle::SmallBold);
  y += cat->ptSize(FontStyle::SmallBold) + vSpace;

  if (mqttEnabled_) {
    int labelH = cat->ptSize(FontStyle::SmallBold) + 4;
    int col1X = fieldX;
    int colW = (fieldW - pad / 2) / 2;
    int col2X = fieldX + colW + pad / 2;

    // Row 6: Broker | Base Topic
    cat->drawText(renderer, "MQTT Broker (ws://):", col1X, y, themes.text,
                  FontStyle::SmallBold);
    mqttBrokerRect_ = {col1X, y, colW, labelH + fieldH};
    int yField = y + labelH;
    mqttBrokerInput_.render(renderer, fontMgr_, col1X, yField, colW, fieldH,
                              FontStyle::SmallRegular, textPad, activeField_ == 2,
                              true, themes.accent, themes.textDim, themes.text, themes.text, themes.textDim, "ws://...", &themes.rowStripe1);

    cat->drawText(renderer, "Base Topic:", col2X, y, themes.text,
                  FontStyle::SmallBold);
    mqttTopicRect_ = {col2X, y, colW, labelH + fieldH};
    mqttTopicInput_.render(renderer, fontMgr_, col2X, yField, colW, fieldH,
                             FontStyle::SmallRegular, textPad, activeField_ == 3,
                             true, themes.accent, themes.textDim, themes.text, themes.text, themes.textDim, "hamclock", &themes.rowStripe1);
    y += labelH + fieldH + vSpace;

    // Row 7: Username | Password
    cat->drawText(renderer, "MQTT Username:", col1X, y, themes.text,
                  FontStyle::SmallBold);
    mqttUsernameRect_ = {col1X, y, colW, labelH + fieldH};
    yField = y + labelH;
    mqttUsernameInput_.render(renderer, fontMgr_, col1X, yField, colW, fieldH,
                                FontStyle::SmallRegular, textPad, activeField_ == 4,
                                true, themes.accent, themes.textDim, themes.text, themes.text, themes.textDim, "(Optional)");

    cat->drawText(renderer, "MQTT Password:", col2X, y, themes.text,
                  FontStyle::SmallBold);
    mqttPasswordRect_ = {col2X, y, colW, labelH + fieldH};
    {
      std::string passMask(mqttPasswordInput_.getValue().length(), '*');
      TextInput tmpPwd;
      tmpPwd.setValue(passMask);
      if (activeField_ == 5) {
        tmpPwd.setCursorPos(mqttPasswordInput_.getCursorPos());
        tmpPwd.setSelectionAnchor(mqttPasswordInput_.getSelectionAnchor());
      }
      tmpPwd.render(renderer, fontMgr_, col2X, yField, colW, fieldH,
                    FontStyle::SmallRegular, textPad, activeField_ == 5, true,
                    themes.accent, themes.textDim, themes.text, themes.text, themes.textDim, "(Optional)");
    }
  } else {
    mqttBrokerRect_ = {0,0,0,0};
    mqttTopicRect_ = {0,0,0,0};
    mqttUsernameRect_ = {0,0,0,0};
    mqttPasswordRect_ = {0,0,0,0};
  }
}
