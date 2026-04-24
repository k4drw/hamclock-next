#pragma once

#include "FontManager.h"
#include "ListPanel.h"
#include <string>
#include <ctime>

struct SDL_Renderer;

class LoTWSyncPanel : public ListPanel {
public:
  LoTWSyncPanel(int x, int y, int w, int h, FontManager &fontMgr);
  ~LoTWSyncPanel() override;

  void update() override;
  void render(SDL_Renderer *renderer) override;
  void onResize(int x, int y, int w, int h) override;

  std::string getName() const override { return "LoTWSync"; }
  const char *typeId() const override { return "lotw_sync"; }
  std::string getDisplayName() const override { return "LoTW Sync"; }
  bool isScrollable() const override { return false; }
  bool requiresConfigKey() const override { return false; }

  void setSyncStatus(time_t lastSync, int qsoCount, const std::string &error);

private:
  void clearCache();

  time_t lastSyncTime_ = 0;
  int qsosSynced_ = 0;
  std::string lastError_;

  struct StatusCache {
    SDL_Texture *tex = nullptr;
    int w = 0, h = 0;
    std::string lastText;
    SDL_Color lastColor = {0, 0, 0, 0};
  };

  StatusCache syncTimeCache_;
  StatusCache qsoCountCache_;
  StatusCache statusCache_;
  StatusCache errorCache_;
  StatusCache configMsgCache_;
};
