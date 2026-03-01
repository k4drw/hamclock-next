#include "ReminderPanel.h"
#include "../core/Theme.h"
#include "FontCatalog.h"
#include <ctime>
#include <iomanip>
#include <sstream>

// Forward declaration — defined later in the file alongside renderReminderList
static int daysUntil(const std::string &dateStr);

ReminderPanel::ReminderPanel(int x, int y, int w, int h, FontManager &fontMgr,
                             AppConfig &config, ConfigManager &cfgMgr,
                             CallbookProvider &callbookProvider,
                             std::shared_ptr<CallbookStore> callbookStore,
                             FccProvider &fccProvider)
    : Widget(x, y, w, h), fontMgr_(fontMgr), config_(config), cfgMgr_(cfgMgr),
      callbookProvider_(callbookProvider), callbookStore_(callbookStore),
      fccProvider_(fccProvider), fccResultReady_(false) {
} // Initialize new member

void ReminderPanel::update() {
  // Drain FCC lookup results from the background thread
  if (fccResultReady_.load()) {
    std::vector<FccLicense> results;
    {
      std::lock_guard<std::mutex> lk(fccMutex_);
      results = std::move(fccResults_);
      fccResultReady_.store(false);
    }
    fetching_ = false;
    // Add a pending reminder for each *active* license found under the FRN.
    // Skip the primary callsign — it is already shown in the auto 'Renewal'
    // row.
    for (const auto &lic : results) {
      if (lic.status != "Active")
        continue;

      // If the FCC results include the primary callsign, update its expiry
      // directly!
      if (lic.callsign == config_.callsign) {
        if (!lic.expiry.empty() && config_.callsignExpiry != lic.expiry) {
          config_.callsignExpiry = lic.expiry;
          config_.callsignExpiryLastCheck = std::time(nullptr);
          cfgMgr_.save(config_);
        }
        continue; // skip adding to custom reminders list since it's the auto
                  // row
      }
      // Build label: "K4DRW Renewal" style
      std::string label = lic.callsign + " Renewal";
      bool exists = false;
      for (const auto &re : pendingReminders_) {
        if (re.label == label) {
          exists = true;
          break;
        }
      }
      if (!exists && pendingReminders_.size() < 5)
        pendingReminders_.push_back({label, lic.expiry, true, ""});
    }
  }

  // Background callsign expiry check (runs if empty or > 7 days since last
  // check)
  if (!config_.callsign.empty() && !checking_ && !fetching_) {
    int64_t now = std::time(nullptr);
    bool needsCheck = config_.callsignExpiry.empty();
    if (!needsCheck) {
      if (config_.callsignExpiryLastCheck == 0) {
        needsCheck = true; // First run after update
      } else if (now - config_.callsignExpiryLastCheck > 7 * 24 * 3600) {
        needsCheck = true; // > 7 days
      }
    }
    if (needsCheck) {
      config_.callsignExpiryLastCheck =
          now; // prevent spamming if it fails immediately
      cfgMgr_.save(config_);
      checkCallsignExpiry();
    }
  }

  if (checking_) {
    const auto &data = callbookStore_->get();
    if (data.valid && data.callsign == config_.callsign) {
      bool changed = false;
      if (!data.expiryDate.empty() && config_.callsignExpiry != data.expiryDate) {
        config_.callsignExpiry = data.expiryDate;
        changed = true;
      }
      if (!data.frn.empty() && config_.callsignFrn != data.frn) {
        config_.callsignFrn = data.frn;
        changed = true;
      }
      if (changed || config_.callsignExpiryLastCheck == 0) {
        config_.callsignExpiryLastCheck = std::time(nullptr);
        cfgMgr_.save(config_);
      }
      checking_ = false;
    } else if (data.valid) {
      // Lookup finished but returned empty or wrong callsign
      checking_ = false;
    }
  }

  // Check if any reminder is due today (fires modal notification)
  if (!notificationActive_)
    checkForDueReminders();
}

void ReminderPanel::checkCallsignExpiry() {
  checking_ = true;
  callbookProvider_.lookup(config_.callsign);
}

// ─── notification
// ─────────────────────────────────────────────────────────────

void ReminderPanel::checkForDueReminders() {
  // Don't fire while snoozed
  if (snoozeUntil_ != 0 && SDL_GetTicks() < snoozeUntil_)
    return;

  // Check auto callsign renewal row first (idx = -1)
  if (!config_.callsign.empty() && !config_.callsignExpiry.empty()) {
    if (daysUntil(config_.callsignExpiry) == 0 &&
        config_.callsignExpiryAcknowledged != config_.callsignExpiry) {
      notifyLabel_ = config_.callsign + " license renewal is due today!";
      notifyDate_ = config_.callsignExpiry;
      notifyIdx_ = -1;
      notificationActive_ = true;
      return;
    }
  }

  // Check custom reminder entries
  for (int i = 0; i < (int)config_.reminders.size(); ++i) {
    const auto &re = config_.reminders[i];
    if (!re.active || re.date.empty())
      continue;
    if (daysUntil(re.date) == 0 && re.acknowledgedDate != re.date) {
      notifyLabel_ = re.label + " is due today!";
      notifyDate_ = re.date;
      notifyIdx_ = i;
      notificationActive_ = true;
      return;
    }
  }
}

bool ReminderPanel::handleNotificationClick(int mx, int my) {
  // Snooze — in-memory for 1 hour, dialog disappears
  if (mx >= notifySnoozeRect_.x &&
      mx <= notifySnoozeRect_.x + notifySnoozeRect_.w &&
      my >= notifySnoozeRect_.y &&
      my <= notifySnoozeRect_.y + notifySnoozeRect_.h) {
    snoozeUntil_ = SDL_GetTicks() + 3600000u; // 1 hour in ms
    notificationActive_ = false;
    notifyIdx_ = -2;
    return true;
  }

  // Acknowledge — persists to config so it won't fire again for this date
  if (mx >= notifyAckRect_.x && mx <= notifyAckRect_.x + notifyAckRect_.w &&
      my >= notifyAckRect_.y && my <= notifyAckRect_.y + notifyAckRect_.h) {
    if (notifyIdx_ == -1) {
      config_.callsignExpiryAcknowledged = notifyDate_;
    } else if (notifyIdx_ >= 0 && notifyIdx_ < (int)config_.reminders.size()) {
      config_.reminders[notifyIdx_].acknowledgedDate = notifyDate_;
    }
    cfgMgr_.save(config_);
    notificationActive_ = false;
    notifyIdx_ = -2;
    return true;
  }

  return true; // consume all clicks while modal is up
}

void ReminderPanel::renderModal(SDL_Renderer *renderer) {
  if (!notificationActive_)
    return;

  // Full-screen dark overlay
  SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
  SDL_SetRenderDrawColor(renderer, 0, 0, 0, 180);
  SDL_Rect full = {0, 0, 9999, 9999};
  // Get actual window size via the renderer output size
  int winW = 800, winH = 480;
  SDL_GetRendererOutputSize(renderer, &winW, &winH);
  full = {0, 0, winW, winH};
  SDL_RenderFillRect(renderer, &full);
  SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_NONE);

  // Dialog box  — 320 x 130, centred
  const int dW = 320, dH = 130;
  int dx = (winW - dW) / 2;
  int dy = (winH - dH) / 2;

  // Box background + border
  SDL_SetRenderDrawColor(renderer, 20, 20, 35, 255);
  SDL_Rect box = {dx, dy, dW, dH};
  SDL_RenderFillRect(renderer, &box);
  SDL_SetRenderDrawColor(renderer, 0, 180, 255, 255);
  SDL_RenderDrawRect(renderer, &box);

  // Title bar strip
  SDL_SetRenderDrawColor(renderer, 0, 60, 100, 255);
  SDL_Rect titleBar = {dx, dy, dW, 22};
  SDL_RenderFillRect(renderer, &titleBar);
  SDL_Color white = {255, 255, 255, 255};
  SDL_Color cyan = {0, 200, 255, 255};
  SDL_Color yellow = {255, 220, 50, 255};
  auto *cat = fontMgr_.catalog();

  // Title text
  cat->drawText(renderer, "Reminder Due", dx + dW / 2, dy + 22 / 2, cyan,
                FontStyle::Caption, true);

  // Reminder label (wrap at dialog width)
  int tw, th;
  SDL_Texture *tex = cat->renderText(renderer, notifyLabel_, yellow,
                                     FontStyle::Caption, &tw, &th);
  if (tex) {
    // Centre horizontally, keep within box
    int lx = dx + (dW - tw) / 2;
    if (lx < dx + 8)
      lx = dx + 8;
    SDL_Rect tr = {lx, dy + 32, tw, th};
    SDL_RenderCopy(renderer, tex, nullptr, &tr);
    cat->destroyTexture(tex);
  }

  // Date subtitle
  cat->drawText(renderer, "Date: " + notifyDate_, dx + dW / 2, dy + 50, white,
                FontStyle::Tiny, true);

  // Snooze / Acknowledge buttons
  const int btnW = 110, btnH = 24;
  const int btnY = dy + dH - btnH - 12;
  const int gap = 16;
  int totalW = btnW * 2 + gap;
  int bx = dx + (dW - totalW) / 2;

  // Snooze
  notifySnoozeRect_ = {bx, btnY, btnW, btnH};
  SDL_SetRenderDrawColor(renderer, 20, 40, 80, 255);
  SDL_RenderFillRect(renderer, &notifySnoozeRect_);
  SDL_SetRenderDrawColor(renderer, 60, 120, 200, 255);
  SDL_RenderDrawRect(renderer, &notifySnoozeRect_);
  cat->drawText(renderer, "Snooze 1h", bx + btnW / 2, btnY + btnH / 2, white,
                FontStyle::Caption, true);

  // Acknowledge
  int ax = bx + btnW + gap;
  notifyAckRect_ = {ax, btnY, btnW, btnH};
  SDL_SetRenderDrawColor(renderer, 20, 60, 20, 255);
  SDL_RenderFillRect(renderer, &notifyAckRect_);
  SDL_SetRenderDrawColor(renderer, 50, 160, 50, 255);
  SDL_RenderDrawRect(renderer, &notifyAckRect_);
  cat->drawText(renderer, "Acknowledge", ax + btnW / 2, btnY + btnH / 2, white,
                FontStyle::Caption, true);
}

void ReminderPanel::render(SDL_Renderer *renderer) {
  if (!fontMgr_.ready())
    return;

  if (showSetup_) {
    renderSetup(renderer);
    return;
  }

  ThemeColors themes = getThemeColors(theme_);
  SDL_SetRenderDrawColor(renderer, themes.bg.r, themes.bg.g, themes.bg.b,
                         themes.bg.a);
  SDL_Rect rect = {x_, y_, width_, height_};
  SDL_RenderFillRect(renderer, &rect);
  SDL_SetRenderDrawColor(renderer, themes.border.r, themes.border.g,
                         themes.border.b, themes.border.a);
  SDL_RenderDrawRect(renderer, &rect);

  // Standard title bar (matches other widgets)
  int titleH = 20;
  fontMgr_.catalog()->drawText(renderer, "Reminders", x_ + 10, y_ + 5,
                               themes.accent, FontStyle::MicroBold);
  SDL_SetRenderDrawColor(renderer, themes.border.r, themes.border.g,
                         themes.border.b, 60);
  SDL_RenderDrawLine(renderer, x_ + 5, y_ + titleH, x_ + width_ - 5,
                     y_ + titleH);

  renderReminderList(renderer);

  // Footer: "Setup" tap target (centred, dimmed) — matches LiveSpotPanel
  // pattern
  int footerH = 14;
  int fy = y_ + height_ - footerH - 2;
  int tw = 0, th = 0;
  SDL_Color dim = {100, 100, 120, 255};
  SDL_Texture *ftex = fontMgr_.catalog()->renderText(renderer, "Setup", dim,
                                                     FontStyle::Tiny, &tw, &th);
  if (ftex) {
    footerRect_ = {x_ + (width_ - tw) / 2, fy + (footerH - th) / 2, tw, th};
    SDL_RenderCopy(renderer, ftex, nullptr, &footerRect_);
    fontMgr_.catalog()->destroyTexture(ftex);
  } else {
    // Fallback if font not ready yet
    footerRect_ = {x_ + width_ / 2 - 20, fy, 40, footerH};
  }
}

// ─── renderSetup (inline inside the pane — no global dimming) ────────────────

void ReminderPanel::renderSetup(SDL_Renderer *renderer) {
  ThemeColors themes = getThemeColors(theme_);
  auto *cat = fontMgr_.catalog();

  // Pane background + border
  SDL_SetRenderDrawColor(renderer, themes.bg.r, themes.bg.g, themes.bg.b, 255);
  SDL_Rect bg = {x_, y_, width_, height_};
  SDL_RenderFillRect(renderer, &bg);
  SDL_SetRenderDrawColor(renderer, themes.border.r, themes.border.g,
                         themes.border.b, 255);
  SDL_RenderDrawRect(renderer, &bg);

  SDL_Color cyan = themes.accent;
  SDL_Color white = themes.text;

  int y = y_ + 10;
  int cx = x_ + width_ / 2;

  // Title
  int tw, th;
  SDL_Texture *t = cat->renderText(renderer, "--- Reminders Setup ---", cyan,
                                   FontStyle::FastBold, &tw, &th);
  if (t) {
    SDL_Rect tr = {cx - tw / 2, y, tw, th};
    SDL_RenderCopy(renderer, t, nullptr, &tr);
    cat->destroyTexture(t);
  }
  y += th + 8;

  int fieldW = width_ - 20;
  int fieldH = 18;
  int textPad = 4;

  deleteRects_.clear();

  SDL_Texture *tex = nullptr;
  for (size_t i = 0; i < pendingReminders_.size(); ++i) {
    int halfW = (fieldW - 30) / 2;
    int ry = y;

    // Label field
    SDL_Rect lRect = {x_ + 5, ry, halfW, fieldH};
    SDL_SetRenderDrawColor(renderer, 30, 30, 40, 255);
    SDL_RenderFillRect(renderer, &lRect);
    SDL_SetRenderDrawColor(renderer, (activeField_ == (int)i) ? 255 : 100, 150,
                           0, 255);
    SDL_RenderDrawRect(renderer, &lRect);
    cat->drawText(renderer, pendingReminders_[i].label, lRect.x + textPad,
                  lRect.y + 2, white, FontStyle::Caption);

    // Date field
    SDL_Rect dRect = {x_ + 5 + halfW + 5, ry, halfW, fieldH};
    SDL_SetRenderDrawColor(renderer, 30, 30, 40, 255);
    SDL_RenderFillRect(renderer, &dRect);
    SDL_SetRenderDrawColor(
        renderer,
        (activeField_ == (int)(i + pendingReminders_.size())) ? 255 : 100, 150,
        0, 255);
    SDL_RenderDrawRect(renderer, &dRect);
    cat->drawText(renderer, pendingReminders_[i].date, dRect.x + textPad,
                  dRect.y + 2, white, FontStyle::Caption);

    // Delete (×) button
    SDL_Rect xRect = {x_ + width_ - 20, ry, 15, fieldH};
    deleteRects_.push_back(xRect);
    SDL_SetRenderDrawColor(renderer, 80, 20, 20, 255);
    SDL_RenderFillRect(renderer, &xRect);
    cat->drawText(renderer, "x", xRect.x + xRect.w / 2, xRect.y + xRect.h / 2,
                  white, FontStyle::Tiny, true, false, true);

    y += fieldH + 5;
  }

  // Buttons
  int btnW = 80;
  int btnH = 28;
  int btnY = y_ + height_ - btnH - 6;

  // "+ Add"
  addRect_ = {x_ + 5, btnY - 32, 56, 20};
  SDL_SetRenderDrawColor(renderer, 20, 60, 20, 255);
  SDL_RenderFillRect(renderer, &addRect_);
  SDL_SetRenderDrawColor(renderer, 50, 150, 50, 255);
  SDL_RenderDrawRect(renderer, &addRect_);
  tex = cat->renderText(renderer, "+ Add", white, FontStyle::Caption, &tw, &th);
  if (tex) {
    SDL_Rect tr = {addRect_.x + (addRect_.w - tw) / 2,
                   addRect_.y + (addRect_.h - th) / 2, tw, th};
    SDL_RenderCopy(renderer, tex, nullptr, &tr);
    cat->destroyTexture(tex);
  }

  // "Auto-Pop"
  autoPopRect_ = {x_ + 65, btnY - 32, 72, 20};
  SDL_SetRenderDrawColor(renderer, 20, 20, 80, 255);
  SDL_RenderFillRect(renderer, &autoPopRect_);
  SDL_SetRenderDrawColor(renderer, 50, 50, 150, 255);
  SDL_RenderDrawRect(renderer, &autoPopRect_);
  tex = cat->renderText(renderer, fetching_ ? "..." : "Auto-Pop", white,
                        FontStyle::Caption, &tw, &th);
  if (tex) {
    SDL_Rect tr = {autoPopRect_.x + (autoPopRect_.w - tw) / 2,
                   autoPopRect_.y + (autoPopRect_.h - th) / 2, tw, th};
    SDL_RenderCopy(renderer, tex, nullptr, &tr);
    cat->destroyTexture(tex);
  }

  // "Cancel"
  cancelRect_ = {cx - btnW - 5, btnY, btnW, btnH};
  SDL_SetRenderDrawColor(renderer, themes.danger.r, themes.danger.g,
                         themes.danger.b, 255);
  SDL_RenderFillRect(renderer, &cancelRect_);
  SDL_SetRenderDrawColor(renderer, 255, 255, 255, 100);
  SDL_RenderDrawRect(renderer, &cancelRect_);
  tex =
      cat->renderText(renderer, "Cancel", white, FontStyle::Caption, &tw, &th);
  if (tex) {
    SDL_Rect tr = {cancelRect_.x + (btnW - tw) / 2,
                   cancelRect_.y + (btnH - th) / 2, tw, th};
    SDL_RenderCopy(renderer, tex, nullptr, &tr);
    cat->destroyTexture(tex);
  }

  // "Done"
  saveRect_ = {cx + 5, btnY, btnW, btnH};
  SDL_SetRenderDrawColor(renderer, themes.success.r, themes.success.g,
                         themes.success.b, 255);
  SDL_RenderFillRect(renderer, &saveRect_);
  SDL_SetRenderDrawColor(renderer, 255, 255, 255, 100);
  SDL_RenderDrawRect(renderer, &saveRect_);
  tex = cat->renderText(renderer, "Done", white, FontStyle::Caption, &tw, &th);
  if (tex) {
    SDL_Rect tr = {saveRect_.x + (btnW - tw) / 2, saveRect_.y + (btnH - th) / 2,
                   tw, th};
    SDL_RenderCopy(renderer, tex, nullptr, &tr);
    cat->destroyTexture(tex);
  }
}

// ─── mouse ───────────────────────────────────────────────────────────────────

bool ReminderPanel::onMouseUp(int mx, int my, Uint16, int) {
  // Notification modal consumes all clicks (full-screen)
  if (notificationActive_)
    return handleNotificationClick(mx, my);

  // Bounds check — consume nothing if outside pane
  if (mx < x_ || mx >= x_ + width_ || my < y_ || my >= y_ + height_)
    return false;

  if (showSetup_)
    return handleSetupClick(mx, my);

  // Footer click → open setup
  if (mx >= footerRect_.x && mx <= footerRect_.x + footerRect_.w &&
      my >= footerRect_.y && my <= footerRect_.y + footerRect_.h) {
    showSetup_ = true;
    pendingReminders_ = config_.reminders;
    activeField_ = -1;
    return true;
  }

  // Title bar click → return false so PaneContainer shows widget selector
  return false;
}

bool ReminderPanel::handleSetupClick(int mx, int my) {
  // Done
  if (mx >= saveRect_.x && mx < saveRect_.x + saveRect_.w &&
      my >= saveRect_.y && my < saveRect_.y + saveRect_.h) {
    config_.reminders = pendingReminders_;
    cfgMgr_.save(config_);
    showSetup_ = false;
    return true;
  }
  // Cancel
  if (mx >= cancelRect_.x && mx < cancelRect_.x + cancelRect_.w &&
      my >= cancelRect_.y && my < cancelRect_.y + cancelRect_.h) {
    showSetup_ = false;
    return true;
  }
  // Add
  if (mx >= addRect_.x && mx < addRect_.x + addRect_.w && my >= addRect_.y &&
      my < addRect_.y + addRect_.h) {
    if (pendingReminders_.size() < 5) {
      pendingReminders_.push_back({"", "", true, ""});
      activeField_ = (int)pendingReminders_.size() - 1;
      activeProxy_.clear();
      activeProxy_.setActive(true);
    }
    return true;
  }
  // Auto-Pop
  if (mx >= autoPopRect_.x && mx < autoPopRect_.x + autoPopRect_.w &&
      my >= autoPopRect_.y && my < autoPopRect_.y + autoPopRect_.h) {
    autoPopulateLicenses();
    return true;
  }
  // Delete buttons
  for (size_t i = 0; i < deleteRects_.size(); ++i) {
    const auto &r = deleteRects_[i];
    if (mx >= r.x && mx < r.x + r.w && my >= r.y && my < r.y + r.h) {
      pendingReminders_.erase(pendingReminders_.begin() + i);
      activeField_ = -1;
      return true;
    }
  }

  // Field click — detect label/date fields
  if (pendingReminders_.empty())
    return true;

  int fy = y_ + 10 + 20; // approx first row Y (title ~28px)
  int fieldW = width_ - 20;
  int fieldH = 18;
  for (size_t i = 0; i < pendingReminders_.size(); ++i) {
    int halfW = (fieldW - 30) / 2;
    if (mx >= x_ + 5 && mx < x_ + 5 + halfW && my >= fy && my < fy + fieldH) {
      activeField_ = (int)i;
      activeProxy_.setValue(pendingReminders_[i].label);
      activeProxy_.setCursorToEnd();
      activeProxy_.setActive(true);
      return true;
    }
    if (mx >= x_ + 10 + halfW && mx < x_ + 10 + 2 * halfW && my >= fy &&
        my < fy + fieldH) {
      activeField_ = (int)(i + pendingReminders_.size());
      activeProxy_.setValue(pendingReminders_[i].date);
      activeProxy_.setCursorToEnd();
      activeProxy_.setActive(true);
      return true;
    }
    fy += fieldH + 5;
  }

  return true;
}

// ─── keyboard ────────────────────────────────────────────────────────────────

static std::string *getReminderField(std::vector<ReminderEntry> &entries,
                                     int activeField) {
  if (activeField < 0 || entries.empty())
    return nullptr;
  size_t idx = (size_t)activeField;
  if (idx >= entries.size()) {
    idx -= entries.size();
    if (idx < entries.size())
      return &entries[idx].date;
  } else {
    return &entries[idx].label;
  }
  return nullptr;
}

bool ReminderPanel::onKeyDown(SDL_Keycode key, Uint16 mod) {
  if (!showSetup_ || activeField_ == -1)
    return false;

  if (key == SDLK_TAB && !pendingReminders_.empty()) {
    // Sync proxy back before switching
    if (std::string *f = getReminderField(pendingReminders_, activeField_))
      *f = activeProxy_.getValue();
    activeField_ = (activeField_ + 1) % (int)(pendingReminders_.size() * 2);
    if (std::string *f = getReminderField(pendingReminders_, activeField_)) {
      activeProxy_.setValue(*f);
      activeProxy_.setCursorToEnd();
    }
    return true;
  }
  if (key == SDLK_RETURN || key == SDLK_ESCAPE) {
    if (std::string *f = getReminderField(pendingReminders_, activeField_))
      *f = activeProxy_.getValue();
    activeField_ = -1;
    activeProxy_.clear();
    return true;
  }

  activeProxy_.onKeyDown(key, mod);
  // Sync proxy value back to the reminder entry immediately
  if (std::string *f = getReminderField(pendingReminders_, activeField_))
    *f = activeProxy_.getValue();
  return true;
}

bool ReminderPanel::onTextInput(const char *inputText) {
  if (!showSetup_ || activeField_ == -1)
    return false;

  activeProxy_.onTextInput(inputText);
  // Sync proxy value back to the reminder entry
  if (std::string *f = getReminderField(pendingReminders_, activeField_))
    *f = activeProxy_.getValue();
  return true;
}

// ─── auto-populate ───────────────────────────────────────────────────────────

void ReminderPanel::autoPopulateLicenses() {
  if (config_.callsignFrn.empty()) {
    // No FRN stored — nothing to look up
    return;
  }
  if (fetching_)
    return; // already in progress

  fetching_ = true;
  std::string frn = config_.callsignFrn;

  fccProvider_.lookupFrn(frn, [this](std::vector<FccLicense> results) {
    // This callback runs on a background thread — store via mutex
    std::lock_guard<std::mutex> lk(fccMutex_);
    fccResults_ = std::move(results);
    fccResultReady_.store(true);
  });
}

// ─── helpers ─────────────────────────────────────────────────────────────────

static int daysUntil(const std::string &dateStr) {
  if (dateStr.empty())
    return 9999;
  std::tm tm = {};
  std::istringstream ss(dateStr);
  ss >> std::get_time(&tm, "%Y-%m-%d");
  if (ss.fail())
    return 9999;
  std::time_t target = std::mktime(&tm);
  std::time_t now = std::time(nullptr);
  double seconds = std::difftime(target, now);
  return static_cast<int>(seconds / (24.0 * 3600.0));
}

static std::string formatDaysDuration(int totalDays) {
  if (totalDays < 0)
    return std::to_string(totalDays) + "d";
  if (totalDays == 0)
    return "Today";
  if (totalDays < 30)
    return std::to_string(totalDays) + "d";

  int y = totalDays / 365;
  int rem = totalDays % 365;
  int m = rem / 30;
  int d = rem % 30;

  std::string res;
  if (y > 0)
    res += std::to_string(y) + "y ";
  if (m > 0)
    res += std::to_string(m) + "m ";
  if (d > 0 || (y == 0 && m == 0))
    res += std::to_string(d) + "d";

  // Trim trailing space if necessary
  if (!res.empty() && res.back() == ' ')
    res.pop_back();
  return res;
}

void ReminderPanel::renderReminderList(SDL_Renderer *renderer) {
  int yOff = y_ + 25;
  int lineH = 15;
  auto *cat = fontMgr_.catalog();

  hoverZones_.clear();

  // Always show primary callsign renewal row if we have it
  if (!config_.callsign.empty()) {
    std::string label = config_.callsign + " Renewal";
    std::string date = config_.callsignExpiry;
    SDL_Color color = {200, 200, 200, 255};
    std::string status;
    if (date.empty()) {
      status = checking_ ? "Checking..." : "Unknown";
    } else {
      int days = daysUntil(date);
      status = formatDaysDuration(days);
      if (days < 30)
        color = {255, 50, 50, 255};
      else if (days < 90)
        color = {255, 255, 50, 255};
    }

    int tw, th;
    SDL_Texture *tex =
        cat->renderText(renderer, label, color, FontStyle::Caption, &tw, &th);
    if (tex) {
      SDL_Rect dst = {x_ + 10, yOff + (lineH - th) / 2, tw, th};
      SDL_RenderCopy(renderer, tex, nullptr, &dst);
      cat->destroyTexture(tex);
    }
    tex =
        cat->renderText(renderer, status, color, FontStyle::Caption, &tw, &th);
    if (tex) {
      SDL_Rect dst = {x_ + width_ - 10 - tw, yOff + (lineH - th) / 2, tw, th};
      SDL_RenderCopy(renderer, tex, nullptr, &dst);
      cat->destroyTexture(tex);
      hoverZones_.push_back({dst, date});
    }
    yOff += lineH;
  }

  // Custom reminder entries
  for (const auto &re : config_.reminders) {
    if (!re.active)
      continue;
    if (yOff > y_ + height_ - 15)
      break;

    int days = daysUntil(re.date);
    std::string status;
    status = formatDaysDuration(days);
    SDL_Color color = {255, 255, 255, 255};
    if (days < 0)
      color = {100, 100, 100, 255};
    else if (days < 10)
      color = {255, 100, 100, 255};

    int tw, th;
    SDL_Texture *tex = cat->renderText(renderer, re.label, color,
                                       FontStyle::Caption, &tw, &th);
    if (tex) {
      SDL_Rect dst = {x_ + 10, yOff + (lineH - th) / 2, tw, th};
      SDL_RenderCopy(renderer, tex, nullptr, &dst);
      cat->destroyTexture(tex);
    }
    tex =
        cat->renderText(renderer, status, color, FontStyle::Caption, &tw, &th);
    if (tex) {
      SDL_Rect dst = {x_ + width_ - 10 - tw, yOff + (lineH - th) / 2, tw, th};
      SDL_RenderCopy(renderer, tex, nullptr, &dst);
      cat->destroyTexture(tex);
      hoverZones_.push_back({dst, re.date});
    }
    yOff += lineH;
  }

  if (config_.reminders.empty() && config_.callsignExpiry.empty() &&
      !checking_) {
    cat->drawText(renderer, "No Reminders", x_ + width_ / 2, y_ + height_ / 2,
                  {150, 150, 150, 255}, FontStyle::Micro, true);
  }

  if (tooltip_.visible) {
    renderTooltip(renderer);
  }
}

void ReminderPanel::onMouseMove(int mx, int my) {
  tooltip_.visible = false;
  if (showSetup_ || notificationActive_)
    return;

  for (const auto &hz : hoverZones_) {
    if (mx >= hz.rect.x && mx < hz.rect.x + hz.rect.w && my >= hz.rect.y &&
        my < hz.rect.y + hz.rect.h) {
      tooltip_.text = "Target: " + hz.text;
      tooltip_.x = mx;
      tooltip_.y = my;
      tooltip_.visible = true;
      break;
    }
  }
}

void ReminderPanel::renderTooltip(SDL_Renderer *renderer) {
  if (tooltip_.text.empty())
    return;

  auto *cat = fontMgr_.catalog();
  int tw, th;
  cat->renderText(renderer, tooltip_.text, {255, 255, 255, 255},
                  FontStyle::Micro, &tw, &th);

  int padX = 8;
  int padY = 4;
  int boxW = tw + padX * 2;
  int boxH = th + padY * 2;

  int bx = tooltip_.x - boxW / 2;
  int by = tooltip_.y - boxH - 12;

  if (by < y_) {
    by = tooltip_.y + 16;
  }
  if (bx < x_)
    bx = x_;
  if (bx + boxW > x_ + width_)
    bx = x_ + width_ - boxW;

  SDL_Rect box = {bx, by, boxW, boxH};
  SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
  SDL_SetRenderDrawColor(renderer, 20, 20, 20, 200);
  SDL_RenderFillRect(renderer, &box);
  SDL_SetRenderDrawColor(renderer, 100, 100, 100, 255);
  SDL_RenderDrawRect(renderer, &box);

  cat->drawText(renderer, tooltip_.text, bx + padX, by + padY,
                {255, 255, 255, 255}, FontStyle::Micro);
}
