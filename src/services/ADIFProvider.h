#pragma once

#include "../core/ADIFData.h"
#include <filesystem>
#include <memory>

class ADIFProvider {
public:
  ADIFProvider(std::shared_ptr<ADIFStore> store);

  void fetch(const std::filesystem::path &path);

private:
  void processFile(const std::filesystem::path &path);

  std::shared_ptr<ADIFStore> store_;
};
