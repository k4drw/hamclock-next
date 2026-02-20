#include "PrefixManager.h"
#include "DXCCData.h"
#include "Logger.h"
#include "PrefixData.h"
#include <algorithm>
#include <cctype>
#include <string_view>

PrefixManager::PrefixManager() {}

void PrefixManager::init() {
  // Data is now sourced directly from the static g_PrefixData array.
  // The init function no longer needs to copy data to a heap vector.
  LOG_I("PrefixManager", "Initialized, using {} static prefixes directly.",
        g_PrefixDataSize);
}

const StaticPrefixEntry *PrefixManager::findEntry(const std::string &call) {
  if (call.empty()) {
    return nullptr;
  }

  // Normalize call (uppercase)
  std::string upperCall = call;
  for (auto &c : upperCall) {
    c = std::toupper(static_cast<unsigned char>(c));
  }

  // The g_PrefixData array is pre-sorted. We can use std::upper_bound to
  // perform an efficient binary search.
  // We search for the first element greater than the callsign, then step back one.
  auto it = std::upper_bound(
      g_PrefixData, g_PrefixData + g_PrefixDataSize, upperCall,
      [](const std::string &val, const StaticPrefixEntry &entry) {
        return val < std::string_view(entry.prefix);
      });

  // Now, iterate backwards from the found position to find the longest matching prefix.
  while (it != g_PrefixData) {
    --it;
    std::string_view entCall(it->prefix);

    if (entCall.length() > upperCall.length()) {
      if (entCall[0] != upperCall[0])
        break; // Moved to a different starting letter, no more matches possible.
      continue;
    }

    // Check if the entry from the static data is a prefix of the searched callsign.
    if (upperCall.rfind(entCall, 0) == 0) {
      return &(*it); // Match found
    }

    if (!entCall.empty() && entCall[0] != upperCall[0]) {
      break; // Optimization: stop if we've moved to a different starting letter.
    }
  }

  return nullptr; // No match found
}

bool PrefixManager::findLocation(const std::string &call, LatLong &ll) {
  std::lock_guard<std::mutex> lock(mutex_);
  const StaticPrefixEntry *entry = findEntry(call);
  if (entry) {
    ll.lat = entry->lat;
    ll.lon = entry->lon;
    return true;
  }
  return false;
}

int PrefixManager::findDXCC(const std::string &call) {
  std::lock_guard<std::mutex> lock(mutex_);
  const StaticPrefixEntry *entry = findEntry(call);
  if (entry) {
    return entry->dxcc;
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
