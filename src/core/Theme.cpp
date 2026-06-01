#include "Theme.h"
#include <algorithm>
#include <map>

ThemeColors getThemeColors(const std::string &theme,
                           const std::map<std::string, SDL_Color> &overrides) {
  ThemeColors colors;

  // Default semantic colors (status)
  colors.success = {50, 200, 50, 255};
  colors.danger = {220, 50, 50, 255};
  colors.warning = {255, 180, 0, 255};
  colors.info = {80, 180, 255, 255};

  if (theme == "dark") {
    colors.bg = {10, 10, 15, 255};
    colors.border = {60, 60, 80, 255};
    colors.text = {220, 220, 220, 255};
    colors.textDim = {100, 100, 110, 255};
    colors.accent = {0, 200, 255, 255};
    colors.rowStripe1 = {25, 25, 30, 255};
    colors.rowStripe2 = {15, 15, 20, 255};
  } else if (theme == "glass") {
    colors.bg = {20, 25, 40, 100}; // semi-transparent
    colors.border = {100, 100, 150, 150};
    colors.text = {255, 255, 255, 255};
    colors.textDim = {180, 180, 200, 255};
    colors.accent = {100, 200, 255, 255};
    colors.rowStripe1 = {30, 35, 50, 150};
    colors.rowStripe2 = {20, 25, 40, 100};
  } else if (theme == "midnight") {
    colors.bg = {5, 5, 20, 255};
    colors.border = {40, 40, 100, 255};
    colors.text = {200, 200, 255, 255};
    colors.textDim = {80, 80, 140, 255};
    colors.accent = {0, 255, 255, 255};
    colors.rowStripe1 = {15, 15, 40, 255};
    colors.rowStripe2 = {10, 10, 30, 255};
  } else if (theme == "amber") {
    colors.bg = {20, 10, 0, 255};
    colors.border = {80, 40, 0, 255};
    colors.text = {255, 176, 0, 255};
    colors.textDim = {180, 100, 0, 255};
    colors.accent = {255, 210, 0, 255};
    colors.rowStripe1 = {35, 20, 0, 255};
    colors.rowStripe2 = {25, 15, 0, 255};
  } else if (theme == "paper") {
    colors.bg = {245, 245, 240, 255};
    colors.border = {200, 200, 190, 255};
    colors.text = {40, 40, 40, 255};
    colors.textDim = {120, 120, 120, 255};
    colors.accent = {0, 100, 200, 255};
    colors.rowStripe1 = {235, 235, 230, 255};
    colors.rowStripe2 = {225, 225, 220, 255};
    // Override semantic colors for light theme contrast
    colors.success = {20, 140, 20, 255};
    colors.danger = {180, 20, 20, 255};
    colors.warning = {160, 110, 0, 255};
    colors.info = {0, 100, 180, 255};
  } else if (theme == "matrix") {
    colors.bg = {0, 0, 0, 255};
    colors.border = {0, 80, 0, 255};
    colors.text = {0, 255, 65, 255};
    colors.textDim = {0, 100, 30, 255};
    colors.accent = {0, 255, 100, 255};
    colors.rowStripe1 = {0, 20, 0, 255};
    colors.rowStripe2 = {0, 10, 0, 255};
  } else {
    // default (dark)
    colors.bg = {20, 20, 25, 255};
    colors.border = {80, 80, 80, 255};
    colors.text = {255, 255, 255, 255};
    colors.textDim = {150, 150, 150, 255};
    colors.accent = {255, 165, 0, 255}; // Orange
    colors.rowStripe1 = {30, 30, 35, 255};
    colors.rowStripe2 = {20, 20, 25, 255};
  }

  // Apply overrides if theme is "custom" or for any theme if overrides exist.
  if (!overrides.empty()) {
    applyOverrides(colors, overrides);
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

std::vector<std::string> getAvailableThemes() {
  return {"dark", "glass", "midnight", "amber", "paper", "matrix", "custom"};
}

bool themeExists(const std::string &theme) {
  auto themes = getAvailableThemes();
  return std::find(themes.begin(), themes.end(), theme) != themes.end();
}
