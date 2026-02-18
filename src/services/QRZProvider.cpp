#include "QRZProvider.h"
#include "../core/Logger.h"
#include "../core/StringUtils.h"

#include <sstream>

QRZProvider::QRZProvider(NetworkManager &netMgr) : netMgr_(netMgr) {}

void QRZProvider::setCredentials(const std::string &username,
                                 const std::string &password) {
  username_ = username;
  password_ = password;
  sessionValid_ = false;
  sessionKey_.clear();
  LOG_I("QRZ", "Credentials configured for user: {}", username);
}

void QRZProvider::lookup(const std::string &callsign,
                         std::function<void(const QRZLookupResult &)> callback) {
  if (!hasCredentials()) {
    LOG_W("QRZ", "No credentials configured");
    QRZLookupResult result;
    result.found = false;
    result.errorMessage = "QRZ credentials not configured";
    callback(result);
    return;
  }

  // First ensure we have a valid session
  establishSession([this, callsign, callback](bool success) {
    if (!success) {
      LOG_E("QRZ", "Failed to establish session");
      QRZLookupResult result;
      result.found = false;
      result.errorMessage = "Failed to authenticate with QRZ.com";
      callback(result);
      return;
    }

    // Now perform the lookup
    std::string url = "https://xmldata.qrz.com/xml/current/?s=" + sessionKey_ +
                      "&callsign=" + callsign;

    netMgr_.fetchAsync(
        url,
        [this, callsign, callback](const std::string &xml) {
          QRZLookupResult result = parseResponse(xml, callsign);

          if (result.found) {
            LOG_I("QRZ", "Lookup successful: {} - {} ({})", callsign,
                  result.name, result.grid);
          } else {
            LOG_W("QRZ", "Lookup failed for {}: {}", callsign,
                  result.errorMessage);
          }

          callback(result);
        },
        3600); // Cache for 1 hour
  });
}

void QRZProvider::establishSession(std::function<void(bool)> callback) {
  // If we already have a valid session, use it
  if (sessionValid_ && !sessionKey_.empty()) {
    callback(true);
    return;
  }

  // Authenticate with QRZ
  std::string url = "https://xmldata.qrz.com/xml/current/?username=" +
                    username_ + "&password=" + password_;

  netMgr_.fetchAsync(
      url,
      [this, callback](const std::string &xml) {
        // Extract session key
        sessionKey_ = extractTag(xml, "Key");

        if (!sessionKey_.empty()) {
          sessionValid_ = true;
          LOG_I("QRZ", "Session established");
          callback(true);
        } else {
          sessionValid_ = false;
          std::string error = extractTag(xml, "Error");
          LOG_E("QRZ", "Authentication failed: {}", error);
          callback(false);
        }
      },
      0); // Don't cache authentication requests
}

QRZLookupResult QRZProvider::parseResponse(const std::string &xml,
                                           const std::string &callsign) {
  QRZLookupResult result;
  result.callsign = callsign;

  // Check for errors
  std::string error = extractTag(xml, "Error");
  if (!error.empty()) {
    result.found = false;
    result.errorMessage = error;

    // If session expired, invalidate it
    if (error.find("Session") != std::string::npos ||
        error.find("Invalid") != std::string::npos) {
      sessionValid_ = false;
      sessionKey_.clear();
    }

    return result;
  }

  // Check if callsign data exists
  std::string call = extractTag(xml, "call");
  if (call.empty()) {
    result.found = false;
    result.errorMessage = "Callsign not found";
    return result;
  }

  // Extract all available fields
  result.found = true;
  result.callsign = call;
  result.name = extractTag(xml, "fname") + " " + extractTag(xml, "name");
  result.addr1 = extractTag(xml, "addr1");
  result.addr2 = extractTag(xml, "addr2");
  result.state = extractTag(xml, "state");
  result.zip = extractTag(xml, "zip");
  result.country = extractTag(xml, "country");
  result.grid = extractTag(xml, "grid");
  result.email = extractTag(xml, "email");
  result.qslMgr = extractTag(xml, "qslmgr");

  // Parse coordinates
  std::string latStr = extractTag(xml, "lat");
  std::string lonStr = extractTag(xml, "lon");
  if (!latStr.empty() && !lonStr.empty()) {
    result.lat = StringUtils::safe_stod(latStr);
    result.lon = StringUtils::safe_stod(lonStr);
  }

  return result;
}

std::string QRZProvider::extractTag(const std::string &xml,
                                    const std::string &tag) {
  std::string openTag = "<" + tag + ">";
  std::string closeTag = "</" + tag + ">";

  size_t start = xml.find(openTag);
  if (start == std::string::npos) {
    return "";
  }

  start += openTag.length();
  size_t end = xml.find(closeTag, start);
  if (end == std::string::npos) {
    return "";
  }

  return xml.substr(start, end - start);
}
