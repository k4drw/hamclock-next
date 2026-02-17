#include "ADIFProvider.h"
#include "../core/Logger.h"
#include <algorithm>
#include <cctype>
#include <fstream>
#include <sstream>

ADIFProvider::ADIFProvider(std::shared_ptr<ADIFStore> store)
    : store_(std::move(store)) {}

void ADIFProvider::fetch(const std::filesystem::path &path) {
  if (std::filesystem::exists(path)) {
    processFile(path);
  }
}

// Enhanced ADIF tag parser supporting all data types
static std::string getTagContent(const std::string &line,
                                 const std::string &tag) {
  // Case-insensitive tag search
  std::string upperLine = line;
  std::string upperTag = tag;
  std::transform(upperLine.begin(), upperLine.end(), upperLine.begin(),
                 [](unsigned char c) { return std::toupper(c); });
  std::transform(upperTag.begin(), upperTag.end(), upperTag.begin(),
                 [](unsigned char c) { return std::toupper(c); });

  size_t pos = upperLine.find("<" + upperTag + ":");
  if (pos == std::string::npos) {
    // Try without length specifier (ADIF 3.x allows <TAG>value)
    pos = upperLine.find("<" + upperTag + ">");
    if (pos != std::string::npos) {
      size_t start = pos + upperTag.length() + 2;
      size_t end = upperLine.find("<", start);
      if (end != std::string::npos) {
        return line.substr(start, end - start);
      }
    }
    return "";
  }

  size_t colon = line.find(":", pos);
  size_t typeStart = colon + 1;

  // Check for type specifier (e.g., <FREQ:9:N>)
  size_t nextColon = line.find(":", typeStart);
  size_t close = line.find(">", typeStart);

  if (close == std::string::npos)
    return "";

  // Parse length
  int len = 0;
  try {
    std::string lenStr;
    if (nextColon != std::string::npos && nextColon < close) {
      // Format: <TAG:LEN:TYPE>
      lenStr = line.substr(typeStart, nextColon - typeStart);
    } else {
      // Format: <TAG:LEN>
      lenStr = line.substr(typeStart, close - typeStart);
    }

    // Trim whitespace
    lenStr.erase(0, lenStr.find_first_not_of(" \t"));
    lenStr.erase(lenStr.find_last_not_of(" \t") + 1);

    if (lenStr.empty())
      return "";

    len = std::stoi(lenStr);
  } catch (...) {
    LOG_W("ADIFProvider", "Invalid length for tag {}", tag);
    return "";
  }

  size_t valueStart = close + 1;
  if (valueStart + len > line.length()) {
    LOG_W("ADIFProvider", "Tag {} length exceeds line boundary", tag);
    return line.substr(valueStart); // Return what's available
  }

  return line.substr(valueStart, len);
}

// Parse ADIF header
static std::string parseHeader(std::ifstream &file) {
  std::string header;
  std::string line;
  bool inHeader = false;

  std::streampos startPos = file.tellg();

  while (std::getline(file, line)) {
    std::string upperLine = line;
    std::transform(upperLine.begin(), upperLine.end(), upperLine.begin(),
                   [](unsigned char c) { return std::toupper(c); });

    if (upperLine.find("<EOH>") != std::string::npos) {
      inHeader = false;
      break;
    }

    if (upperLine.find("<ADIF_VER:") != std::string::npos || inHeader) {
      inHeader = true;
      header += line + "\n";
    }
  }

  if (header.empty()) {
    // No header found, reset to start
    file.clear();
    file.seekg(startPos);
  }

  return header;
}

void ADIFProvider::processFile(const std::filesystem::path &path) {
  std::ifstream file(path);
  if (!file.is_open()) {
    LOG_E("ADIFProvider", "Failed to open ADIF file: {}", path.string());
    return;
  }

  LOG_I("ADIFProvider", "Processing ADIF file: {}", path.string());

  ADIFStats stats;
  std::string line;
  std::string record;
  int lineNum = 0;
  int recordNum = 0;

  // Parse header
  std::string header = parseHeader(file);
  if (!header.empty()) {
    std::string version = getTagContent(header, "ADIF_VER");
    if (!version.empty()) {
      LOG_I("ADIFProvider", "ADIF version: {}", version);
    }
  }

  // Process records
  while (std::getline(file, line)) {
    lineNum++;

    // Handle multi-line records
    size_t eor = line.find("<EOR>");
    if (eor == std::string::npos) {
      eor = line.find("<eor>");
    }

    if (eor != std::string::npos) {
      record += line.substr(0, eor);
      recordNum++;

      // Extract common fields
      std::string call = getTagContent(record, "CALL");
      std::string mode = getTagContent(record, "MODE");
      std::string band = getTagContent(record, "BAND");
      std::string freq = getTagContent(record, "FREQ");
      std::string qsoDate = getTagContent(record, "QSO_DATE");
      std::string timeOn = getTagContent(record, "TIME_ON");
      std::string rstSent = getTagContent(record, "RST_SENT");
      std::string rstRcvd = getTagContent(record, "RST_RCVD");
      std::string name = getTagContent(record, "NAME");
      std::string qth = getTagContent(record, "QTH");
      std::string gridsquare = getTagContent(record, "GRIDSQUARE");
      std::string country = getTagContent(record, "COUNTRY");
      std::string cqZone = getTagContent(record, "CQZ");
      std::string ituZone = getTagContent(record, "ITUZ");
      std::string dxcc = getTagContent(record, "DXCC");
      std::string contest = getTagContent(record, "CONTEST_ID");
      std::string satName = getTagContent(record, "SAT_NAME");
      std::string satMode = getTagContent(record, "SAT_MODE");
      std::string propMode = getTagContent(record, "PROP_MODE");
      std::string txPwr = getTagContent(record, "TX_PWR");
      std::string operator_ = getTagContent(record, "OPERATOR");
      std::string stationCall = getTagContent(record, "STATION_CALLSIGN");
      std::string myGrid = getTagContent(record, "MY_GRIDSQUARE");
      std::string comment = getTagContent(record, "COMMENT");
      std::string notes = getTagContent(record, "NOTES");

      if (!call.empty()) {
        stats.totalQSOs++;

        // Count by mode
        if (!mode.empty()) {
          stats.modeCounts[mode]++;
        }

        // Infer band from frequency if BAND tag missing
        std::string useBand = band;
        if (useBand.empty() && !freq.empty()) {
          try {
            double freqMhz = std::stod(freq);
            if (freqMhz >= 1.8 && freqMhz < 2.0) useBand = "160m";
            else if (freqMhz >= 3.5 && freqMhz < 4.0) useBand = "80m";
            else if (freqMhz >= 7.0 && freqMhz < 7.3) useBand = "40m";
            else if (freqMhz >= 10.1 && freqMhz < 10.15) useBand = "30m";
            else if (freqMhz >= 14.0 && freqMhz < 14.35) useBand = "20m";
            else if (freqMhz >= 18.068 && freqMhz < 18.168) useBand = "17m";
            else if (freqMhz >= 21.0 && freqMhz < 21.45) useBand = "15m";
            else if (freqMhz >= 24.89 && freqMhz < 24.99) useBand = "12m";
            else if (freqMhz >= 28.0 && freqMhz < 29.7) useBand = "10m";
            else if (freqMhz >= 50.0 && freqMhz < 54.0) useBand = "6m";
            else if (freqMhz >= 144.0 && freqMhz < 148.0) useBand = "2m";
            else if (freqMhz >= 420.0 && freqMhz < 450.0) useBand = "70cm";
          } catch (...) {
            // Invalid frequency, skip
          }
        }

        // Count by band
        if (!useBand.empty()) {
          stats.bandCounts[useBand]++;
        }

        // Maintain latest calls list (most recent first)
        auto it = std::find(stats.latestCalls.begin(), stats.latestCalls.end(), call);
        if (it != stats.latestCalls.end()) {
          stats.latestCalls.erase(it);
        }
        stats.latestCalls.insert(stats.latestCalls.begin(), call);
        if (stats.latestCalls.size() > 10) {
          stats.latestCalls.resize(10);
        }

        // Store full QSO record (keep most recent 100)
        QSORecord qso;
        qso.callsign = call;
        qso.date = qsoDate;
        qso.time = timeOn;
        qso.band = useBand;
        qso.mode = mode;
        qso.freq = freq;
        qso.rstSent = rstSent;
        qso.rstRcvd = rstRcvd;
        qso.name = name;
        qso.qth = qth;
        qso.gridsquare = gridsquare;
        qso.comment = comment;

        // Insert at beginning (newest first)
        stats.recentQSOs.insert(stats.recentQSOs.begin(), qso);
        if (stats.recentQSOs.size() > 50) {
          stats.recentQSOs.resize(50);
        }
      } else {
        LOG_W("ADIFProvider", "Record {} has no CALL field", recordNum);
      }

      // Clear record for next one
      record = line.substr(eor + 5);
    } else {
      // Accumulate multi-line record
      record += line + " ";

      // Safety check: if record gets too large, something is wrong
      if (record.length() > 100000) {
        LOG_E("ADIFProvider", "Record too large (>100KB) at line {}, skipping", lineNum);
        record.clear();
      }
    }
  }

  // Handle case where file doesn't end with <EOR>
  if (!record.empty() && record.find("<") != std::string::npos) {
    std::string call = getTagContent(record, "CALL");
    if (!call.empty()) {
      stats.totalQSOs++;
      stats.latestCalls.insert(stats.latestCalls.begin(), call);
      if (stats.latestCalls.size() > 10) {
        stats.latestCalls.resize(10);
      }
    }
  }

  stats.valid = true;
  store_->update(stats);

  LOG_I("ADIFProvider", "Processed {} QSOs from {} records in {} lines",
        stats.totalQSOs, recordNum, lineNum);
}
