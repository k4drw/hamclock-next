#pragma once

#include <SDL.h>
#include <map>
#include <string>

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

ThemeColors getThemeColors(const std::string &theme,
                           const std::map<std::string, SDL_Color> *overrides =
                               nullptr);
void applyOverrides(ThemeColors &colors,
                    const std::map<std::string, SDL_Color> &overrides);
