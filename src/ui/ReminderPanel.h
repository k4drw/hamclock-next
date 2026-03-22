#pragma once

#include "../core/ConfigManager.h"
#include "../services/CallbookProvider.h"
#include "../services/FccProvider.h"
#include "FontManager.h"
#include "TextInput.h"
#include "Widget.h"
#include <atomic>
#include <mutex>
#include <string>
#include <vector>

class ReminderPanel : public Widget {
public:
  ReminderPanel(int x, int y, int w, int h, FontManager &fontMgr,
                AppConfig &config, ConfigManager &cfgMgr,
                CallbookProvider &callbookProvider,
                std::shared_ptr<CallbookStore> callbookStore,
                FccProvider &fccProvider);

  void update() override;
  void render(SDL_Renderer *renderer) override;
  void onResize(int x, int y, int w, int h) override;
  bool onMouseUp(int mx, int my, Uint16 mod, int clicks) override;
  bool onKeyDown(SDL_Keycode key, Uint16 mod) override;
  bool onTextInput(const char *inputText) override;

  // Inline setup
  bool isConfiguring() const override { return showSetup_; }

  // Global modal notification (dims whole screen)
  bool isModalActive() const override { return notificationActive_; }
  void renderModal(SDL_Renderer *renderer) override;

  std::string getName() const override { return "Reminders"; }

private:
  FontManager &fontMgr_;
  AppConfig &config_;
  ConfigManager &cfgMgr_;
  CallbookProvider &callbookProvider_;
  std::shared_ptr<CallbookStore> callbookStore_;
  FccProvider &fccProvider_;

  // ── Inline setup ──────────────────────────────────────────────────────────
  bool showSetup_ = false;
  std::vector<ReminderEntry> pendingReminders_;
  int activeField_ = -1; // -1=none, 0..N-1=label, N..2N-1=date
  TextInput activeProxy_;   // proxy cursor/selection for the active field
  bool checking_ = false;
  bool fetching_ = false;

  struct HoverZone {
    SDL_Rect rect;
    std::string text;
  };
  std::vector<HoverZone> hoverZones_;

  void onMouseMove(int mx, int my) override;

  // ── Notification state ────────────────────────────────────────────────────
  bool notificationActive_ = false;
  std::string notifyLabel_;  // Text shown in the dialog title
  std::string notifyDate_;   // The date that triggered this notification
  int notifyIdx_ = -2;       // -1=callsign row, >=0=reminders[i], -2=none
  uint32_t snoozeUntil_ = 0; // SDL_GetTicks() ms; 0=not snoozed
  SDL_Rect notifySnoozeRect_ = {0, 0, 0, 0};
  SDL_Rect notifyAckRect_ = {0, 0, 0, 0};

  // ── Thread-safe FCC result hand-off ──────────────────────────────────────
  // Shared with the detached FCC callback thread; outlives widget destruction.
  struct FccState {
    std::mutex mutex;
    std::vector<FccLicense> results;
    std::atomic<bool> ready{false};
  };
  std::shared_ptr<FccState> fccState_ = std::make_shared<FccState>();

  // ── Button rects (inline setup) ──────────────────────────────────────────
  SDL_Rect saveRect_ = {0, 0, 0, 0};
  SDL_Rect cancelRect_ = {0, 0, 0, 0};
  SDL_Rect addRect_ = {0, 0, 0, 0};
  SDL_Rect autoPopRect_ = {0, 0, 0, 0};
  SDL_Rect footerRect_ = {0, 0, 0, 0};
  std::vector<SDL_Rect> deleteRects_;
  std::vector<SDL_Rect> labelRects_;
  std::vector<SDL_Rect> dateRects_;

  // ── Private helpers ───────────────────────────────────────────────────────
  void renderReminderList(SDL_Renderer *renderer);
  void renderSetup(SDL_Renderer *renderer);
  bool handleSetupClick(int mx, int my);
  bool handleNotificationClick(int mx, int my);
  void checkCallsignExpiry();
  void autoPopulateLicenses();
  void checkForDueReminders(); // called from update()
};
