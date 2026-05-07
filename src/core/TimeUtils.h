#pragma once

#include <cstdio>
#include <ctime>
#include <string>
#include <string_view>

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

// Lightweight ISO 8601 (YYYY-MM-DD) to time_t.
// Returns 0 on failure. Avoids mktime/tzset overhead.
inline time_t isoToTimeT(std::string_view iso) {
  if (iso.size() < 10) return 0;
  int y = 0, m = 0, d = 0;
  if (std::sscanf(iso.data(), "%d-%d-%d", &y, &m, &d) != 3) return 0;
  
  struct tm tm = {};
  tm.tm_year = y - 1900;
  tm.tm_mon = m - 1;
  tm.tm_mday = d;
  
  // Use timegm if available to avoid tzset/lock, otherwise use a custom fast epoch calculation
#ifdef _WIN32
  return _mkgmtime(&tm);
#else
  return timegm(&tm);
#endif
}

} // namespace TimeUtils
