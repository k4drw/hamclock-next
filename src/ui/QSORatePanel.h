#pragma once

#include "../core/ADIFData.h"
#include "FontManager.h"
#include "Widget.h"
#include <memory>
#include <string>

struct SDL_Renderer;

class QSORatePanel : public Widget {
public:
  QSORatePanel(int x, int y, int w, int h, FontManager &fontMgr,
               std::shared_ptr<ADIFStore> adifStore);
  ~QSORatePanel() override;

  std::string getName() const override { return "QSO Rate"; }
  const char *typeId() const override { return "qso_rate"; }
  std::string getDisplayName() const override { return "QSO Rate"; }

  void update() override;
  void render(SDL_Renderer *renderer) override;
  void onResize(int x, int y, int w, int h) override;

private:
  FontManager &fontMgr_;
  std::shared_ptr<ADIFStore> adifStore_;
};
