#include "WidgetRegistry.h"
#include <cstdio>

WidgetRegistry &WidgetRegistry::instance() {
  static WidgetRegistry s_instance;
  return s_instance;
}

void WidgetRegistry::registerWidget(WidgetDescriptor desc) {
  widgets_.push_back(std::move(desc));
}

const WidgetDescriptor *WidgetRegistry::find(const std::string &typeId) const {
  for (const auto &d : widgets_) {
    if (typeId == d.typeId)
      return &d;
  }
  return nullptr;
}

std::vector<const WidgetDescriptor *>
WidgetRegistry::getAll(bool includeKeyRequired) const {
  std::vector<const WidgetDescriptor *> result;
  result.reserve(widgets_.size());
  for (const auto &d : widgets_) {
    if (includeKeyRequired || !d.requiresConfigKey)
      result.push_back(&d);
  }
  return result;
}

std::unique_ptr<Widget>
WidgetRegistry::create(const std::string &typeId, const WidgetDeps &deps) const {
  const WidgetDescriptor *desc = find(typeId);
  if (!desc) {
    std::fprintf(stderr, "WidgetRegistry: unknown typeId '%s'\n", typeId.c_str());
    return nullptr;
  }
  if (desc->factory)
    return desc->factory(deps);
  return nullptr;
}
