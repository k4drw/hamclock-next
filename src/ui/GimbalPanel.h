#pragma once

#include "../core/OrbitPredictor.h"
#include "../core/RotatorData.h"
#include "FontManager.h"
#include "Widget.h"

#include <memory>

struct SDL_Renderer;

class GimbalPanel : public Widget {
public:
  GimbalPanel(int x, int y, int w, int h, FontManager &fontMgr,
              std::shared_ptr<RotatorDataStore> rotatorStore = nullptr);

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
  std::shared_ptr<RotatorDataStore> rotatorStore_;
  double obsLat_ = 0, obsLon_ = 0;

  // Display values (either from rotator or satellite prediction)
  double az_ = 0, el_ = -90;
  bool hasSat_ = false;
  bool hasRotator_ = false;
  bool rotatorConnected_ = false;

  // Satellite prediction (for comparison or auto-track)
  double satAz_ = 0, satEl_ = -90;

  int labelFontSize_ = 12;
  int valueFontSize_ = 18;
};
