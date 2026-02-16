#include "PrefixManager.h"
#include "DXCCData.h"
#include "Logger.h"
#include "PrefixData.h"
#include <algorithm>
#include <cctype>
#include <cstdio>

PrefixManager::PrefixManager() {}

void PrefixManager::init() {
  std::lock_guard<std::mutex> lock(mutex_);
  entries_.clear();
  entries_.reserve(g_PrefixDataSize);

  for (size_t i = 0; i < g_PrefixDataSize; ++i) {
    PrefixEntry entry;
    entry.call = g_PrefixData[i].prefix;
    entry.lat = g_PrefixData[i].lat;
    entry.lon = g_PrefixData[i].lon;
    entry.dxcc = g_PrefixData[i].dxcc;
    entries_.push_back(entry);
  }

  // g_PrefixData is already sorted by prefix string in the generation script,
  // so entries_ is also sorted and ready for upper_bound binary searching.
  LOG_I("PrefixManager", "Initialized with {} STATIC DATA prefixes.",
        entries_.size());
}

bool PrefixManager::findLocation(const std::string &call, LatLong &ll) {
  if (call.empty())
    return false;

  // Normalize call (uppercase)
  std::string upperCall = call;
  for (auto &c : upperCall)
    c = std::toupper(static_cast<unsigned char>(c));

  {
    std::lock_guard<std::mutex> lock(mutex_);
    if (!entries_.empty()) {
      PrefixEntry searchKey;
      searchKey.call = upperCall;

      auto it =
          std::upper_bound(entries_.begin(), entries_.end(), searchKey,
                           [](const PrefixEntry &a, const PrefixEntry &b) {
                             return a.call < b.call;
                           });

      while (it != entries_.begin()) {
        --it;
        const std::string &entCall = it->call;

        if (entCall.length() > upperCall.length()) {
          if (entCall[0] != upperCall[0])
            break;
          continue;
        }

        if (upperCall.compare(0, entCall.length(), entCall) == 0) {
          ll.lat = it->lat;
          ll.lon = it->lon;
          return true;
        }

        if (!entCall.empty() && entCall[0] != upperCall[0])
          break;
      }
    }
  }

  return false;
}

int PrefixManager::findDXCC(const std::string &call) {
  if (call.empty())
    return -1;

  std::string upperCall = call;
  for (auto &c : upperCall)
    c = std::toupper(static_cast<unsigned char>(c));

  {
    std::lock_guard<std::mutex> lock(mutex_);
    if (!entries_.empty()) {
      PrefixEntry searchKey;
      searchKey.call = upperCall;

      auto it =
          std::upper_bound(entries_.begin(), entries_.end(), searchKey,
                           [](const PrefixEntry &a, const PrefixEntry &b) {
                             return a.call < b.call;
                           });

      while (it != entries_.begin()) {
        --it;
        const std::string &entCall = it->call;

        if (entCall.length() > upperCall.length()) {
          if (entCall[0] != upperCall[0])
            break;
          continue;
        }

        if (upperCall.compare(0, entCall.length(), entCall) == 0) {
          return it->dxcc;
        }

        if (!entCall.empty() && entCall[0] != upperCall[0])
          break;
      }
    }
  }

  return -1;
}

std::string PrefixManager::getCountryName(int dxcc) {
  const DXCCEntity *entity = findDXCCEntity(dxcc);
  return entity ? std::string(entity->name) : "";
}

std::string PrefixManager::getContinent(int dxcc) {
  const DXCCEntity *entity = findDXCCEntity(dxcc);
  return entity ? std::string(entity->continent) : "";
}

int PrefixManager::getCQZone(int dxcc) {
  const DXCCEntity *entity = findDXCCEntity(dxcc);
  return entity ? entity->cqZone : -1;
}

int PrefixManager::getITUZone(int dxcc) {
  const DXCCEntity *entity = findDXCCEntity(dxcc);
  return entity ? entity->ituZone : -1;
}
