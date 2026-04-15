#pragma once

#include "ConfigManager.h"
#include <string>

namespace HamClock {

class EEPROMMigrator {
public:
  /**
   * Attempts to find and migrate settings from an original HamClock EEPROM file.
   * Checks ~/.hamclock/eeprom and /home/pi/.hamclock/eeprom.
   * 
   * @param config The AppConfig to populate with migrated settings.
   * @return true if an eeprom file was found and at least one setting was migrated.
   */
  static bool migrate(AppConfig &config);
};

} // namespace HamClock
