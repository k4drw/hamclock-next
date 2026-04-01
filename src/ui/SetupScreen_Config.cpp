#include "SetupScreen.h"
#include "WidgetRegistry.h"
#include "../core/StringUtils.h"
#include <SDL.h>
#include <string>

void SetupScreen::setConfig(const AppConfig &cfg) {
  // Set max lengths
  callsignInput_.setMaxLength(12);
  gridInput_.setMaxLength(6);
  latInput_.setMaxLength(12);
  lonInput_.setMaxLength(12);
  clusterHostInput_.setMaxLength(64);
  clusterPortInput_.setMaxLength(5);
  clusterLoginInput_.setMaxLength(12);
  wsjtxPortInput_.setMaxLength(5);
  qrzUsernameInput_.setMaxLength(32);
  qrzPasswordInput_.setMaxLength(32);
  dimTimeInput_.setMaxLength(5);
  brightTimeInput_.setMaxLength(5);
  rigHostInput_.setMaxLength(64);
  rigPortInput_.setMaxLength(5);
  hubIpInput_.setMaxLength(40);
  hubPortInput_.setMaxLength(5);
  watchlistInputField_.setMaxLength(256);

  gpsEnabled_ = cfg.gpsEnabled;
  audioMuted_ = cfg.audioMuted;
  callsignInput_.setValue(cfg.callsign);
  gridInput_.setValue(cfg.grid);
  frnText_ = cfg.callsignFrn;
  if (cfg.lat != 0.0 || cfg.lon != 0.0) {
    char buf[32];
    std::snprintf(buf, sizeof(buf), "%.4f", cfg.lat);
    latInput_.setValue(buf);
    std::snprintf(buf, sizeof(buf), "%.4f", cfg.lon);
    lonInput_.setValue(buf);
  }
  clusterHostInput_.setValue(cfg.dxClusterHost);
  clusterPortInput_.setValue(std::to_string(cfg.dxClusterPort));
  clusterLoginInput_.setValue(cfg.dxClusterLogin);
  clusterEnabled_ = cfg.dxClusterEnabled;
  clusterWSJTX_ = cfg.dxClusterUseWSJTX;
  wsjtxPortInput_.setValue(std::to_string(cfg.wsjtxPort));
  rbnEnabled_ = cfg.rbnEnabled;
  pskOfDe_ = cfg.liveSpotsOfDe;
  pskUseCall_ = cfg.liveSpotsUseCall;
  pskMaxAge_ = cfg.liveSpotsMaxAge;

  rotationInterval_ = cfg.rotationIntervalS;
  syncRotation_ = cfg.syncRotation;
  contestModeActive_ = cfg.contestModeActive;
  watchlistEntries_ = cfg.watchlist;
  ontaFilter_ = cfg.ontaFilter;
  watchlistInputField_.clear();
  watchlistScrollOffset_ = 0;
  theme_ = cfg.theme;
  mapStyle_ = cfg.mapStyle;
  projection_ = cfg.projection;
  displayPowerMethod_ = cfg.displayPowerMethod;
  mapNightLights_ = cfg.mapNightLights;
  useMetric_ = cfg.useMetric;
  callsignColor_ = cfg.callsignColor;
  panelMode_ = cfg.panelMode;
  selectedSatellite_ = cfg.selectedSatellite;
  rssEnabled_ = cfg.rssEnabled;
  showBorders_ = cfg.showBorders;
  weatherOverlay_ = cfg.weatherOverlay;

  qrzUsernameInput_.setValue(cfg.qrzUsername);
  qrzPasswordInput_.setValue(cfg.qrzPassword);
  repeaterBookInput_.setValue(cfg.repeaterBookKey);
  winlinkInput_.setValue(cfg.winlinkKey);
  countdownLabel_ = cfg.countdownLabel;
  countdownTime_ = cfg.countdownTime;
  brightnessMgr_.setBrightness(cfg.brightness);
  brightnessMgr_.setScheduleEnabled(cfg.brightnessSchedule);
  brightnessMgr_.setDimTime(cfg.dimHour, cfg.dimMinute);
  brightnessMgr_.setBrightTime(cfg.brightHour, cfg.brightMinute);

  char timeBuf[16];
  std::snprintf(timeBuf, sizeof(timeBuf), "%02d:%02d", cfg.dimHour,
                cfg.dimMinute);
  dimTimeInput_.setValue(timeBuf);
  std::snprintf(timeBuf, sizeof(timeBuf), "%02d:%02d", cfg.brightHour,
                cfg.brightMinute);
  brightTimeInput_.setValue(timeBuf);

  rigHostInput_.setValue(cfg.rigHost);
  rigPortInput_.setValue(std::to_string(cfg.rigPort));
  rigAutoTune_ = cfg.rigAutoTune;

  rotatorHostInput_.setValue(cfg.rotatorHost);
  rotatorPortInput_.setValue(std::to_string(cfg.rotatorPort));
  rotatorAutoTrack_ = cfg.rotatorAutoTrack;
  rotatorUpover_ = cfg.rotatorUpover;

  hubMode_ = cfg.hubMode;
  hubIpInput_.setValue(cfg.hubIp);
  hubPortInput_.setValue(std::to_string(cfg.hubPort));

  paneRotations_[0] = cfg.pane1Rotation;
  paneRotations_[1] = cfg.pane2Rotation;
  paneRotations_[2] = cfg.pane3Rotation;
  paneRotations_[3] = cfg.pane4Rotation;
  paneRotations_[4] = cfg.pane5Rotation;
  paneRotations_[5] = cfg.pane6Rotation;
  {
    bool allScrollable = !cfg.pane5Rotation.empty();
    for (auto t : cfg.pane5Rotation)
      { auto *d = WidgetRegistry::instance().find(t); if (!d || !d->isScrollable) { allScrollable = false; break; } }
    pane5FullHeight_ = cfg.pane6Rotation.empty() && allScrollable;
  }

  colorOverrides_ = cfg.colorOverrides;

  fontPath_ = cfg.fontPath;
  fontListSelected_ = 0;
#ifndef __EMSCRIPTEN__
  systemFonts_ = enumerateSystemFonts();
  if (!fontPath_.empty()) {
    for (int i = 0; i < (int)systemFonts_.size(); ++i) {
      if (systemFonts_[i].second == fontPath_) {
        fontListSelected_ = i + 1; // +1 for built-in entry at index 0
        break;
      }
    }
  }
#endif

  callsignInput_.setCursorToEnd();
}

AppConfig SetupScreen::getConfig(const AppConfig& base) const {
  AppConfig cfg = base;
  cfg.gpsEnabled = gpsEnabled_;
  cfg.audioMuted = audioMuted_;
  cfg.callsign = callsignInput_.getValue();
  cfg.grid = gridInput_.getValue();
  cfg.callsignFrn = frnText_;
  cfg.lat = StringUtils::safe_stod(latInput_.getValue());
  cfg.lon = StringUtils::safe_stod(lonInput_.getValue());
  cfg.dxClusterHost = clusterHostInput_.getValue();
  cfg.dxClusterPort = StringUtils::safe_stoi(clusterPortInput_.getValue());
  if (cfg.dxClusterPort == 0)
    cfg.dxClusterPort = 7300;
  cfg.dxClusterLogin = clusterLoginInput_.getValue();
  cfg.dxClusterEnabled = clusterEnabled_;
  cfg.dxClusterUseWSJTX = clusterWSJTX_;
  cfg.wsjtxPort = StringUtils::safe_stoi(wsjtxPortInput_.getValue());
  if (cfg.wsjtxPort == 0)
    cfg.wsjtxPort = 2237;
  cfg.rbnEnabled = rbnEnabled_;
  cfg.liveSpotsOfDe = pskOfDe_;
  cfg.liveSpotsUseCall = pskUseCall_;
  cfg.liveSpotsMaxAge = pskMaxAge_;

  cfg.rotationIntervalS = rotationInterval_;
  cfg.syncRotation = syncRotation_;
  cfg.watchlist = watchlistEntries_;
  cfg.ontaFilter = ontaFilter_;
  cfg.theme = theme_;
  cfg.mapStyle = mapStyle_;
  cfg.projection = projection_;
  cfg.displayPowerMethod = displayPowerMethod_;
  cfg.mapNightLights = mapNightLights_;
  cfg.useMetric = useMetric_;
  cfg.rssEnabled = rssEnabled_;
  cfg.showBorders = showBorders_;
  cfg.weatherOverlay = weatherOverlay_;
  cfg.callsignColor = callsignColor_;
  cfg.panelMode = panelMode_;
  cfg.selectedSatellite = selectedSatellite_;

  cfg.qrzUsername = qrzUsernameInput_.getValue();
  cfg.qrzPassword = qrzPasswordInput_.getValue();
  cfg.repeaterBookKey = repeaterBookInput_.getValue();
  cfg.winlinkKey = winlinkInput_.getValue();
  cfg.countdownLabel = countdownLabel_;
  cfg.countdownTime = countdownTime_;
  cfg.brightness = brightnessMgr_.getBrightness();
  cfg.brightnessSchedule = brightnessMgr_.isScheduleEnabled();

  int dh, dm, bh, bm;
  if (std::sscanf(dimTimeInput_.getValue().c_str(), "%d:%d", &dh, &dm) == 2) {
    cfg.dimHour = dh;
    cfg.dimMinute = dm;
  }
  if (std::sscanf(brightTimeInput_.getValue().c_str(), "%d:%d", &bh, &bm) ==
      2) {
    cfg.brightHour = bh;
    cfg.brightMinute = bm;
  }

  cfg.pane1Rotation = paneRotations_[0];
  cfg.pane2Rotation = paneRotations_[1];
  cfg.pane3Rotation = paneRotations_[2];
  cfg.pane4Rotation = paneRotations_[3];
  cfg.pane5Rotation = paneRotations_[4];
  cfg.pane6Rotation = paneRotations_[5];
  cfg.contestModeActive = contestModeActive_;

  cfg.rigHost = rigHostInput_.getValue();
  cfg.rigPort = StringUtils::safe_stoi(rigPortInput_.getValue());
  if (cfg.rigPort == 0)
    cfg.rigPort = 4532;
  cfg.rigAutoTune = rigAutoTune_;

  cfg.rotatorHost = rotatorHostInput_.getValue();
  cfg.rotatorPort = StringUtils::safe_stoi(rotatorPortInput_.getValue());
  if (cfg.rotatorPort == 0)
    cfg.rotatorPort = 4533;
  cfg.rotatorAutoTrack = rotatorAutoTrack_;
  cfg.rotatorUpover = rotatorUpover_;

  cfg.hubMode = hubMode_;
  cfg.hubIp = hubIpInput_.getValue();
  cfg.hubPort = StringUtils::safe_stoi(hubPortInput_.getValue());
  if (cfg.hubPort == 0)
    cfg.hubPort = 8080;

  cfg.colorOverrides = colorOverrides_;
  cfg.fontPath = fontPath_;

  return cfg;
}

std::vector<std::string> SetupScreen::getActions() const {
  return {"tab_identity",
          "tab_dxcluster",
          "tab_appearance",
          "tab_widgets",
          "field_0",
          "field_1",
          "field_2",
          "field_3",
          "toggle_night_lights",
          "toggle_metric",
          "toggle_rss",
          "toggle_borders",
          "toggle_weather_overlay",
          "toggle_map_style",
          "done",
          "cancel"};
}

SDL_Rect SetupScreen::getActionRect(const std::string &action) const {
  auto *cat = fontMgr_.catalog();
  if (!cat)
    return {0, 0, 0, 0};
  int pad = 20;
  int fieldW = modalRect_.w - 2 * pad;
  int fieldX = modalRect_.x + pad;
  int fieldH = cat->ptSize(FontStyle::SmallRegular) + 14;
  int titleShift = cat->ptSize(FontStyle::MediumBold) + pad / 2;
  int tabY = modalRect_.y + titleShift + pad / 2;
  int numTabs = 9;
  int tabW = fieldW / numTabs;

  if (action == "tab_identity")
    return {fieldX, tabY, tabW, fieldH};
  if (action == "tab_dxcluster")
    return {fieldX + tabW, tabY, tabW, fieldH};
  if (action == "tab_appearance")
    return {fieldX + 2 * tabW, tabY, tabW, fieldH};
  if (action == "tab_display")
    return {fieldX + 3 * tabW, tabY, tabW, fieldH};
  if (action == "tab_rig")
    return {fieldX + 4 * tabW, tabY, tabW, fieldH};
  if (action == "tab_services")
    return {fieldX + 5 * tabW, tabY, tabW, fieldH};
  if (action == "tab_widgets")
    return {fieldX + 6 * tabW, tabY, tabW, fieldH};

  // Fields (approximate positions)
  int yStart = modalRect_.y + titleShift + 2 * pad + fieldH;
  if (action.find("field_") == 0) {
    int idx = StringUtils::safe_stoi(action.substr(6));
    int fy =
        yStart + idx * (cat->ptSize(FontStyle::SmallBold) + fieldH + pad / 2);
    return {fieldX, fy, fieldW, fieldH};
  }

  if (action == "toggle_night_lights") {
    return nightLightsRect_;
  }
  if (action == "toggle_metric")
    return metricToggleRect_;
  if (action == "toggle_rss")
    return rssToggleRect_;
  if (action == "toggle_borders")
    return bordersToggleRect_;
  if (action == "toggle_weather_overlay")
    return weatherOverlayRect_;
  if (action == "toggle_map_style")
    return mapStyleRect_;

  if (action == "done") {
    return okBtnRect_;
  }
  if (action == "cancel") {
    return cancelBtnRect_;
  }

  return {0, 0, 0, 0};
}
