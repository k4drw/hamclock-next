#pragma once

#include "../core/DXClusterData.h"
#include "ListPanel.h"
#include "FontManager.h"
#include <memory>
#include <string>

struct SDL_Renderer;

class GreylineDXFilterPanel : public ListPanel {
public:
  GreylineDXFilterPanel(int x, int y, int w, int h, FontManager &fontMgr,
                        std::shared_ptr<DXClusterDataStore> dxcStore);
  ~GreylineDXFilterPanel() override;

  void update() override;
  void render(SDL_Renderer *renderer) override;
  void onResize(int x, int y, int w, int h) override;

  std::string getName() const override { return "GreylineSpots"; }
  const char *typeId() const override { return "greyline_spots"; }
  std::string getDisplayName() const override { return "Greyline Spots"; }
  bool isScrollable() const override { return true; }
  bool requiresConfigKey() const override { return false; }

private:
  std::shared_ptr<DXClusterDataStore> dxcStore_;

  bool isNearGreyline(double lat) const;
};
