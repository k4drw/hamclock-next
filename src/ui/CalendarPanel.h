#pragma once

#include "../core/CalendarData.h"
#include "FontManager.h"
#include "Widget.h"
#include <ctime>
#include <functional>
#include <memory>
#include <set>
#include <vector>

// Displays upcoming calendar events from CalendarStore (fed by POST
// /set_calendar.json). Shows events for the next 24 hours.
// Tap the gear icon in the title bar to open the inline config screen.
class CalendarPanel : public Widget {
public:
  CalendarPanel(int x, int y, int w, int h, FontManager &fontMgr,
                std::shared_ptr<CalendarStore> store);

  std::string getName() const override { return "Calendar"; }
  const char *typeId() const override { return "calendar"; }
  std::string getDisplayName() const override { return "Calendar"; }
  bool isScrollable() const override { return true; }
  bool requiresConfigKey() const override { return false; }
  void update() override {}
  void render(SDL_Renderer *renderer) override;
  bool onMouseUp(int mx, int my, Uint16 mod, int clicks) override;
  bool onMouseWheel(int scrollY) override { (void)scrollY; return true; }
  void onMouseMove(int mx, int my) override;

  // Initialize config values from AppConfig before first render
  void setNotifyMinutes(int m) { notifyMinutes_ = pendingMinutes_ = m; }
  void setAllDayNotifyHour(int h) { allDayHour_ = pendingHour_ = h; }
  void setDismissMinutes(int m) { dismissMinutes_ = pendingDismiss_ = m; }

  // Called with (notifyMinutes, allDayHour, dismissMinutes) when user taps Done
  void setOnConfigChanged(std::function<void(int, int, int)> cb) {
    onConfigChanged_ = std::move(cb);
  }

private:
  void renderSetup(SDL_Renderer *renderer);
  bool handleSetupClick(int mx, int my);

  FontManager &fontMgr_;
  std::shared_ptr<CalendarStore> store_;

  // Config values (live) and pending (while setup is open)
  int notifyMinutes_ = 10;
  int allDayHour_ = 8;
  int dismissMinutes_ = 10;
  int pendingMinutes_ = 10;
  int pendingHour_ = 8;
  int pendingDismiss_ = 10;

  std::function<void(int, int, int)> onConfigChanged_;

  // Setup state
  bool showSetup_ = false;
  SDL_Rect footerRect_ = {};
  SDL_Rect minsDecBtn_ = {};
  SDL_Rect minsIncBtn_ = {};
  SDL_Rect hourDecBtn_ = {};
  SDL_Rect hourIncBtn_ = {};
  SDL_Rect dismissDecBtn_ = {};
  SDL_Rect dismissIncBtn_ = {};
  SDL_Rect doneBtn_ = {};

  // Cached event list from last render — used by onMouseMove for tooltip
  std::vector<CalendarEvent> upcoming_;
  int firstRowY_ = 0;
  int lastRowH_ = 0;
  int numRows_ = 0;

  // TTS dedup: event start times that have already been spoken
  std::set<time_t> spokenEventStarts_;
};
