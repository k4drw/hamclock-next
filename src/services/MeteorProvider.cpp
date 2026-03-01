#include "MeteorProvider.h"
#include <SDL.h>
#include <cmath>
#include <ctime>
#include <algorithm>

namespace HamClock {

MeteorProvider::MeteorProvider() {
  // Major annual showers
  showers_ = {
    {"Quadrantids", 1, 1, 1, 5, 1, 3, 120},
    {"Lyrids", 4, 16, 4, 25, 4, 22, 18},
    {"Eta Aquariids", 4, 19, 5, 28, 5, 6, 50},
    {"Delta Aquariids", 7, 12, 8, 23, 7, 30, 20},
    {"Perseids", 7, 17, 8, 24, 8, 12, 100},
    {"Orionids", 10, 2, 11, 7, 10, 21, 25},
    {"Leonids", 11, 6, 11, 30, 11, 17, 15},
    {"Geminids", 12, 4, 12, 17, 12, 14, 120},
    {"Ursids", 12, 17, 12, 26, 12, 22, 10}
  };
}

float MeteorProvider::getDiurnalMultiplier(int localHour) {
  // Meteor activity peaks at 04:00 - 08:00 local time
  // and is lowest at 16:00 - 20:00.
  // We use a shifted sine wave.
  float phase = (localHour - 6) * M_PI / 12.0f;
  return 1.0f + 0.5f * std::cos(phase); // Range 0.5 to 1.5
}

void MeteorProvider::update(double lat, double lon) {
  uint32_t now = SDL_GetTicks();
  if (lastUpdate_ > 0 && (now - lastUpdate_) < kUpdateIntervalMs) {
    return;
  }
  lastUpdate_ = now;
  calculateActivity(lat, lon);
}

void MeteorProvider::calculateActivity(double lat, double lon) {
  std::time_t t = std::time(nullptr);
  std::tm *now = std::gmtime(&t);
  
  int month = now->tm_mon + 1;
  int day = now->tm_mday;
  
  MeteorData newData;
  newData.valid = true;
  
  float baseZhr = 6.0f; // Background sporadic rate
  float showerZhr = 0.0f;
  
  for (const auto& s : showers_) {
    // Simplified date check (doesn't handle year wrap for Quadrantids yet)
    bool active = false;
    if (s.startMonth == s.endMonth) {
      active = (month == s.startMonth && day >= s.startDay && day <= s.endDay);
    } else {
      active = (month == s.startMonth && day >= s.startDay) || (month == s.endMonth && day <= s.endDay);
    }
    
    if (active) {
      newData.activeShowers.push_back(s.name);
      // Linear ramp to peak
      float daysFromPeak = 0; // Simplified
      float contrib = s.zhr * std::exp(-std::abs(daysFromPeak) / 2.0f);
      showerZhr += contrib;
    }
  }

  // Calculate hourly prediction for next 24 hours
  int currentHour = now->tm_hour;
  // Local hour offset based on longitude
  int lonOffset = static_cast<int>(lon / 15.0);

  for (int i = 0; i < 24; ++i) {
    int hour = (currentHour + i + lonOffset + 24) % 24;
    float diurnal = getDiurnalMultiplier(hour);
    newData.hourlyActivity[i] = (baseZhr + showerZhr) * diurnal;
  }

  // Current index normalized 0-10
  float currentZhr = (baseZhr + showerZhr) * getDiurnalMultiplier((currentHour + lonOffset + 24) % 24);
  newData.currentIndex = std::clamp(currentZhr / 15.0f, 0.5f, 10.0f);

  {
    std::lock_guard<std::mutex> lock(mutex_);
    data_ = newData;
  }

  if (callback_) callback_(newData);
}

MeteorData MeteorProvider::getData() const {
  std::lock_guard<std::mutex> lock(mutex_);
  return data_;
}

} // namespace HamClock
