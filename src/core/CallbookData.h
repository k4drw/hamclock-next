#pragma once

#include <mutex>
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
  CallbookData get() const { std::lock_guard<std::mutex> lk(mu_); return data_; }
  void set(const CallbookData &d) { std::lock_guard<std::mutex> lk(mu_); data_ = d; }

private:
  mutable std::mutex mu_;
  CallbookData data_;
};
