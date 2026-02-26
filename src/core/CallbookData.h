#pragma once

#include <string>

struct CallbookData {
  std::string callsign;
  std::string name;
  std::string address;
  std::string city;
  std::string state;
  std::string zip;
  std::string country;
  std::string grid;
  float lat = 0.0f;
  float lon = 0.0f;

  // QSL Info
  bool lotw = false;
  bool eqsl = false;
  std::string bureau;

  // Bio/Meta
  std::string bioUrl;
  std::string imageUrl;
  std::string source;     // e.g., "Callook + HamDB"
  std::string expiryDate; // YYYY-MM-DD
  std::string frn;

  bool valid = false;
};

class CallbookStore {
public:
  const CallbookData &get() const { return data_; }
  void set(const CallbookData &d) { data_ = d; }

private:
  CallbookData data_;
};
