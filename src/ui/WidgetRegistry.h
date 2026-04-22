#pragma once
// WidgetRegistry — self-registration system for widget types.
//
// Usage (in each widget's .cpp file, at file scope):
//
//   REGISTER_WIDGET("solar", "Solar", /*scrollable*/false, /*requiresKey*/false, {
//     return std::make_unique<SpaceWeatherPanel>(
//         0, 0, 0, 0, deps.fontMgr, deps.texMgr, deps.solarStore, deps.xrayHistoryStore);
//   })
//
// The macro registers a descriptor + factory lambda that fires at program startup
// via a static initializer in the widget's own translation unit.  No manual enum
// or switch-case additions are needed.

#include "Widget.h"
#include "WidgetDeps.h"
#include <functional>
#include <memory>
#include <string>
#include <vector>

// ---------------------------------------------------------------------------
// WidgetDescriptor — metadata + factory for one widget type
// ---------------------------------------------------------------------------

struct WidgetDescriptor {
  const char *typeId;         // config string: "solar"
  const char *displayName;    // UI label:      "Solar"
  bool isScrollable;          // true → scrollable list (eligible for pane-5 full-height mode)
  bool requiresConfigKey;     // true → only offered when the user has configured a key/account

  // Factory: called once per widget type; result is cached in the widget pool.
  // deps holds non-owning references to all long-lived app resources.
  std::function<std::unique_ptr<Widget>(const WidgetDeps &deps)> factory;

  // One-sentence help text (~80 chars). nullptr until populateWidgetDescriptions() runs.
  // Must be AFTER factory so existing REGISTER_WIDGET brace-init lists are unaffected.
  const char *description = nullptr;
};

// ---------------------------------------------------------------------------
// WidgetRegistry — singleton
// ---------------------------------------------------------------------------

class WidgetRegistry {
public:
  // Construct-on-first-use singleton — safe with static initializers.
  static WidgetRegistry &instance();

  // Called by REGISTER_WIDGET static initializers.  Append-only; no removal.
  void registerWidget(WidgetDescriptor desc);

  // Look up by typeId string.  Returns nullptr if not found.
  const WidgetDescriptor *find(const std::string &typeId) const;

  // All descriptors, optionally including those with requiresConfigKey==true.
  std::vector<const WidgetDescriptor *> getAll(bool includeKeyRequired = false) const;

  // Create a new widget instance via its registered factory.
  // Returns nullptr if typeId is unknown.
  std::unique_ptr<Widget> create(const std::string &typeId, const WidgetDeps &deps) const;

  // Set the help description for a registered widget by typeId.
  // Safe to call any time after the widget's REGISTER_WIDGET has run.
  // No-op (silent) if typeId is not found.
  void setDescription(const std::string &typeId, const char *desc) {
    for (auto &w : widgets_)
      if (w.typeId == typeId) { w.description = desc; return; }
  }

private:
  WidgetRegistry() = default;
  std::vector<WidgetDescriptor> widgets_;
};

// ---------------------------------------------------------------------------
// REGISTER_WIDGET macro
// ---------------------------------------------------------------------------
// Each invocation defines an anonymous-namespace struct whose constructor calls
// WidgetRegistry::instance().registerWidget(...).  The static instance of that
// struct is initialized before main() runs, so the registry is populated by the
// time DashboardContext creates its first widget.
//
// __COUNTER__ is used to give each registration a unique symbol even when
// multiple REGISTER_WIDGET calls appear in the same translation unit (e.g.
// ENVPanel.cpp registers four ENV_* variants).

// Two-level indirection so __COUNTER__ is expanded before token-pasting.
#define REGISTER_WIDGET_IMPL2_(TypeId, DisplayName, Scrollable, RequiresKey, Uid, ...) \
  namespace {                                                                           \
    struct _HC_WReg_##Uid {                                                             \
      _HC_WReg_##Uid() {                                                               \
        WidgetRegistry::instance().registerWidget({                                    \
          TypeId, DisplayName, Scrollable, RequiresKey,                                \
          [](const WidgetDeps &deps) -> std::unique_ptr<Widget> { __VA_ARGS__ }        \
        });                                                                             \
      }                                                                                \
    } _hc_wreg_inst_##Uid;                                                             \
  }

#define REGISTER_WIDGET_IMPL_(TypeId, DisplayName, Scrollable, RequiresKey, Uid, ...) \
  REGISTER_WIDGET_IMPL2_(TypeId, DisplayName, Scrollable, RequiresKey, Uid, __VA_ARGS__)

#define REGISTER_WIDGET(TypeId, DisplayName, Scrollable, RequiresKey, ...)            \
  REGISTER_WIDGET_IMPL_(TypeId, DisplayName, Scrollable, RequiresKey, __COUNTER__, __VA_ARGS__)
