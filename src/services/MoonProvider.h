#pragma once

#include "../core/MoonData.h"
#include <memory>

class MoonProvider {
public:
  MoonProvider(std::shared_ptr<MoonStore> store);

  void update(double lat, double lon);

private:
  std::shared_ptr<MoonStore> store_;
};
