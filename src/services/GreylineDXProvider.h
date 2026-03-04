#pragma once

#include "../core/GreylineDXData.h"
#include "../core/PrefixManager.h"
#include <memory>

class GreylineDXProvider {
public:
  GreylineDXProvider(PrefixManager &pm, std::shared_ptr<GreylineDXStore> store);

  // Perform one calculation pass and update store
  void update();

private:
  PrefixManager &pm_;
  std::shared_ptr<GreylineDXStore> store_;
};
