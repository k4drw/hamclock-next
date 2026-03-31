#pragma once

#include "../core/ADIFData.h"
#include "../core/PrefixManager.h"
#include "FontManager.h"
#include "Widget.h"
#include <map>
#include <memory>
#include <string>
#include <vector>

struct SDL_Renderer;

class DXCCProgressPanel : public Widget {
public:
  DXCCProgressPanel(int x, int y, int w, int h, FontManager &fontMgr,
                    std::shared_ptr<ADIFStore> store, PrefixManager &prefixMgr);

  std::string getName() const override { return "DXCCProgress"; }
  const char *typeId() const override { return "dxcc_progress"; }
  std::string getDisplayName() const override { return "DXCC Progress"; }
  void update() override;
  void render(SDL_Renderer *renderer) override;
  void onResize(int x, int y, int w, int h) override;
  bool onMouseWheel(int scrollY) override;

private:
  void rebuild(const ADIFStats &stats);

  FontManager &fontMgr_;
  std::shared_ptr<ADIFStore> store_;
  PrefixManager &prefixMgr_;

  int lastTotalQSOs_ = -1;
  int uniqueEntities_ = 0;
  std::map<std::string, int> entitiesByBand_; // band -> unique DXCC count
  std::vector<std::string> recentEntities_;   // newest unique entities

  int scrollOffset_ = 0;
  int maxScroll_ = 0;
  int rowFontSize_ = 11;
};
