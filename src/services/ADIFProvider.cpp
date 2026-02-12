#include "ADIFProvider.h"
#include <fstream>

ADIFProvider::ADIFProvider(std::shared_ptr<ADIFStore> store)
    : store_(std::move(store)) {}

void ADIFProvider::fetch(const std::filesystem::path &path) {
  if (std::filesystem::exists(path)) {
    processFile(path);
  }
}

static std::string getTagContent(const std::string &line,
                                 const std::string &tag) {
  size_t pos = line.find("<" + tag + ":");
  if (pos == std::string::npos)
    return "";

  size_t colon = line.find(":", pos);
  size_t close = line.find(">", colon);
  if (colon == std::string::npos || close == std::string::npos)
    return "";

  try {
    int len = std::stoi(line.substr(colon + 1, close - colon - 1));
    return line.substr(close + 1, len);
  } catch (...) {
    return "";
  }
}

void ADIFProvider::processFile(const std::filesystem::path &path) {
  std::ifstream file(path);
  if (!file.is_open())
    return;

  ADIFStats stats;
  std::string line;
  std::string record;

  while (std::getline(file, line)) {
    size_t eor = line.find("<EOR>");
    if (eor != std::string::npos) {
      record += line.substr(0, eor);

      std::string call = getTagContent(record, "CALL");
      std::string mode = getTagContent(record, "MODE");
      std::string band = getTagContent(record, "BAND");

      if (!call.empty()) {
        stats.totalQSOs++;
        if (!mode.empty())
          stats.modeCounts[mode]++;
        if (!band.empty())
          stats.bandCounts[band]++;

        stats.latestCalls.insert(stats.latestCalls.begin(), call);
        if (stats.latestCalls.size() > 5)
          stats.latestCalls.pop_back();
      }

      record = line.substr(eor + 5);
    } else {
      record += line + " ";
    }
  }

  stats.valid = true;
  store_->update(stats);
}
