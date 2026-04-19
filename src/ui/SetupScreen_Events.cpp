#include "SetupScreen.h"
#include "WidgetRegistry.h"
#include "../core/ContestModeManager.h"
#include "../core/DisplayPower.h"
#include "../core/StringUtils.h"
#include "../services/UpdateChecker.h"
#include <SDL.h>
#include <algorithm>

bool SetupScreen::onMouseDown(int mx, int my, Uint16 mod, int clicks) {
  if (themeCustomizer_ && themeCustomizer_->isActive()) {
    return themeCustomizer_->onMouseDown(mx, my, mod, clicks);
  }

  if (tzModalOpen_) {
    if (mx >= tzModalOkRect_.x && mx < tzModalOkRect_.x + tzModalOkRect_.w &&
        my >= tzModalOkRect_.y && my < tzModalOkRect_.y + tzModalOkRect_.h) {
      if (tzModalCustom_) {
        defaultTzOffset_ = std::clamp(StringUtils::safe_stoi(tzCustomOffsetInput_.getValue()), -12, 14);
        defaultTzLabel_ = tzCustomLabelInput_.getValue();
        if (defaultTzLabel_.empty()) defaultTzLabel_ = "UTC";
      } else {
        defaultTzOffset_ = kTzPresets[tzModalSelected_].offset;
        defaultTzLabel_ = kTzPresets[tzModalSelected_].label;
      }
      tzModalOpen_ = false;
      return true;
    }
    if (mx >= tzModalCancelRect_.x && mx < tzModalCancelRect_.x + tzModalCancelRect_.w &&
        my >= tzModalCancelRect_.y && my < tzModalCancelRect_.y + tzModalCancelRect_.h) {
      tzModalOpen_ = false;
      return true;
    }
    for (int i = 0; i <= kNumTzPresets; ++i) {
      if (mx >= tzModalRowRects_[i].x && mx < tzModalRowRects_[i].x + tzModalRowRects_[i].w &&
          my >= tzModalRowRects_[i].y && my < tzModalRowRects_[i].y + tzModalRowRects_[i].h) {
        if (i < kNumTzPresets) {
          tzModalSelected_ = i;
          tzModalCustom_ = false;
        } else {
          tzModalCustom_ = true;
          tzCustomOffsetActive_ = true;
          tzCustomOffsetInput_.setActive(true);
        }
        return true;
      }
    }
    if (tzModalCustom_) {
      if (mx >= tzModalCustomOffsetRect_.x && mx < tzModalCustomOffsetRect_.x + tzModalCustomOffsetRect_.w &&
          my >= tzModalCustomOffsetRect_.y && my < tzModalCustomOffsetRect_.y + tzModalCustomOffsetRect_.h) {
        tzCustomOffsetActive_ = true;
        tzCustomOffsetInput_.setActive(true);
        tzCustomLabelInput_.setActive(false);
        return true;
      }
      if (mx >= tzModalCustomLabelRect_.x && mx < tzModalCustomLabelRect_.x + tzModalCustomLabelRect_.w &&
          my >= tzModalCustomLabelRect_.y && my < tzModalCustomLabelRect_.y + tzModalCustomLabelRect_.h) {
        tzCustomOffsetActive_ = false;
        tzCustomOffsetInput_.setActive(false);
        tzCustomLabelInput_.setActive(true);
        return true;
      }
    }
    if (mx < tzModalRect_.x || mx >= tzModalRect_.x + tzModalRect_.w ||
        my < tzModalRect_.y || my >= tzModalRect_.y + tzModalRect_.h) {
      tzModalOpen_ = false;
    }
    return true;
  }

#ifndef __EMSCRIPTEN__
  if (fontModalOpen_) {
    if (fontModalOkRect_.w > 0 &&
        mx >= fontModalOkRect_.x && mx < fontModalOkRect_.x + fontModalOkRect_.w &&
        my >= fontModalOkRect_.y && my < fontModalOkRect_.y + fontModalOkRect_.h) {
      fontListSelected_ = fontModalSelected_;
      fontPath_ = (fontListSelected_ == 0) ? "" : systemFonts_[fontListSelected_ - 1].second;
      fontModalOpen_ = false;
      return true;
    }
    if (fontModalCancelRect_.w > 0 &&
        mx >= fontModalCancelRect_.x && mx < fontModalCancelRect_.x + fontModalCancelRect_.w &&
        my >= fontModalCancelRect_.y && my < fontModalCancelRect_.y + fontModalCancelRect_.h) {
      fontModalOpen_ = false;
      return true;
    }
    if (fontModalListRect_.w > 0 &&
        mx >= fontModalListRect_.x && mx < fontModalListRect_.x + fontModalListRect_.w &&
        my >= fontModalListRect_.y && my < fontModalListRect_.y + fontModalListRect_.h) {
      auto *lcat = fontMgr_.catalog();
      int rowH = lcat->ptSize(FontStyle::SmallRegular) + 10;
      int row = (my - fontModalListRect_.y) / rowH;
      int idx = fontModalScroll_ + row;
      if (idx >= 0 && idx < (int)fontModalFiltered_.size()) {
        int fontIdx = fontModalFiltered_[idx];
        fontModalSelected_ = (fontIdx == -1) ? 0 : fontIdx + 1;
        if (clicks == 2) {
          fontListSelected_ = fontModalSelected_;
          fontPath_ = (fontListSelected_ == 0) ? "" : systemFonts_[fontListSelected_ - 1].second;
          fontModalOpen_ = false;
        }
      }
      return true;
    }
    if (mx < fontModalRect_.x || mx >= fontModalRect_.x + fontModalRect_.w ||
        my < fontModalRect_.y || my >= fontModalRect_.y + fontModalRect_.h) {
      fontModalOpen_ = false;
    }
    return true;
  }
#endif

  auto *cat = fontMgr_.catalog();
  int pad = 20;
  int fieldW = modalRect_.w - 2 * pad;
  int fieldX = modalRect_.x + pad;
  int fieldH = cat->ptSize(FontStyle::SmallRegular) + 14;
  int textPad = 7;

  // 1. Tab Switching (Highest Priority)
  int yTabs = modalRect_.y + cat->ptSize(FontStyle::MediumBold) + 3 * pad / 2;
  if (my >= yTabs && my <= yTabs + fieldH) {
    const Tab tabValues[] = {Tab::Identity, Tab::Spotting,  Tab::Appearance,
                             Tab::Rig,      Tab::Services,  Tab::Network,
                             Tab::Widgets,  Tab::Watchlist, Tab::Update};
    int numTabs = 9;
    int tabW = fieldW / numTabs;
    for (int i = 0; i < numTabs; ++i) {
      if (mx >= fieldX + i * tabW && mx <= fieldX + (i + 1) * tabW) {
        activeTab_ = tabValues[i];
        activeField_ = 0;
        TextInput *ti = getActiveInput();
        if (ti) {
          ti->setActive(true);
          ti->setCursorToEnd();
        }
        return true;
      }
    }
  }

  // Helper for field hit-testing
  auto hitField = [&](SDL_Rect &r, int idx, TextInput *ti) -> bool {
    if (r.w > 0 && mx >= r.x && mx < r.x + r.w && my >= r.y && my < r.y + r.h) {
      activeField_ = idx;
      if (ti) {
        ti->setActive(true);
        ti->onMouseDown(mx, my, clicks, fontMgr_, r.x, r.y, r.w, r.h,
                        FontStyle::SmallRegular, textPad);
#ifdef __ANDROID__
        SDL_StopTextInput();  // reset IME composition buffer before activating new field
        SDL_StartTextInput();
#endif
      }
      return true;
    }
    return false;
  };

  // 2. Tab-Specific Field Interaction
  if (activeTab_ == Tab::Identity) {
    if (hitField(callsignRect_, 0, &callsignInput_))
      return true;
    if (hitField(gridRect_, 1, &gridInput_))
      return true;
    if (hitField(latRect_, 2, &latInput_))
      return true;
    if (hitField(lonRect_, 3, &lonInput_))
      return true;
  } else if (activeTab_ == Tab::Spotting) {
    if (hitField(clusterHostRect_, 0, &clusterHostInput_))
      return true;
    if (hitField(clusterPortRect_, 1, &clusterPortInput_))
      return true;
    if (hitField(clusterLoginRect_, 2, &clusterLoginInput_))
      return true;
    if (clusterWSJTX_ && wsjtxPortRect_.w > 0) {
      if (hitField(wsjtxPortRect_, 3, &wsjtxPortInput_))
        return true;
    }
  } else if (activeTab_ == Tab::Rig) {
    if (hitField(rigHostRect_, 0, &rigHostInput_))
      return true;
    if (hitField(rigPortRect_, 1, &rigPortInput_))
      return true;
    if (hitField(rotatorHostRect_, 2, &rotatorHostInput_))
      return true;
    if (hitField(rotatorPortRect_, 3, &rotatorPortInput_))
      return true;

    // Toggles
    if (mx >= toggleRect_.x && mx < toggleRect_.x + toggleRect_.w &&
        my >= toggleRect_.y && my < toggleRect_.y + toggleRect_.h) {
      rigAutoTune_ = !rigAutoTune_;
      return true;
    }
    if (mx >= rotatorAutoTrackRect_.x &&
        mx < rotatorAutoTrackRect_.x + rotatorAutoTrackRect_.w &&
        my >= rotatorAutoTrackRect_.y &&
        my < rotatorAutoTrackRect_.y + rotatorAutoTrackRect_.h) {
      rotatorAutoTrack_ = !rotatorAutoTrack_;
      return true;
    }
    if (mx >= rotatorUpoverRect_.x &&
        mx < rotatorUpoverRect_.x + rotatorUpoverRect_.w &&
        my >= rotatorUpoverRect_.y &&
        my < rotatorUpoverRect_.y + rotatorUpoverRect_.h) {
      rotatorUpover_ = !rotatorUpover_;
      return true;
    }
  } else if (activeTab_ == Tab::Services) {
    if (hitField(qrzUsernameRect_, 0, &qrzUsernameInput_))
      return true;
    if (hitField(qrzPasswordRect_, 1, &qrzPasswordInput_))
      return true;
    if (hitField(repeaterBookRect_, 2, &repeaterBookInput_))
      return true;
    if (hitField(winlinkRect_, 3, &winlinkInput_))
      return true;
  } else if (activeTab_ == Tab::Network && hubMode_ == HubMode::Client) {
    if (hitField(hubIpRect_, 0, &hubIpInput_))
      return true;
    if (hitField(hubPortRect_, 1, &hubPortInput_))
      return true;
  } else if (activeTab_ == Tab::Appearance) {
    if (hitField(dimTimeRect_, 0, &dimTimeInput_))
      return true;
    if (hitField(brightTimeRect_, 1, &brightTimeInput_))
      return true;
  } else if (activeTab_ == Tab::Widgets) {
    // 1. Pane Switching via layout diagram (panes 1-4 top bar + 5-6 side panel)
    for (int i = 0; i < 6; ++i) {
      const auto &pr = paneDiagramRects_[i];
      if (pr.w > 0 && mx >= pr.x && mx < pr.x + pr.w && my >= pr.y && my < pr.y + pr.h) {
        if (activePane_ != i) {
          activePane_ = i;
          widgetListScrollOffset_ = 0;
        }
        return true;
      }
    }

    // 1b. "Full Height" checkbox for pane 5 and 6
    if ((activePane_ == 4 || activePane_ == 5) && fullHeightCheckRect_.w > 0 &&
        mx >= fullHeightCheckRect_.x && mx < fullHeightCheckRect_.x + fullHeightCheckRect_.w &&
        my >= fullHeightCheckRect_.y && my < fullHeightCheckRect_.y + fullHeightCheckRect_.h) {
      pane5FullHeight_ = !pane5FullHeight_;
      if (pane5FullHeight_) {
        activePane_ = 4; // switch to pane 5 since full height widgets reside there
        paneRotations_[5].clear();
        auto &p5 = paneRotations_[4];
        p5.erase(std::remove_if(p5.begin(), p5.end(),
                                [](const std::string &t) {
                                  auto *d = WidgetRegistry::instance().find(t);
                                  return !d || !d->isScrollable;
                                }),
                 p5.end());
      }
      widgetListScrollOffset_ = 0;
      return true;
    }

    // 2. Sync Rotation Toggle
    if (mx >= syncRotationRect_.x &&
        mx < syncRotationRect_.x + syncRotationRect_.w &&
        my >= syncRotationRect_.y &&
        my < syncRotationRect_.y + syncRotationRect_.h) {
      syncRotation_ = !syncRotation_;
      return true;
    }

    // 3. Widget Selection (only within visible list area)
    if (my >= widgetListStartY_ && my < widgetListEndY_) {
      for (auto &wr : widgetRects_) {
        if (mx >= wr.rect.x && mx < wr.rect.x + wr.rect.w && my >= wr.rect.y &&
            my < wr.rect.y + wr.rect.h) {
          auto &cur = paneRotations_[activePane_];
          auto it = std::find(cur.begin(), cur.end(), wr.type);
          if (it != cur.end()) {
            if (cur.size() > 1)
              cur.erase(it);
          } else {
            cur.push_back(wr.type);
          }
          return true;
        }
      }
    }

    // 4. Rotation Interval
    if (mx >= rotationToggleRect_.x &&
        mx < rotationToggleRect_.x + rotationToggleRect_.w &&
        my >= rotationToggleRect_.y &&
        my < rotationToggleRect_.y + rotationToggleRect_.h) {
      activeField_ = 0;
      rotationInterval_ = 0;  // Clear for fresh input
      return true;
    }

    // 5. Contest Mode Toggle
    if (contestModeBtn_.w > 0 &&
        mx >= contestModeBtn_.x && mx < contestModeBtn_.x + contestModeBtn_.w &&
        my >= contestModeBtn_.y && my < contestModeBtn_.y + contestModeBtn_.h) {
      if (!contestModeActive_) {
        ContestModeManager::activate(paneRotations_, contestSavedRotations_);
        contestModeActive_ = true;
      } else {
        ContestModeManager::deactivate(paneRotations_, contestSavedRotations_);
        contestModeActive_ = false;
      }
      return true;
    }

  } else if (activeTab_ == Tab::Watchlist) {
    // Input field focus
    if (hitField(watchlistInputRect_, 0, &watchlistInputField_))
      return true;

    // Add button
    if (watchlistAddRect_.w > 0 &&
        mx >= watchlistAddRect_.x && mx < watchlistAddRect_.x + watchlistAddRect_.w &&
        my >= watchlistAddRect_.y && my < watchlistAddRect_.y + watchlistAddRect_.h) {
      addWatchlistEntriesFromInput();
      return true;
    }

    // Delete buttons
    for (int i = 0; i < (int)watchlistDeleteRects_.size(); ++i) {
      const SDL_Rect &dr = watchlistDeleteRects_[i];
      if (mx >= dr.x && mx < dr.x + dr.w && my >= dr.y && my < dr.y + dr.h) {
        int realIdx = watchlistScrollOffset_ + i;
        if (realIdx >= 0 && realIdx < (int)watchlistEntries_.size()) {
          watchlistEntries_.erase(watchlistEntries_.begin() + realIdx);
          if (watchlistScrollOffset_ > 0 &&
              watchlistScrollOffset_ >= (int)watchlistEntries_.size())
            --watchlistScrollOffset_;
        }
        return true;
      }
    }

    // Scroll arrows
    if (watchlistScrollUpRect_.w > 0 &&
        mx >= watchlistScrollUpRect_.x &&
        mx < watchlistScrollUpRect_.x + watchlistScrollUpRect_.w &&
        my >= watchlistScrollUpRect_.y &&
        my < watchlistScrollUpRect_.y + watchlistScrollUpRect_.h) {
      if (watchlistScrollOffset_ > 0)
        --watchlistScrollOffset_;
      return true;
    }
    if (watchlistScrollDownRect_.w > 0 &&
        mx >= watchlistScrollDownRect_.x &&
        mx < watchlistScrollDownRect_.x + watchlistScrollDownRect_.w &&
        my >= watchlistScrollDownRect_.y &&
        my < watchlistScrollDownRect_.y + watchlistScrollDownRect_.h) {
      int maxVisible = 5; // matches renderTabWatchlist
      if (watchlistScrollOffset_ + maxVisible < (int)watchlistEntries_.size())
        ++watchlistScrollOffset_;
      return true;
    }

    // On The Air filter buttons (ALL / POTA / SOTA)
    static const char *kOntaFilterValues[] = {"all", "pota", "sota"};
    for (int i = 0; i < 3; ++i) {
      const SDL_Rect &r = ontaFilterRects_[i];
      if (r.w > 0 && mx >= r.x && mx < r.x + r.w && my >= r.y && my < r.y + r.h) {
        ontaFilter_ = kOntaFilterValues[i];
        return true;
      }
    }
  }

  // 3. Toggles and Buttons (Non-text fields)
  if (activeTab_ == Tab::Identity) {
    if (mx >= gpsToggleRect_.x && mx <= gpsToggleRect_.x + gpsToggleRect_.w &&
        my >= gpsToggleRect_.y && my <= gpsToggleRect_.y + gpsToggleRect_.h) {
      gpsEnabled_ = !gpsEnabled_;
      return true;
    }
    if (mx >= audioMuteToggleRect_.x && mx <= audioMuteToggleRect_.x + audioMuteToggleRect_.w &&
        my >= audioMuteToggleRect_.y && my <= audioMuteToggleRect_.y + audioMuteToggleRect_.h) {
      audioMuted_ = !audioMuted_;
      return true;
    }
    if (mx >= defaultTzRect_.x && mx < defaultTzRect_.x + defaultTzRect_.w &&
        my >= defaultTzRect_.y && my < defaultTzRect_.y + defaultTzRect_.h) {
      tzModalOpen_ = true;
      // Sync temp modal state
      tzModalCustom_ = true;
      tzModalSelected_ = 0;
      for (int i = 0; i < kNumTzPresets; ++i) {
        if (kTzPresets[i].offset == defaultTzOffset_ && defaultTzLabel_ == kTzPresets[i].label) {
          tzModalSelected_ = i;
          tzModalCustom_ = false;
          break;
        }
      }
      if (tzModalCustom_) {
        tzCustomOffsetInput_.setValue(std::to_string(defaultTzOffset_ == 999 ? 0 : defaultTzOffset_));
        tzCustomLabelInput_.setValue(defaultTzLabel_);
      }
      return true;
    }
  } else if (activeTab_ == Tab::Spotting) {
    if (mx >= clusterToggleRect_.x &&
        mx <= clusterToggleRect_.x + clusterToggleRect_.w &&
        my >= clusterToggleRect_.y &&
        my <= clusterToggleRect_.y + clusterToggleRect_.h) {
      clusterEnabled_ = !clusterEnabled_;
      return true;
    }
    if (mx >= toggleRect_.x && mx <= toggleRect_.x + toggleRect_.w &&
        my >= toggleRect_.y && my <= toggleRect_.y + toggleRect_.h) {
      clusterWSJTX_ = !clusterWSJTX_;
      return true;
    }
    if (mx >= clusterHideDuplicatesRect_.x &&
        mx <= clusterHideDuplicatesRect_.x + clusterHideDuplicatesRect_.w &&
        my >= clusterHideDuplicatesRect_.y &&
        my <= clusterHideDuplicatesRect_.y + clusterHideDuplicatesRect_.h) {
      clusterHideDuplicates_ = !clusterHideDuplicates_;
      return true;
    }
    {
      static constexpr int kAgeChoices[4] = {10, 20, 40, 60};
      for (int i = 0; i < 4; i++) {
        if (mx >= clusterAgeRects_[i].x &&
            mx <= clusterAgeRects_[i].x + clusterAgeRects_[i].w &&
            my >= clusterAgeRects_[i].y &&
            my <= clusterAgeRects_[i].y + clusterAgeRects_[i].h) {
          clusterMaxAgeMinutes_ = kAgeChoices[i];
          return true;
        }
      }
    }
    if (mx >= rbnToggleRect_.x && mx <= rbnToggleRect_.x + rbnToggleRect_.w &&
        my >= rbnToggleRect_.y && my <= rbnToggleRect_.y + rbnToggleRect_.h) {
      rbnEnabled_ = !rbnEnabled_;
      return true;
    }
  } else if (activeTab_ == Tab::Network) {
    if (mx >= hubModeRect_.x && mx <= hubModeRect_.x + hubModeRect_.w &&
        my >= hubModeRect_.y && my <= hubModeRect_.y + hubModeRect_.h) {
      if (hubMode_ == HubMode::Off)
        hubMode_ = HubMode::Master;
      else if (hubMode_ == HubMode::Master)
        hubMode_ = HubMode::Client;
      else
        hubMode_ = HubMode::Off;
      activeField_ = 0;
      return true;
    }
  } else if (activeTab_ == Tab::Appearance) {
    if (mx >= themeRect_.x && mx < themeRect_.x + themeRect_.w &&
        my >= themeRect_.y && my < themeRect_.y + themeRect_.h) {
      auto themes = getAvailableThemes();
      auto it = std::find(themes.begin(), themes.end(), theme_);
      size_t idx = (it != themes.end()) ? std::distance(themes.begin(), it) : 0;
      idx = (idx + 1) % themes.size();
      theme_ = themes[idx];
      if (theme_ != "custom") {
        colorOverrides_.clear();
      }
      return true;
    }
    if (mx >= customizeBtnRect_.x &&
        mx < customizeBtnRect_.x + customizeBtnRect_.w &&
        my >= customizeBtnRect_.y &&
        my < customizeBtnRect_.y + customizeBtnRect_.h) {
      themeCustomizer_->setActive(true);
      return true;
    }
    if (mx >= nightLightsRect_.x &&
        mx < nightLightsRect_.x + nightLightsRect_.w &&
        my >= nightLightsRect_.y &&
        my < nightLightsRect_.y + nightLightsRect_.h) {
      mapNightLights_ = !mapNightLights_;
      return true;
    }
    if (mx >= metricToggleRect_.x &&
        mx < metricToggleRect_.x + metricToggleRect_.w &&
        my >= metricToggleRect_.y &&
        my < metricToggleRect_.y + metricToggleRect_.h) {
      useMetric_ = !useMetric_;
      return true;
    }
    if (mx >= brightnessSliderRect_.x &&
        mx < brightnessSliderRect_.x + brightnessSliderRect_.w &&
        my >= brightnessSliderRect_.y &&
        my < brightnessSliderRect_.y + brightnessSliderRect_.h) {
      int val = (mx - brightnessSliderRect_.x) * 100 / brightnessSliderRect_.w;
      brightnessMgr_.setBrightness(val);
      return true;
    }
    if (mx >= scheduleToggleRect_.x &&
        mx < scheduleToggleRect_.x + scheduleToggleRect_.w &&
        my >= scheduleToggleRect_.y &&
        my < scheduleToggleRect_.y + scheduleToggleRect_.h) {
      brightnessMgr_.setScheduleEnabled(!brightnessMgr_.isScheduleEnabled());
      return true;
    }
    if (mx >= powerMethodRect_.x && mx < powerMethodRect_.x + powerMethodRect_.w &&
        my >= powerMethodRect_.y && my < powerMethodRect_.y + powerMethodRect_.h) {
      std::vector<DisplayPower::Method> methods = {DisplayPower::Method::NONE};
      if (displayPower_) {
        const auto &available = displayPower_->getMethods();
        methods.insert(methods.end(), available.begin(), available.end());
      }

      auto current = DisplayPower::stringToMethod(displayPowerMethod_);
      auto it = std::find(methods.begin(), methods.end(), current);
      size_t idx = (it == methods.end()) ? 0 : std::distance(methods.begin(), it);
      idx = (idx + 1) % methods.size();
      displayPowerMethod_ = DisplayPower::methodToString(methods[idx]);
      return true;
    }
#ifndef __EMSCRIPTEN__
    if (fontListRect_.w > 0 &&
        mx >= fontListRect_.x && mx < fontListRect_.x + fontListRect_.w &&
        my >= fontListRect_.y && my < fontListRect_.y + fontListRect_.h) {
      fontModalSelected_ = fontListSelected_;
      fontModalFilter_.clear();
      fontModalScroll_ = std::max(0, fontListSelected_ - 4);
      fontModalOpen_ = true;
      return true;
    }
#endif
  } else if (activeTab_ == Tab::Rig) {
    if (mx >= toggleRect_.x && mx < toggleRect_.x + toggleRect_.w &&
        my >= toggleRect_.y && my < toggleRect_.y + toggleRect_.h) {
      rigAutoTune_ = !rigAutoTune_;
      return true;
    }
    if (mx >= rotatorAutoTrackRect_.x &&
        mx < rotatorAutoTrackRect_.x + rotatorAutoTrackRect_.w &&
        my >= rotatorAutoTrackRect_.y &&
        my < rotatorAutoTrackRect_.y + rotatorAutoTrackRect_.h) {
      rotatorAutoTrack_ = !rotatorAutoTrack_;
      return true;
    }
    if (mx >= rotatorUpoverRect_.x &&
        mx < rotatorUpoverRect_.x + rotatorUpoverRect_.w &&
        my >= rotatorUpoverRect_.y &&
        my < rotatorUpoverRect_.y + rotatorUpoverRect_.h) {
      rotatorUpover_ = !rotatorUpover_;
      return true;
    }
  }

  // 4. Footer Buttons
  if (mx >= cancelBtnRect_.x && mx <= cancelBtnRect_.x + cancelBtnRect_.w &&
      my >= cancelBtnRect_.y && my <= cancelBtnRect_.y + cancelBtnRect_.h) {
    complete_ = true;
    cancelled_ = true;
    return true;
  }
  if (mx >= okBtnRect_.x && mx <= okBtnRect_.x + okBtnRect_.w &&
      my >= okBtnRect_.y && my <= okBtnRect_.y + okBtnRect_.h) {
    if (!callsignInput_.getValue().empty() && gridValid_) {
      complete_ = true;
    }
    return true;
  }

  return false;
}

bool SetupScreen::onMouseUp(int mx, int my, Uint16 mod, int clicks) {
  if (themeCustomizer_ && themeCustomizer_->isActive()) {
    return themeCustomizer_->onMouseUp(mx, my, mod, clicks);
  }

  // End any drag selection in the active text field
  if (TextInput *ti = getActiveInput())
    ti->onMouseUp();

  // If clicked outside modal, cancel (or just dismiss keyboard on Android)
  if (mx < modalRect_.x || mx >= modalRect_.x + modalRect_.w ||
      my < modalRect_.y || my >= modalRect_.y + modalRect_.h) {
#ifdef __ANDROID__
    if (SDL_IsTextInputActive()) {
      SDL_StopTextInput();
      return true; // consume tap — dismiss keyboard, don't close settings
    }
#endif
    cancelled_ = true;
    complete_ = true;
    return true;
  }

  // Update tab — download button
  if (activeTab_ == Tab::Update && updateChecker_ &&
      downloadBtnRect_.w > 0 &&
      mx >= downloadBtnRect_.x && mx < downloadBtnRect_.x + downloadBtnRect_.w &&
      my >= downloadBtnRect_.y && my < downloadBtnRect_.y + downloadBtnRect_.h) {
    updateChecker_->startDownload();
    return true;
  }

  return true;
}

bool SetupScreen::onMouseWheel(int scrollY) {
  if (activeTab_ == Tab::Widgets && widgetListMaxScroll_ > 0) {
    widgetListScrollOffset_ = std::max(
        0, std::min(widgetListScrollOffset_ - scrollY, widgetListMaxScroll_));
    return true;
  }
#ifndef __EMSCRIPTEN__
  if (fontModalOpen_ && fontModalListRect_.h > 0) {
    auto *cat = fontMgr_.catalog();
    int rowH = cat->ptSize(FontStyle::SmallRegular) + 10;
    int visibleRows = std::max(1, fontModalListRect_.h / rowH);
    int maxScroll = std::max(0, (int)fontModalFiltered_.size() - visibleRows);
    fontModalScroll_ = std::max(0, std::min(fontModalScroll_ - scrollY, maxScroll));
    return true;
  }
#endif
  return false;
}

void SetupScreen::onMouseMove(int mx, int /*my*/) {
  TextInput *ti = getActiveInput();
  if (!ti)
    return;

  // Resolve the stored rect for the active field so we can pass field bounds
  SDL_Rect r = {0, 0, 0, 0};
  if (activeTab_ == Tab::Identity) {
    const SDL_Rect *rects[] = {&callsignRect_, &gridRect_, &latRect_,
                               &lonRect_};
    if (activeField_ >= 0 && activeField_ < 4)
      r = *rects[activeField_];
  } else if (activeTab_ == Tab::Spotting) {
    const SDL_Rect *rects[] = {&clusterHostRect_, &clusterPortRect_,
                               &clusterLoginRect_, &wsjtxPortRect_};
    if (activeField_ >= 0 && activeField_ < 4)
      r = *rects[activeField_];
  } else if (activeTab_ == Tab::Rig) {
    const SDL_Rect *rects[] = {&rigHostRect_, &rigPortRect_, &rotatorHostRect_,
                               &rotatorPortRect_};
    if (activeField_ >= 0 && activeField_ < 4)
      r = *rects[activeField_];
  } else if (activeTab_ == Tab::Services) {
    // Both Username and Password use full fieldW in render pass
    int pad = 20;
    int fieldW = modalRect_.w - 2 * pad;
    int fieldX = modalRect_.x + pad;
    r = {fieldX, 0, fieldW, 0}; // Y and H not strictly needed for onMouseMove
  } else if (activeTab_ == Tab::Network) {
    const SDL_Rect *rects[] = {&hubIpRect_, &hubPortRect_};
    if (activeField_ >= 0 && activeField_ < 2)
      r = *rects[activeField_];
  } else if (activeTab_ == Tab::Appearance) {
    const SDL_Rect *rects[] = {&dimTimeRect_, &brightTimeRect_};
    if (activeField_ == 0 || activeField_ == 1)
      r = *rects[activeField_];
  }

  if (r.w > 0) {
    int textPad = 7;
    ti->onMouseMove(mx, fontMgr_, r.x, r.w, FontStyle::SmallRegular, textPad);
  }
}

bool SetupScreen::onKeyDown(SDL_Keycode key, Uint16 mod) {
#ifndef __EMSCRIPTEN__
  if (fontModalOpen_) {
    auto *cat = fontMgr_.catalog();
    int rowH = cat->ptSize(FontStyle::SmallRegular) + 10;
    int visibleRows = (fontModalListRect_.h > 0) ? fontModalListRect_.h / rowH : 8;
    int maxScroll = std::max(0, (int)fontModalFiltered_.size() - visibleRows);
    switch (key) {
    case SDLK_ESCAPE:
      fontModalOpen_ = false;
      return true;
    case SDLK_RETURN:
    case SDLK_KP_ENTER:
      fontListSelected_ = fontModalSelected_;
      fontPath_ = (fontListSelected_ == 0) ? "" : systemFonts_[fontListSelected_ - 1].second;
      fontModalOpen_ = false;
      return true;
    case SDLK_UP: {
      int pos = -1;
      for (int i = 0; i < (int)fontModalFiltered_.size(); ++i) {
        int s = (fontModalFiltered_[i] == -1) ? 0 : fontModalFiltered_[i] + 1;
        if (s == fontModalSelected_) { pos = i; break; }
      }
      if (pos > 0) {
        --pos;
        int fi = fontModalFiltered_[pos];
        fontModalSelected_ = (fi == -1) ? 0 : fi + 1;
        if (pos < fontModalScroll_) fontModalScroll_ = pos;
      }
      return true;
    }
    case SDLK_DOWN: {
      int pos = -1;
      for (int i = 0; i < (int)fontModalFiltered_.size(); ++i) {
        int s = (fontModalFiltered_[i] == -1) ? 0 : fontModalFiltered_[i] + 1;
        if (s == fontModalSelected_) { pos = i; break; }
      }
      if (pos >= 0 && pos < (int)fontModalFiltered_.size() - 1) {
        ++pos;
        int fi = fontModalFiltered_[pos];
        fontModalSelected_ = (fi == -1) ? 0 : fi + 1;
        if (pos >= fontModalScroll_ + visibleRows)
          fontModalScroll_ = pos - visibleRows + 1;
      }
      return true;
    }
    case SDLK_HOME:
      if (!fontModalFiltered_.empty()) {
        int fi = fontModalFiltered_[0];
        fontModalSelected_ = (fi == -1) ? 0 : fi + 1;
        fontModalScroll_ = 0;
      }
      return true;
    case SDLK_END:
      if (!fontModalFiltered_.empty()) {
        int last = (int)fontModalFiltered_.size() - 1;
        int fi = fontModalFiltered_[last];
        fontModalSelected_ = (fi == -1) ? 0 : fi + 1;
        fontModalScroll_ = std::max(0, last - visibleRows + 1);
      }
      return true;
    case SDLK_PAGEUP:
      fontModalScroll_ = std::max(0, fontModalScroll_ - visibleRows);
      return true;
    case SDLK_PAGEDOWN:
      fontModalScroll_ = std::min(maxScroll, fontModalScroll_ + visibleRows);
      return true;
    case SDLK_BACKSPACE:
      if (!fontModalFilter_.empty()) {
        fontModalFilter_.pop_back();
        fontModalScroll_ = 0;
      }
      return true;
    default:
      return true;
    }
  }
#endif

  if (tzModalOpen_) {
    if (key == SDLK_ESCAPE) {
      tzModalOpen_ = false;
      return true;
    }
    if (tzModalCustom_) {
      if (key == SDLK_TAB) {
        tzCustomOffsetActive_ = !tzCustomOffsetActive_;
        tzCustomOffsetInput_.setActive(tzCustomOffsetActive_);
        tzCustomLabelInput_.setActive(!tzCustomOffsetActive_);
        return true;
      }
      if (tzCustomOffsetActive_) return tzCustomOffsetInput_.onKeyDown(key, mod);
      return tzCustomLabelInput_.onKeyDown(key, mod);
    }
    return true;
  }

  int nFields = 1;

  if (activeTab_ == Tab::Identity) {
    nFields = 4;
  } else if (activeTab_ == Tab::Spotting) {
    nFields = clusterWSJTX_ ? 4 : 3;
  } else if (activeTab_ == Tab::Appearance) {
    nFields = 2; // 0=dimTime, 1=brightTime
  } else if (activeTab_ == Tab::Services) {
    nFields = 4;
  } else if (activeTab_ == Tab::Rig) {
    nFields = 4;
  } else if (activeTab_ == Tab::Widgets) {
    nFields = 1; // 0=rotation interval
  } else if (activeTab_ == Tab::Watchlist) {
    nFields = 1;
  }

  // Watchlist: Enter adds the entry
  if (activeTab_ == Tab::Watchlist && activeField_ == 0 &&
      (key == SDLK_RETURN || key == SDLK_KP_ENTER)) {
    addWatchlistEntriesFromInput();
    return true;
  }

  // Rotation interval: handle digit keys directly from KEYDOWN so this works on
  // framebuffer builds where SDL_TEXTINPUT events may not be generated.
  if (activeTab_ == Tab::Widgets && activeField_ == 0) {
    int digit = -1;
    if (key >= SDLK_0 && key <= SDLK_9)
      digit = key - SDLK_0;
    else if (key >= SDLK_KP_1 && key <= SDLK_KP_9)
      digit = key - SDLK_KP_1 + 1;
    else if (key == SDLK_KP_0)
      digit = 0;
    if (digit >= 0) {
      rotationInterval_ = rotationInterval_ * 10 + digit;
      if (rotationInterval_ > 3600)
        rotationInterval_ = 3600;
      return true;
    }
  }

  switch (key) {
  case SDLK_ESCAPE:
    complete_ = true;
    cancelled_ = true;
    return true;
  case SDLK_TAB: {
    activeField_ = (activeField_ + 1) % nFields;
    TextInput *ni = getActiveInput();
    if (ni)
      ni->setCursorToEnd();
    return true;
  }
  case SDLK_RETURN:
  case SDLK_KP_ENTER:
    if (!callsignInput_.getValue().empty() && gridValid_) {
      complete_ = true;
    }
    return true;
  case SDLK_BACKSPACE:
  case SDLK_DELETE: {
    bool isLatLon = (activeTab_ == Tab::Identity &&
                     (activeField_ == 2 || activeField_ == 3));
    bool isRotInterval = (activeTab_ == Tab::Widgets && activeField_ == 0);
    if (isRotInterval) {
      if (key == SDLK_BACKSPACE)
        rotationInterval_ /= 10;
      return true;
    }
    TextInput *ti = getActiveInput();
    if (ti) {
      bool hadContent = ti->hasSelection() ||
                        (key == SDLK_BACKSPACE
                             ? ti->getCursorPos() > 0
                             : ti->getCursorPos() < (int)ti->getValue().size());
      ti->onKeyDown(key, mod);
      if (hadContent && isLatLon)
        latLonManual_ = true;
    }
    return true;
  }
  case SDLK_v:
    if (mod & (KMOD_CTRL | KMOD_GUI)) {
      char *clip = SDL_GetClipboardText();
      if (clip) {
        for (char c : std::string(clip))
          onTextInput(std::string(1, c).c_str());
        SDL_free(clip);
      }
      return true;
    }
    // fall through to default for non-ctrl v
    {
      TextInput *ti = getActiveInput();
      if (ti)
        ti->onKeyDown(key, mod);
    }
    return true;
  default: {
    TextInput *ti = getActiveInput();
    if (ti) {
      if (ti->onKeyDown(key, mod))
        return true;
    }
    return false;
  }
  }
}

bool SetupScreen::onTextInput(const char *inputText) {
#ifndef __EMSCRIPTEN__
  if (fontModalOpen_) {
    for (const char *p = inputText; *p; ++p) {
      if ((unsigned char)*p >= 0x20 && (unsigned char)*p < 0x7f)
        fontModalFilter_ += *p;
    }
    fontModalScroll_ = 0;
    return true;
  }
#endif

  if (tzModalOpen_ && tzModalCustom_) {
    if (tzCustomOffsetActive_) return tzCustomOffsetInput_.onTextInput(inputText);
    return tzCustomLabelInput_.onTextInput(inputText);
  }

  if (activeTab_ == Tab::Widgets) {
    if (activeField_ == 0) {
      // Digits are handled in onKeyDown to support framebuffer builds.
      // Consume TEXTINPUT here to prevent double-counting.
      return true;
    }
  }

  if (activeTab_ == Tab::Appearance) {
    // Fields 0 & 1: dimTime / brightTime (HH:MM) - auto-insert colon
    TextInput *ti = getActiveInput();
    if (ti && ti->getValue().size() == 2 && inputText[0] != ':') {
      ti->onTextInput(":");
    }
    if (ti)
      ti->onTextInput(inputText);
    return true;
  }

  if (activeTab_ == Tab::Watchlist && activeField_ == 0) {
    // Callsign/List: alphanumeric, slash, comma, space, max 256 chars
    for (const char *p = inputText; *p; ++p) {
      if (!((*p >= 'A' && *p <= 'Z') || (*p >= 'a' && *p <= 'z') ||
            (*p >= '0' && *p <= '9') || *p == '/' || *p == ',' || *p == ' ')) {
        return true;
      }
    }
    if ((int)watchlistInputField_.getValue().size() < 256) {
      std::string upper = inputText;
      std::transform(upper.begin(), upper.end(), upper.begin(), ::toupper);
      watchlistInputField_.onTextInput(upper.c_str());
    }
    return true;
  }

  // === CALLSIGN VALIDATION (Identity Tab, Field 0) ===
  if (activeTab_ == Tab::Identity && activeField_ == 0) {
    if (callsignInput_.hasSelection()) {
      // If we have a selection, we're replacing it - just pass it through
      // and TextInput will handle upper/lower as needed via its own
      // render/storage but here we still want to force upper case for
      // callsigns.
      std::string upper = inputText;
      std::transform(upper.begin(), upper.end(), upper.begin(), ::toupper);
      callsignInput_.onTextInput(upper.c_str());
      return true;
    }
    for (const char *p = inputText; *p; ++p) {
      if (!((*p >= 'A' && *p <= 'Z') || (*p >= 'a' && *p <= 'z') ||
            (*p >= '0' && *p <= '9') || *p == '/' || *p == ',' || *p == ' ')) {
        return true;
      }
    }
    std::string upper = inputText;
    std::transform(upper.begin(), upper.end(), upper.begin(), ::toupper);
    callsignInput_.onTextInput(upper.c_str());
    return true;
  }

  // === GRID SQUARE VALIDATION (Identity Tab, Field 1) ===
  if (activeTab_ == Tab::Identity && activeField_ == 1) {
    if (gridInput_.hasSelection()) {
      gridInput_.onTextInput(inputText); // Let it through
      autoPopulateLatLon();
      return true;
    }
    const std::string &cur = gridInput_.getValue();
    for (const char *p = inputText; *p; ++p) {
      int pos = static_cast<int>(cur.size());
      if (pos >= 6) {
        return true;
      }
      if (pos < 2) {
        if (!((*p >= 'A' && *p <= 'R') || (*p >= 'a' && *p <= 'r'))) {
          return true;
        }
      } else if (pos < 4) {
        if (!(*p >= '0' && *p <= '9')) {
          return true;
        }
      } else {
        if (!((*p >= 'A' && *p <= 'X') || (*p >= 'a' && *p <= 'x'))) {
          return true;
        }
      }
    }
    std::string formatted = inputText;
    for (size_t i = 0; i < formatted.size(); ++i) {
      int pos =
          static_cast<int>(gridInput_.getValue().size()) + static_cast<int>(i);
      if (pos < 2) {
        formatted[i] = std::toupper(formatted[i]);
      } else if (pos >= 4) {
        formatted[i] = std::tolower(formatted[i]);
      }
    }
    gridInput_.onTextInput(formatted.c_str());
    latLonManual_ = false;
    return true;
  }

  // === LAT/LON VALIDATION (Identity Tab, Fields 2 & 3) ===
  if (activeTab_ == Tab::Identity && (activeField_ == 2 || activeField_ == 3)) {
    for (const char *p = inputText; *p; ++p) {
      if ((*p >= '0' && *p <= '9') || *p == '-' || *p == '.')
        continue;
      return true;
    }
    latLonManual_ = true;
    TextInput *ti = getActiveInput();
    if (ti)
      ti->onTextInput(inputText);
    return true;
  }

  // === PORT VALIDATION (Spotting Tab, Fields 1 and 3) ===
  if (activeTab_ == Tab::Spotting && (activeField_ == 1 || activeField_ == 3)) {
    for (const char *p = inputText; *p; ++p) {
      if (!(*p >= '0' && *p <= '9')) {
        return true;
      }
    }
    TextInput *ti = getActiveInput();
    if (ti) {
      std::string testValue = ti->getValue();
      testValue += inputText;
      int port = StringUtils::safe_stoi(testValue);
      if (port > 65535 || port == 0) {
        return true;
      }
      ti->onTextInput(inputText);
    }
    return true;
  }

  // === DEFAULT INSERTION ===
  TextInput *ti = getActiveInput();
  if (ti)
    ti->onTextInput(inputText);

  return true;
}
