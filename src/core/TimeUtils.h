#pragma once

#include <cstdio>
#include <string>

// Lightweight time/date formatting helpers.
// Functions return std::string for direct assignment — no caller-managed buffer needed.
namespace TimeUtils {

// Format as "HH:MM"
inline std::string hm(int h, int m) {
  char buf[8];
  std::snprintf(buf, sizeof(buf), "%02d:%02d", h, m);
  return buf;
}

// Format as "HH:MM:SS"
inline std::string hms(int h, int m, int s) {
  char buf[10];
  std::snprintf(buf, sizeof(buf), "%02d:%02d:%02d", h, m, s);
  return buf;
}

// Format as "YYYY-MM-DD"
inline std::string dateISO(int year, int month, int day) {
  char buf[12];
  std::snprintf(buf, sizeof(buf), "%04d-%02d-%02d", year, month, day);
  return buf;
}

} // namespace TimeUtils
