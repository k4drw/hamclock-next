#pragma once

#include "Widget.h"
#include "TextInput.h"
#include "../core/ConfigManager.h"
#include <vector>

class WorldClockPanel : public Widget {
public:
  WorldClockPanel(int x, int y, int w, int h, ConfigManager &configMgr, FontManager &fontMgr);

  void update() override;
  void render(SDL_Renderer *renderer) override;
  void onResize(int x, int y, int w, int h) override;
  bool onMouseDown(int mx, int my, Uint16 mod, int clicks) override;
  bool onMouseUp(int mx, int my, Uint16 mod, int clicks) override;
  void onMouseMove(int mx, int my) override;
  bool onTextInput(const char *text) override;
  bool onKeyDown(SDL_Keycode key, Uint16 mod) override;
  
  std::string getName() const override { return "WorldClockPanel"; }
  std::string getDisplayName() const override { return "World Clock"; }
  const char *typeId() const override { return "world_clock"; }

private:
  ConfigManager &configMgr_;
  FontManager &fontMgr_;
  
  bool configuring_ = false;
  int editingIndex_ = -1; // -1 = none, 0-3 = label being edited
  
  struct UIState {
    SDL_Rect gearRect;
    SDL_Rect doneRect;
    SDL_Rect cancelRect;
    SDL_Rect activeRects[4];
    SDL_Rect labelRects[4];
    SDL_Rect plusRects[4];
    SDL_Rect minusRects[4];
  } ui_;

  TextInput labelInput_;
  std::vector<WorldClockEntry> tempClocks_;
  
  void renderMain(SDL_Renderer *renderer);
  void renderConfig(SDL_Renderer *renderer);
  void recalcLayout();
};
