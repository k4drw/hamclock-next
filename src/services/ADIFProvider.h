#pragma once

#include "ProviderBase.h"
#include "../core/ADIFData.h"
#include <filesystem>
#include <memory>

class PrefixManager;

class ADIFProvider : public ProviderBase {
public:
  ADIFProvider(std::shared_ptr<ADIFStore> store, PrefixManager &prefixMgr);

  void fetch(const std::filesystem::path &path);

private:
  void processFile(const std::filesystem::path &path);

  std::shared_ptr<ADIFStore> store_;
  PrefixManager &prefixMgr_;
};
