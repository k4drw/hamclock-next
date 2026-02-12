#pragma once

#include "../core/OrbitPredictor.h"
#include "FontManager.h"
#include "Widget.h"

class GimbalPanel : public Widget {
public:
  GimbalPanel(int x, int y, int w, int h, FontManager &fontMgr);

  void setPredictor(OrbitPredictor *pred) { predictor_ = pred; }
  void setObserver(double lat, double lon) {
    obsLat_ = lat;
    obsLon_ = lon;
  }

  void update() override;
  void render(SDL_Renderer *renderer) override;
  void onResize(int x, int y, int w, int h) override;

private:
  FontManager &fontMgr_;
  OrbitPredictor *predictor_ = nullptr;
  double obsLat_ = 0, obsLon_ = 0;

  double az_ = 0, el_ = -90;
  bool hasSat_ = false;

  int labelFontSize_ = 12;
  int valueFontSize_ = 18;
};
