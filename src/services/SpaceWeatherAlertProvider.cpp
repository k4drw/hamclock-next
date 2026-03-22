#include "SpaceWeatherAlertProvider.h"
#include "../core/Constants.h"
#include "../core/WorkerService.h"
#include <SDL.h>
#include <SDL_events.h>
#include <chrono>
#include <cstring>
#include <nlohmann/json.hpp>
#include <sstream>

using json = nlohmann::json;

SpaceWeatherAlertProvider::SpaceWeatherAlertProvider(
    NetworkManager &net, std::shared_ptr<SpaceWeatherAlertStore> store)
    : net_(net), store_(std::move(store)) {}

// Extract the "Space Weather Message Code:" value from a NOAA alert message.
static std::string extractMsgCode(const std::string &msg) {
  const char *tag = "Space Weather Message Code:";
  auto pos = msg.find(tag);
  if (pos == std::string::npos)
    return "";
  size_t start = msg.find_first_not_of(" \t", pos + strlen(tag));
  if (start == std::string::npos)
    return "";
  size_t end = msg.find_first_of(" \t\r\n", start);
  if (end == std::string::npos)
    end = msg.size();
  return msg.substr(start, end - start);
}

// Classify alert type from NOAA message code.
static SpaceWxAlertType classifyCode(const std::string &code) {
  if (code.size() >= 4 && code.find("ALTK") == 0)
    return SpaceWxAlertType::KIndex;
  if (code.find("WATX") == 0 || code.find("ALTXF") == 0)
    return SpaceWxAlertType::SolarFlareX;
  if (code.find("WATM") == 0 || code.find("ALTMF") == 0)
    return SpaceWxAlertType::SolarFlareM;
  if (code.find("WARCMEG") == 0 || code.find("ALTCMEGSR") == 0 ||
      code.find("PACMEG") == 0)
    return SpaceWxAlertType::CMEArrival;
  if (code.find("WATA") == 0 || code.find("ALTEF") == 0 ||
      code.find("SGAS") == 0)
    return SpaceWxAlertType::ProtonEvent;
  if (code.find("WARTP") == 0 || code.find("ALTTP") == 0 ||
      code.find("ALTG") == 0 || code.find("WARG") == 0)
    return SpaceWxAlertType::GeoStorm;
  return SpaceWxAlertType::Other;
}

// Extract first meaningful content line (after the header block).
// The header ends at the first blank line; the first non-empty line after
// that is the alert/warning/summary text.
static std::string extractContentLine(const std::string &msg) {
  bool pastBlank = false;
  std::istringstream ss(msg);
  std::string line;
  while (std::getline(ss, line)) {
    // Strip trailing CR
    if (!line.empty() && line.back() == '\r')
      line.pop_back();
    if (!pastBlank) {
      if (line.empty())
        pastBlank = true;
    } else {
      if (!line.empty()) {
        if (line.size() > 50)
          line = line.substr(0, 49) + "~";
        return line;
      }
    }
  }
  return "";
}

// Shorten "YYYY-MM-DD HH:MM:SS" to "YYYY-MM-DD HH:MM".
static std::string shortenTime(const std::string &dt) {
  return dt.size() >= 16 ? dt.substr(0, 16) : dt;
}

void SpaceWeatherAlertProvider::fetch() {
  lastFetchMs_ = SDL_GetTicks();
  net_.fetchAsync(
      ALERTS_URL,
      [](std::string body) {
        if (body.empty())
          return;
        WorkerService::getInstance().submitTask([body]() {
          try {
            auto j = json::parse(body, nullptr, false);
            if (j.is_discarded() || !j.is_array())
              return;

            auto *update = new SpaceWxAlertData();
            int count = 0;
            for (const auto &item : j) {
              if (count >= 20)
                break;
              if (!item.is_object())
                continue;
              std::string msg = item.value("message", "");
              std::string dt = item.value("issue_datetime", "");
              if (msg.empty())
                continue;

              SpaceWxAlert alert;
              alert.issueTime = shortenTime(dt);
              std::string code = extractMsgCode(msg);
              alert.type = classifyCode(code);
              alert.typeLabel = extractContentLine(msg);
              if (alert.typeLabel.empty() && !code.empty())
                alert.typeLabel = code;

              update->alerts.push_back(std::move(alert));
              ++count;
            }
            update->valid = true;
            update->lastUpdate = std::chrono::system_clock::now();

            SDL_Event event;
            SDL_zero(event);
            event.type =
                HamClock::AE_BASE_EVENT + HamClock::AE_SPACEWX_ALERT_READY;
            event.user.code = 0;
            event.user.data1 = update;
            SDL_PushEvent(&event);
          } catch (...) {
          }
        });
      },
      900 /* 15-minute network cache */);
}
