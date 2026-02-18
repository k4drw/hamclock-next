#include "ContestProvider.h"
#include "../core/Astronomy.h"
#include "../core/StringUtils.h"
#include <chrono>
#include <cstdio>
#include <ctime>
#include <vector>

ContestProvider::ContestProvider(NetworkManager &net,
                                 std::shared_ptr<ContestStore> store)
    : net_(net), store_(std::move(store)) {}

void ContestProvider::fetch() {
  net_.fetchAsync(CONTEST_URL, [this](std::string body) {
    if (!body.empty()) {
      processData(body);
    }
  });
}

// Simple RSS parser for WA7BNM Contest Calendar
void ContestProvider::processData(const std::string &body) {
  ContestData data;
  size_t pos = 0;

  // Get current year from system clock
  auto now = std::chrono::system_clock::now();
  std::time_t now_c = std::chrono::system_clock::to_time_t(now);
  struct tm now_tm_buf{};
  struct tm *now_tm = Astronomy::portable_gmtime(&now_c, &now_tm_buf);
  int currentYear = now_tm->tm_year + 1900;

  static const char *MONTHS[] = {"Jan", "Feb", "Mar", "Apr", "May", "Jun",
                                 "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};

  while ((pos = body.find("<item>", pos)) != std::string::npos) {
    size_t end = body.find("</item>", pos);
    if (end == std::string::npos)
      break;

    std::string item = body.substr(pos, end - pos);
    pos = end;

    Contest c;
    // Extract title
    size_t t_start = item.find("<title>");
    size_t t_end = item.find("</title>");
    if (t_start != std::string::npos && t_end != std::string::npos) {
      c.title = item.substr(t_start + 7, t_end - (t_start + 7));
    }

    // Extract link/url
    size_t l_start = item.find("<link>");
    size_t l_end = item.find("</link>");
    if (l_start != std::string::npos && l_end != std::string::npos) {
      c.url = item.substr(l_start + 6, l_end - (l_start + 6));
      // Trim whitespace
      while (!c.url.empty() && (c.url.front() == ' ' || c.url.front() == '\n' || c.url.front() == '\r'))
        c.url.erase(c.url.begin());
      while (!c.url.empty() && (c.url.back() == ' ' || c.url.back() == '\n' || c.url.back() == '\r'))
        c.url.pop_back();
    }

    // Extract description (dates)
    size_t d_start = item.find("<description>");
    size_t d_end = item.find("</description>");
    if (d_start != std::string::npos && d_end != std::string::npos) {
      std::string desc = item.substr(d_start + 13, d_end - (d_start + 13));
      c.dateDesc = desc;

      // Format 1: "1300Z, Feb 9 to 2359Z, Feb 13"
      // Format 2: "0130Z-0330Z, Feb 11"

      auto parseTime =
          [&](const std::string &timeStr, const std::string &dayStr,
              int monthIdx) -> std::chrono::system_clock::time_point {
        struct tm t = {};
        t.tm_year = currentYear - 1900;
        t.tm_mon = monthIdx;
        t.tm_mday = StringUtils::safe_stoi(dayStr);
        t.tm_hour = StringUtils::safe_stoi(timeStr.substr(0, 2));
        t.tm_min = StringUtils::safe_stoi(timeStr.substr(2, 2));
        t.tm_isdst = 0;
        return std::chrono::system_clock::from_time_t(
            Astronomy::portable_timegm(&t));
      };

      try {
        size_t toPos = desc.find(" to ");
        if (toPos != std::string::npos) {
          // Multi-day
          std::string startPart = desc.substr(0, toPos);
          std::string endPart = desc.substr(toPos + 4);

          // Start: "1300Z, Feb 9"
          size_t comma = startPart.find(",");
          std::string sTime = startPart.substr(0, 4);
          std::string sMonth = startPart.substr(comma + 2, 3);
          std::string sDay = startPart.substr(comma + 6);

          int mIdx = 0;
          for (; mIdx < 12; ++mIdx)
            if (sMonth == MONTHS[mIdx])
              break;
          c.startTime = parseTime(sTime, sDay, mIdx);

          // End: "2359Z, Feb 13"
          comma = endPart.find(",");
          std::string eTime = endPart.substr(0, 4);
          std::string eMonth = endPart.substr(comma + 2, 3);
          std::string eDay = endPart.substr(comma + 6);
          mIdx = 0;
          for (; mIdx < 12; ++mIdx)
            if (eMonth == MONTHS[mIdx])
              break;
          c.endTime = parseTime(eTime, eDay, mIdx);

        } else {
          // Single day: "0130Z-0330Z, Feb 11"
          size_t dash = desc.find("-");
          size_t comma = desc.find(",");
          std::string sTime = desc.substr(0, 4);
          std::string eTime = desc.substr(dash + 1, 4);
          std::string Month = desc.substr(comma + 2, 3);
          std::string Day = desc.substr(comma + 6);

          int mIdx = 0;
          for (; mIdx < 12; ++mIdx)
            if (Month == MONTHS[mIdx])
              break;
          c.startTime = parseTime(sTime, Day, mIdx);
          c.endTime = parseTime(eTime, Day, mIdx);

          // If end time is before start time (crosses midnight), add a day
          if (c.endTime < c.startTime) {
            c.endTime += std::chrono::hours(24);
          }
        }
      } catch (...) {
        continue;
      }
    }

    data.contests.push_back(c);
  }

  data.lastUpdate = std::chrono::system_clock::now();
  data.valid = !data.contests.empty();
  store_->update(data);
}
