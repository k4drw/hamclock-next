#pragma once

#include "../core/ADIFData.h"
#include "../core/ClublogData.h"
#include "ListPanel.h"
#include <memory>

class ClublogWantedPanel : public ListPanel {
public:
  ClublogWantedPanel(int x, int y, int w, int h, FontManager &fontMgr,
                     std::shared_ptr<ClublogStore> clublogStore,
                     std::shared_ptr<ADIFStore> adifStore);
  ~ClublogWantedPanel() override;

  void update() override;
  void onResize(int x, int y, int w, int h) override;

  std::string getName() const override { return "ClublogWanted"; }
  const char *typeId() const override { return "clublog_wanted"; }
  std::string getDisplayName() const override { return "Most Wanted"; }
  bool isScrollable() const override { return true; }
  bool requiresConfigKey() const override { return false; }

  void render(SDL_Renderer *renderer) override;

private:
  struct DisplayRow {
    int rank;
    std::string name;
    bool needed;  // true if not yet worked
  };

  std::shared_ptr<ClublogStore> clublogStore_;
  std::shared_ptr<ADIFStore> adifStore_;
  std::vector<DisplayRow> displayRows_;

  struct RowCache {
    SDL_Texture *rankTex = nullptr;
    int rankW = 0, rankH = 0;
    SDL_Texture *nameTex = nullptr;
    int nameW = 0, nameH = 0;
    std::string lastName;
    int lastRank = -1;
  };
  std::vector<RowCache> rowCache_;

  void clearRowCache();
  int scrollOffset_ = 0;
  int rowH_ = 14;
};
