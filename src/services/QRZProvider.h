#pragma once

#include "../network/NetworkManager.h"
#include <memory>
#include <string>

// QRZ callsign lookup result
struct QRZLookupResult {
  bool found = false;
  std::string callsign;
  std::string name;
  std::string addr1;
  std::string addr2;
  std::string state;
  std::string zip;
  std::string country;
  std::string grid;
  std::string email;
  std::string qslMgr;
  double lat = 0.0;
  double lon = 0.0;
  std::string errorMessage;
};

// QRZ.com XML API provider
// Requires QRZ.com XML subscription
class QRZProvider {
public:
  QRZProvider(NetworkManager &netMgr);
  ~QRZProvider() = default;

  // Configure QRZ credentials
  void setCredentials(const std::string &username, const std::string &password);

  // Lookup a callsign (async, returns immediately)
  // Callback receives result
  void lookup(const std::string &callsign,
              std::function<void(const QRZLookupResult &)> callback);

  // Check if credentials are configured
  bool hasCredentials() const {
    return !username_.empty() && !password_.empty();
  }

private:
  NetworkManager &netMgr_;
  std::string username_;
  std::string password_;
  std::string sessionKey_;
  bool sessionValid_ = false;

  // Establish QRZ session
  void establishSession(std::function<void(bool)> callback);

  // Parse QRZ XML response
  QRZLookupResult parseResponse(const std::string &xml,
                                const std::string &callsign);

  // Extract XML tag value
  std::string extractTag(const std::string &xml, const std::string &tag);
};
