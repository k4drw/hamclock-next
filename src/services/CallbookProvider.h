#pragma once

#include "../core/CallbookData.h"
#include "../network/NetworkManager.h"
#include <functional>
#include <memory>
#include <string>

class CallbookProvider {
public:
  using DataCb = std::function<void(const CallbookData &data)>;

  CallbookProvider(NetworkManager &net, std::shared_ptr<CallbookStore> store);

  // Main entry point for a lookup
  void lookup(const std::string &callsign);

private:
  NetworkManager &net_;
  std::shared_ptr<CallbookStore> store_;
};
