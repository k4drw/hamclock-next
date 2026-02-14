#pragma once

#include <SDL.h>
#include <string>

struct ThemeColors {
  SDL_Color bg;
  SDL_Color border;
  SDL_Color text;
  SDL_Color textDim;
  SDL_Color accent;
  SDL_Color rowStripe1;
  SDL_Color rowStripe2;
};

inline ThemeColors getThemeColors(const std::string &theme) {
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
  return colors;
}
