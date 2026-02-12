#pragma once

#include <string>

struct LatLong {
  double lat;
  double lon;
};

class PrefixManager {
public:
  static bool findLocation(const std::string &call, LatLong &ll);

  struct SmallPrefix {
    char pref[4];
    int16_t lat; // degs * 100
    int16_t lon; // degs * 100
  };

  static const SmallPrefix small_prefs[];
};
