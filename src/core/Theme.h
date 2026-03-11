#pragma once

#include <SDL.h>
#include <map>
#include <string>
#include <vector>

struct ThemeColors {
  SDL_Color bg;
  SDL_Color border;
  SDL_Color text;
  SDL_Color textDim;
  SDL_Color accent;
  SDL_Color rowStripe1;
  SDL_Color rowStripe2;
  // Semantic status colors
  SDL_Color success; // Good / operational
  SDL_Color danger;  // Error / severe
  SDL_Color warning; // Caution
  SDL_Color info;    // Informational / secondary accent
};

// Use an empty map as the default for overrides
ThemeColors getThemeColors(const std::string &theme,
                           const std::map<std::string, SDL_Color> &overrides = {});

void applyOverrides(ThemeColors &colors,
                    const std::map<std::string, SDL_Color> &overrides);

/**
 * @brief Returns a list of all built-in theme IDs.
 */
std::vector<std::string> getAvailableThemes();

/**
 * @brief Checks if a given theme ID is valid.
 */
bool themeExists(const std::string &theme);
