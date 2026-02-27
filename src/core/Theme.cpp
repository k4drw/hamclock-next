#include "Theme.h"
#include "ConfigManager.h"
#include <map>

ThemeColors getThemeColors(const std::string &theme,
                           const std::map<std::string, SDL_Color> *overrides) {
  ThemeColors colors;
  if (theme == "dark") {
    colors.bg = {10, 10, 15, 255};
    colors.border = {60, 60, 80, 255};
    colors.text = {220, 220, 220, 255};
    colors.textDim = {100, 100, 110, 255};
    colors.accent = {0, 200, 255, 255};
    colors.rowStripe1 = {25, 25, 30, 255};
    colors.rowStripe2 = {15, 15, 20, 255};
  } else if (theme == "glass") {
    colors.bg = {20, 25, 40, 200}; // semi-transparent
    colors.border = {100, 100, 150, 150};
    colors.text = {255, 255, 255, 255};
    colors.textDim = {180, 180, 200, 255};
    colors.accent = {100, 200, 255, 255};
    colors.rowStripe1 = {30, 35, 50, 150};
    colors.rowStripe2 = {20, 25, 40, 100};
  } else {
    // default (original HamClock-like colors often use dark backgrounds)
    colors.bg = {20, 20, 25, 255};
    colors.border = {80, 80, 80, 255};
    colors.text = {255, 255, 255, 255};
    colors.textDim = {150, 150, 150, 255};
    colors.accent = {255, 165, 0, 255}; // Orange
    colors.rowStripe1 = {30, 30, 35, 255};
    colors.rowStripe2 = {20, 20, 25, 255};
  }

  // Semantic colors are the same across all themes by default
  colors.success = {50, 200, 50, 255};
  colors.danger = {220, 50, 50, 255};
  colors.warning = {255, 180, 0, 255};
  colors.info = {80, 180, 255, 255};

  // Apply overrides if theme is "custom" or for any theme if overrides exist
  // We prioritize the "custom" theme.
  if (theme == "custom") {
    if (overrides) {
      applyOverrides(colors, *overrides);
    } else {
      applyOverrides(colors,
                     ConfigManager::instance().getConfig().colorOverrides);
    }
  }

  return colors;
}

void applyOverrides(ThemeColors &colors,
                    const std::map<std::string, SDL_Color> &overrides) {
  auto apply = [&](const std::string &key, SDL_Color &target) {
    auto it = overrides.find(key);
    if (it != overrides.end()) {
      target = it->second;
    }
  };

  apply("bg", colors.bg);
  apply("border", colors.border);
  apply("text", colors.text);
  apply("textDim", colors.textDim);
  apply("accent", colors.accent);
  apply("rowStripe1", colors.rowStripe1);
  apply("rowStripe2", colors.rowStripe2);
  apply("success", colors.success);
  apply("danger", colors.danger);
  apply("warning", colors.warning);
  apply("info", colors.info);
}
