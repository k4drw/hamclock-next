#pragma once

#include "../core/MoonData.h"
#include "FontManager.h"
#include "TextureManager.h"
#include "Widget.h"
#include <memory>
#include <vector>

struct SDL_Renderer;

// 2-day moon elevation chart with DE (green) and DX (blue) curves.
// Shows the next mutual-above-horizon window.
class EMEToolPanel : public Widget {
public:
  EMEToolPanel(int x, int y, int w, int h, FontManager &fontMgr,
               TextureManager &texMgr, std::shared_ptr<MoonStore> store);

  void update() override;
  void render(SDL_Renderer *renderer) override;
  void onResize(int x, int y, int w, int h) override;
  void onMouseMove(int mx, int my) override;

  // Called by the app when the DX location changes.
  void setDxLocation(double lat, double lon) {
    dxLat_ = lat;
    dxLon_ = lon;
  }
  void setDeLocation(double lat, double lon) {
    deLat_ = lat;
    deLon_ = lon;
  }

  std::string getName() const override { return "EMEToolPanel"; }
  const char *typeId() const override { return "eme_tool"; }
  std::string getDisplayName() const override { return "EME Tool"; }
  nlohmann::json getDebugData() const override;

private:
  FontManager &fontMgr_;
  TextureManager &texMgr_;
  std::shared_ptr<MoonStore> store_;
  MoonData currentData_;

  double deLat_ = 0.0, deLon_ = 0.0;
  double dxLat_ = 0.0, dxLon_ = 0.0;

  // Pre-computed elevation curves (96 points = every 30 min for 48 h)
  static constexpr int kPoints = 96;
  static constexpr int kIntervalSec = 1800; // 30 minutes
  struct ElevPoint { double deEl; double dxEl; };
  std::vector<ElevPoint> curve_;
  std::time_t curveBase_ = 0; // UTC epoch of curve_[0]

  // Chart layout cached for mouse tracking
  SDL_Rect chartRect_ = {0, 0, 0, 0};

  // Tooltip state
  struct Tooltip {
    bool visible = false;
    int index = -1;
    int x = 0, y = 0;
  } tooltip_;

  // Next mutual window start (0 if none in 48h window)
  std::time_t nextWindow_ = 0;

  void recomputeCurve();
};
