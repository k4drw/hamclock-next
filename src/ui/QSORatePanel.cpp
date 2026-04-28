#include "QSORatePanel.h"
#include "../core/Theme.h"
#include "FontCatalog.h"
#include "WidgetRegistry.h"
#include <SDL.h>
#include <algorithm>
#include <cstdio>
#include <ctime>
#include <array>

QSORatePanel::QSORatePanel(int x, int y, int w, int h, FontManager &fontMgr,
                           std::shared_ptr<ADIFStore> adifStore)
    : Widget(x, y, w, h), fontMgr_(fontMgr),
      adifStore_(std::move(adifStore)) {}

QSORatePanel::~QSORatePanel() {}

void QSORatePanel::update() {}

void QSORatePanel::onResize(int x, int y, int w, int h) {
  Widget::onResize(x, y, w, h);
}

void QSORatePanel::render(SDL_Renderer *renderer) {
  ThemeColors themes = getThemeColors(theme_);
  auto *cat = fontMgr_.catalog();

  if (!fontMgr_.ready())
    return;

  renderChrome(renderer);
  renderTitle(renderer, fontMgr_, "QSO Rate");

  if (!adifStore_) {
    cat->drawText(renderer, "No ADIF", x_ + width_ / 2, y_ + height_ / 2,
                  themes.textDim, FontStyle::Micro, true, false, true);
    return;
  }

  ADIFStats stats = adifStore_->get();
  if (!stats.valid || stats.recentQSOs.empty()) {
    cat->drawText(renderer, "No QSOs", x_ + width_ / 2, y_ + height_ / 2,
                  themes.textDim, FontStyle::Micro, true, false, true);
    return;
  }

  // Parse QSOs into hourly buckets for last 12 hours
  time_t now = time(nullptr);
  std::array<int, 12> qsos_per_hour = {0};

  for (const auto &qso : stats.recentQSOs) {
    // Parse date YYYYMMDD and time HHMMSS
    if (qso.date.length() < 8 || qso.time.length() < 6)
      continue;

    int year = std::stoi(qso.date.substr(0, 4));
    int month = std::stoi(qso.date.substr(4, 2));
    int day = std::stoi(qso.date.substr(6, 2));
    int hour = std::stoi(qso.time.substr(0, 2));
    int minute = std::stoi(qso.time.substr(2, 2));
    int second = std::stoi(qso.time.substr(4, 2));

    struct tm qso_tm = {};
    qso_tm.tm_year = year - 1900;
    qso_tm.tm_mon = month - 1;
    qso_tm.tm_mday = day;
    qso_tm.tm_hour = hour;
    qso_tm.tm_min = minute;
    qso_tm.tm_sec = second;

    time_t qso_time = mktime(&qso_tm);
    int hours_ago = (now - qso_time) / 3600;

    if (hours_ago >= 0 && hours_ago < 12) {
      qsos_per_hour[11 - hours_ago]++;
    }
  }

  // Calculate stats
  int max_qsos = 0;
  int total_qsos = 0;
  int current_rate = qsos_per_hour[11];

  for (int i = 0; i < 12; i++) {
    max_qsos = std::max(max_qsos, qsos_per_hour[i]);
    total_qsos += qsos_per_hour[i];
  }
  if (max_qsos == 0)
    max_qsos = 1;

  // Draw stats line
  char stat_buf[64];
  std::snprintf(stat_buf, sizeof(stat_buf), "Now: %d/h | Peak: %d | Total: %d",
                current_rate, max_qsos, total_qsos);
  cat->drawText(renderer, stat_buf, x_ + 6, y_ + 20, themes.accent,
                FontStyle::Tiny);

  // Sparkline bars
  const int spark_y = y_ + 32;
  const int spark_h = height_ - 38;
  const int bar_spacing = (width_ - 12) / 12;
  const int bar_width = bar_spacing - 2;

  if (bar_width > 0 && spark_h > 0) {
    for (int i = 0; i < 12; i++) {
      int bar_x = x_ + 6 + i * bar_spacing;
      int bar_h = (qsos_per_hour[i] * spark_h) / max_qsos;

      if (bar_h > 0) {
        SDL_Rect bar_rect = {bar_x, spark_y + spark_h - bar_h, bar_width,
                             bar_h};

        // Color by rate: green = good, yellow = ok, red = low
        SDL_Color bar_color = themes.textDim;
        if (qsos_per_hour[i] >= max_qsos / 2)
          bar_color = {60, 200, 60, 255};      // green
        else if (qsos_per_hour[i] > 0)
          bar_color = {240, 220, 30, 255};     // yellow

        SDL_SetRenderDrawColor(renderer, bar_color.r, bar_color.g,
                               bar_color.b, bar_color.a);
        SDL_RenderFillRect(renderer, &bar_rect);

        // Border
        SDL_SetRenderDrawColor(renderer, themes.border.r, themes.border.g,
                               themes.border.b, 128);
        SDL_RenderDrawRect(renderer, &bar_rect);
      }
    }
  }
}

REGISTER_WIDGET("qso_rate", "QSO Rate", false, false, {
  return std::make_unique<QSORatePanel>(0, 0, 0, 0, deps.fontMgr,
                                        deps.adifStore);
})
