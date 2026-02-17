#pragma once

#include "../core/SolarData.h"
#include "../core/Theme.h"
#include "FontManager.h"
#include "Widget.h"

#include <SDL.h>
#include <memory>
#include <string>

class SpaceWeatherPanel : public Widget {
public:
  SpaceWeatherPanel(int x, int y, int w, int h, FontManager &fontMgr,
                    std::shared_ptr<SolarDataStore> store);
  ~SpaceWeatherPanel() override { destroyCache(); }

  void update() override;
  void render(SDL_Renderer *renderer) override;
  void onResize(int x, int y, int w, int h) override;
  bool onMouseUp(int mx, int my, Uint16 mod) override;

  std::string getName() const override { return "SpaceWeather"; }
  std::vector<std::string> getActions() const override;
  bool performAction(const std::string &action) override;
  SDL_Rect getActionRect(const std::string &action) const override;
  nlohmann::json getDebugData() const override;

private:
  void destroyCache();
  static SDL_Color colorForK(int k);
  static SDL_Color colorForSFI(int sfi);
  static SDL_Color colorForNOAAScale(int scale, const ThemeColors &themes);

  FontManager &fontMgr_;
  std::shared_ptr<SolarDataStore> store_;

  static constexpr int kNumItems = 15;
  static constexpr int kItemsPerPage = 4;
  struct Item {
    std::string label;
    std::string value;
    std::string lastValue;
    SDL_Color valueColor = {255, 255, 255, 255};
    SDL_Color lastValueColor = {0, 0, 0, 0};
    SDL_Texture *labelTex = nullptr;
    SDL_Texture *valueTex = nullptr;
    int labelW = 0, labelH = 0;
    int valueW = 0, valueH = 0;
  };
  Item items_[kNumItems];

  int currentPage_ = 0;
  uint32_t lastPageUpdate_ = 0;

  int labelFontSize_ = 10;
  int valueFontSize_ = 24;
  int lastLabelFontSize_ = 0;
  int lastValueFontSize_ = 0;
  bool dataValid_ = false;
};
