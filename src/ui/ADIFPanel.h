#pragma once

#include "../core/ADIFData.h"
#include "FontManager.h"
#include "Widget.h"
#include <memory>

struct SDL_Renderer;

class ADIFPanel : public Widget {
public:
  ADIFPanel(int x, int y, int w, int h, FontManager &fontMgr,
            std::shared_ptr<ADIFStore> store);

  void update() override;
  void render(SDL_Renderer *renderer) override;
  void onResize(int x, int y, int w, int h) override;
  bool onMouseWheel(int delta) override;
  void onMouseMove(int mx, int my) override;
  bool onMouseUp(int mx, int my, Uint16 mod) override;

  std::string getName() const override { return "ADIFLog"; }

private:
  void renderStatsView(SDL_Renderer *renderer);
  void renderLogView(SDL_Renderer *renderer);
  std::string formatTime(const std::string &date, const std::string &time) const;

  FontManager &fontMgr_;
  std::shared_ptr<ADIFStore> store_;
  ADIFStats stats_;

  // Scroll state
  int scrollOffset_ = 0;
  int maxScroll_ = 0;
  int rowHeight_ = 14;
  int headerHeight_ = 20;

  // View toggle (future: can switch between stats and log view)
  bool showLogView_ = true;

  // Filter state (band/mode cycle buttons in log view header)
  int filterBandIdx_ = 0;
  int filterModeIdx_ = 0;

  // Mouse drag state for scrollbar
  bool draggingScrollbar_ = false;
  int dragStartY_ = 0;
  int dragStartOffset_ = 0;
};
