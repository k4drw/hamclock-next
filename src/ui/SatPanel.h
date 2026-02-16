#pragma once

#include "../core/OrbitPredictor.h"
#include "FontManager.h"
#include "TextureManager.h"
#include "Widget.h"

#include <string>
#include <vector>

struct SDL_Renderer;
struct SDL_Texture;

class SatPanel : public Widget {
public:
  SatPanel(int x, int y, int w, int h, FontManager &fontMgr,
           TextureManager &texMgr);
  ~SatPanel() override { destroyCache(); }

  void update() override;
  void render(SDL_Renderer *renderer) override;
  void onResize(int x, int y, int w, int h) override;

  // Set the predictor to display (non-owning). Pass nullptr to clear.
  void setPredictor(OrbitPredictor *pred) { predictor_ = pred; }

  // True if a predictor is set and ready.
  bool hasPredictor() const { return predictor_ && predictor_->isReady(); }

  // Semantic Debug API
  std::string getName() const override { return "SatPanel"; }
  std::vector<std::string> getActions() const override;
  SDL_Rect getActionRect(const std::string &action) const override;
  nlohmann::json getDebugData() const override;

private:
  void destroyCache();
  void renderPolarPlot(SDL_Renderer *renderer, float cx, float cy, int radius);

  FontManager &fontMgr_;
  TextureManager &texMgr_;
  OrbitPredictor *predictor_ = nullptr;

  // Text lines: name, rise/set info, az/el, TLE age
  static constexpr int kNumLines = 5;
  SDL_Texture *lineTex_[kNumLines] = {};
  int lineW_[kNumLines] = {};
  int lineH_[kNumLines] = {};
  std::string lineText_[kNumLines];
  std::string lastLineText_[kNumLines];
  int lineFontSize_[kNumLines] = {};
  int lastLineFontSize_[kNumLines] = {};

  // Compass labels for polar plot (N, NE, E, SE, S, SW, W, NW)
  static constexpr int kNumCompass = 8;
  SDL_Texture *compassTex_[kNumCompass] = {};
  int compassW_[kNumCompass] = {};
  int compassH_[kNumCompass] = {};
  int compassFontSize_ = 0;
  int lastCompassFontSize_ = 0;

  // Pass trajectory for polar plot (az/el pairs)
  struct AzElPoint {
    double az;
    double el;
  };
  std::vector<AzElPoint> passTrack_;
  AzElPoint currentPos_ = {0, 0};
  bool satAboveHorizon_ = false;

  // Font sizes
  int nameFontSize_ = 14;
  int infoFontSize_ = 10;

  // Throttle updates to avoid recalculating every frame
  std::time_t lastUpdate_ = 0;
};
