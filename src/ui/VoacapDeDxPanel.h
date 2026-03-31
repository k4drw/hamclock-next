#pragma once

#include "../core/HamClockState.h"
#include "../core/PropEngine.h"
#include "../core/SolarData.h"
#include "../services/IonosondeProvider.h"
#include "FontManager.h"
#include "Widget.h"
#include <SDL.h>
#include <memory>
#include <vector>

class VoacapDeDxPanel : public Widget {
public:
  VoacapDeDxPanel(int x, int y, int w, int h, FontManager &fontMgr,
                  std::shared_ptr<HamClockState> state,
                  std::shared_ptr<SolarDataStore> solarStore,
                  std::shared_ptr<IonosondeProvider> ionoProvider);

  void update() override;
  void render(SDL_Renderer *renderer) override;
  void onResize(int x, int y, int w, int h) override;

  std::string getName() const override { return "VoacapDeDx"; }
  const char *typeId() const override { return "voacap_dedx"; }
  std::string getDisplayName() const override { return "Voacap DE-DX"; }
  bool isScrollable() const override { return false; }
  bool requiresConfigKey() const override { return false; }
  std::vector<std::string> getActions() const override { return {}; }
  SDL_Rect getActionRect(const std::string &action) const override;
  nlohmann::json getDebugData() const override;

private:
  FontManager &fontMgr_;
  std::shared_ptr<HamClockState> state_;
  std::shared_ptr<SolarDataStore> solarStore_;
  std::shared_ptr<IonosondeProvider> ionoProvider_;

  // 24 UTC hours x 8 bands (80, 40, 30, 20, 17, 15, 12, 10m)
  float relMatrix_[24][8];
  bool hasTarget_ = false;

  // Caching to avoid recalculating unnecessarily
  double lastDeLat_ = -999;
  double lastDeLon_ = -999;
  double lastDxLat_ = -999;
  double lastDxLon_ = -999;
  double lastSFI_ = -1;
  int lastCalcHour_ = -1;

  void recalculateMatrix();

  static constexpr double BANDS_MHZ[8] = {3.5, 7.0, 10.1, 14.0, 18.1, 21.0, 24.9, 28.0};
  static constexpr const char* BANDS_STR[8] = {"80m", "40m", "30m", "20m", "17m", "15m", "12m", "10m"};
};
