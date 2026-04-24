#pragma once

#include "../core/ADIFData.h"
#include "ListPanel.h"
#include <memory>

class GridProgressPanel : public ListPanel {
public:
  GridProgressPanel(int x, int y, int w, int h, FontManager &fontMgr,
                    std::shared_ptr<ADIFStore> adifStore);
  ~GridProgressPanel() override;

  void update() override;
  void onResize(int x, int y, int w, int h) override;

  std::string getName() const override { return "GridProgress"; }
  const char *typeId() const override { return "grid_progress"; }
  std::string getDisplayName() const override { return "Grid Progress"; }
  bool isScrollable() const override { return true; }
  bool requiresConfigKey() const override { return false; }

  void render(SDL_Renderer *renderer) override;

private:
  struct GridDisplayRow {
    std::string grid4;
    bool confirmed;
  };

  std::shared_ptr<ADIFStore> adifStore_;
  std::vector<GridDisplayRow> displayRows_;

  struct RowCache {
    SDL_Texture *gridTex = nullptr;
    int gridW = 0, gridH = 0;
    SDL_Texture *statusTex = nullptr;
    int statusW = 0, statusH = 0;
    std::string lastGrid;
    bool lastConfirmed = false;
  };
  std::vector<RowCache> rowCache_;

  void clearRowCache();
  int scrollOffset_ = 0;
  int rowH_ = 14;
};
