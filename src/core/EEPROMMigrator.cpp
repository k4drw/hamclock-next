#include "EEPROMMigrator.h"
#include "Logger.h"
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <vector>

#if !defined(_WIN32) && !defined(__EMSCRIPTEN__)
#include <pwd.h>
#include <sys/types.h>
#include <unistd.h>
#endif

namespace HamClock {

// Original HamClock NV_ sizes mapping (indices 0..79)
// Values confirmed from original HamClock HamClock.h and nvram.cpp
static const uint8_t NV_SIZES[] = {
    4,  4,  4,  4,  4,  // 0..4 (Touch Cal A-E)
    4,  4,  1,  1,  4,  // 5..9 (Touch F, Div, DXMaxN, DETimeFmt, DELat)
    4,  4,  1,  4,  4,  // 10..14 (DELng, P0Rot, P0, DXLat, DXLng)
    4,  2,  2,  1,  1,  // 15..19 (DXGridOld, CallFG, CallBG, CallRainbow, PSKDist)
    4,  1,  1,  1,  1,  // 20..24 (UTCOffset, P1, P2, BRBRotSetOld, P3)
    1,  2,  2,  2,  2,  // 25..29 (RSSOn, BPWMDim, PhotDim, BPWMBright, PhotBright)
    1,  1,  1,  1,  1,  // 30..34 (LP, Units, LkScrn, MapProj, RotScrnOld)
    32, 32, 12, 9,  1,  // 35..39 (SSID, PW_Old, Callsign, Sat1Name, DESRSS)
    1,  1,  2,  2,  26, // 40..44 (DXSRSS, GridStyle, DpyOn, DpyOff, DXHost)
    2,  1,  4,  18, 4,  // 45..49 (DXPort, SWHue, TempCorr76, GPSDHostOld, KX3Baud)
    2,  4,  4,  2,  1,  // 50..54 (BCPower, CDPeriod, PresCorr76, BRIdle, BRMin)
    1,  4,  4,  10, 1,  // 55..59 (BRMax, DETZ, DXTZ, CoreMapStyle, UseDXCluster)
    1,  1,  1,  64, 1,  // 60..64 (UseGPSD, LogUsage, LblStyle, WIFI_PW, NTPSet)
    18, 1,  2,  2,  2,  // 65..69 (NTPHostOld, GPIOOK, Sat1Color, Sat2Color, X11Flags)
    2,  28, 4,  4,  2,  // 70..74 (BCFlags, DailyOnOff, TempCorr77, PresCorr77, ShortPathCol)
    2,  2,  1,  7,  7   // 75..79 (LongPathCol, PlotOpsOld, NightOn, DEGrid, DXGrid)
};

bool EEPROMMigrator::migrate(AppConfig &config) {
  std::vector<std::filesystem::path> eepromPaths;

  // 1. Current user's home
  const char *home = std::getenv("HOME");
  if (home) {
    eepromPaths.push_back(std::filesystem::path(home) / ".hamclock/eeprom");
  }

#if !defined(_WIN32) && !defined(__EMSCRIPTEN__)
  // 2. Dynamic lookup for UID 1000 (often 'pi' or default user)
  struct passwd *pw = getpwuid(1000);
  if (pw && pw->pw_dir) {
    std::filesystem::path piPath =
        std::filesystem::path(pw->pw_dir) / ".hamclock/eeprom";
    // Avoid duplicates
    if (std::find(eepromPaths.begin(), eepromPaths.end(), piPath) ==
        eepromPaths.end()) {
      eepromPaths.push_back(piPath);
    }
  }
#endif

  // 3. Local directory (fallback/portable)
  eepromPaths.push_back("eeprom");

  std::filesystem::path foundFile;
  for (const auto &p : eepromPaths) {
    if (std::filesystem::exists(p)) {
      foundFile = p;
      break;
    }
  }

  if (foundFile.empty())
    return false;

  LOG_I("Config", "Found original HamClock EEPROM at: {}", foundFile.string());

  std::ifstream ifs(foundFile, std::ios::binary);
  if (!ifs)
    return false;

  std::vector<uint8_t> buffer((std::istreambuf_iterator<char>(ifs)),
                              std::istreambuf_iterator<char>());
  if (buffer.empty())
    return false;

  // Detect hex dump format (starts with "00000000 00\n")
  if (buffer.size() > 11 && buffer[8] == ' ' && buffer[11] == '\n') {
    LOG_I("Config", "Parsing EEPROM text hex dump...");
    std::vector<uint8_t> binBuf;
    std::string s(reinterpret_cast<const char *>(buffer.data()), buffer.size());
    std::istringstream iss(s);
    std::string line;
    while (std::getline(iss, line)) {
      if (line.size() >= 11) {
        unsigned int val;
        std::stringstream ss;
        ss << std::hex << line.substr(9, 2);
        if (ss >> val) {
          binBuf.push_back(static_cast<uint8_t>(val));
        }
      }
    }
    buffer = std::move(binBuf);
  }

  // Parser helpers
  auto readString = [&](size_t offset, size_t len) -> std::string {
    if (offset + len > buffer.size())
      return "";
    std::string s(reinterpret_cast<const char *>(&buffer[offset]), len);
    size_t nulPos = s.find('\0');
    if (nulPos != std::string::npos)
      s.resize(nulPos);
    // Trim trailing spaces (original HamClock sometimes pads)
    while (!s.empty() && std::isspace(s.back()))
      s.pop_back();
    return s;
  };

  auto readFloat = [&](size_t offset) -> float {
    if (offset + 4 > buffer.size())
      return 0.0f;
    float f;
    std::memcpy(&f, &buffer[offset], 4);
    return f;
  };

  auto readU16 = [&](size_t offset) -> uint16_t {
    if (offset + 2 > buffer.size())
      return 0;
    uint16_t v;
    std::memcpy(&v, &buffer[offset], 2);
    return v;
  };

  // Walk NV items starting at NV_BASE (55)
  size_t offset = 55;
  int migratedCount = 0;
  for (size_t i = 0; i < sizeof(NV_SIZES) / sizeof(NV_SIZES[0]); ++i) {
    size_t size = NV_SIZES[i];
    if (offset >= buffer.size())
      break;

    // Original HamClock prefix each valid entry with 0x5A cookie
    if (buffer[offset] == 0x5A) {
      size_t dataOffset = offset + 1;
      switch (i) {
      case 9: // NV_DE_LAT
        config.lat = readFloat(dataOffset);
        migratedCount++;
        break;
      case 10: // NV_DE_LNG
        config.lon = readFloat(dataOffset);
        migratedCount++;
        break;
      case 37: // NV_CALLSIGN
        config.callsign = readString(dataOffset, size);
        migratedCount++;
        break;
      case 44: // NV_DXHOST
        config.dxClusterHost = readString(dataOffset, size);
        migratedCount++;
        break;
      case 45: // NV_DXPORT
        config.dxClusterPort = readU16(dataOffset);
        migratedCount++;
        break;
      case 78: // NV_DE_GRID
        config.grid = readString(dataOffset, size);
        migratedCount++;
        break;
      }
    }
    offset += 1 + size;
  }

  if (migratedCount > 0) {
    LOG_I("Config", "Successfully migrated {} settings from original HamClock.",
          migratedCount);
    return true;
  }

  return false;
}

} // namespace HamClock
