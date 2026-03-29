
import { z } from "zod";
import { writeFile, mkdir } from "fs/promises";
import { resolve } from "path";
import { existsSync } from "fs";

// Convert CamelCase to snake_case for typeId generation
function toSnakeCase(name: string): string {
    return name
        .replace(/([A-Z]+)([A-Z][a-z])/g, "$1_$2")
        .replace(/([a-z\d])([A-Z])/g, "$1_$2")
        .toLowerCase();
}

// ---------------------------------------------------------------------------
// Templates
// ---------------------------------------------------------------------------

function generateHeader(name: string, type: "Widget" | "Service"): string {
    const guard = name.toUpperCase() + "_H";

    if (type === "Widget") {
        const typeId = toSnakeCase(name);
        return `#pragma once

#include "Widget.h"
#include "FontManager.h"
#include <memory>
#include <string>

class ${name} : public Widget {
public:
  ${name}(int x, int y, int w, int h, FontManager &fontMgr);
  ~${name}() override = default;

  void update() override;
  void render(SDL_Renderer *renderer) override;
  void onResize(int x, int y, int w, int h) override;

  std::string getName() const override { return "${name}"; }
  const char *typeId() const override { return "${typeId}"; }
  std::string getDisplayName() const override { return "${name}"; }
  bool isScrollable() const override { return false; }
  bool requiresConfigKey() const override { return false; }

private:
  FontManager &fontMgr_;

  // Custom members
};
`;
    } else {
        return `#pragma once

#include "../network/NetworkManager.h"
#include "../core/HamClockState.h"
#include <memory>
#include <string>

class ${name} {
public:
  ${name}(NetworkManager &netMgr, std::shared_ptr<HamClockState> state);
  
  void fetch();
  void update();
  
  bool isReady() const;

private:
  NetworkManager &netMgr_;
  std::shared_ptr<HamClockState> state_;
  
  // Custom members
};
`;
    }
}

function generateSource(name: string, type: "Widget" | "Service"): string {
    if (type === "Widget") {
        const typeId = toSnakeCase(name);
        return `#include "${name}.h"
#include "WidgetRegistry.h"
#include "WidgetDeps.h"
#include "../core/Theme.h"

${name}::${name}(int x, int y, int w, int h, FontManager &fontMgr)
    : Widget(x, y, w, h), fontMgr_(fontMgr) {
}

void ${name}::update() {
  // Check for updates, handle timers, etc.
}

void ${name}::render(SDL_Renderer *renderer) {
  renderChrome(renderer);
  renderTitle(renderer, fontMgr_, getDisplayName().c_str());

  if (!fontMgr_.ready()) return;

  // ThemeColors themes = getThemeColors(theme_);
  // auto* cat = fontMgr_.catalog();
  // IMPORTANT: Never hardcode SDL_Color values. Always use ThemeColors tokens
  // (themes.text, themes.accent, themes.success, etc.).
  // cat->drawText(renderer, "Hello", x_ + 10, y_ + 30, themes.text, FontStyle::Fast);
}

bool ${name}::onMouseDown(int mx, int my, Uint16 mod) {
  // Return true if you handle the click
  return false;
}

bool ${name}::onMouseUp(int mx, int my, Uint16 mod, int clicks) {
  // Return true if you handle the release
  return false;
}

void ${name}::onResize(int x, int y, int w, int h) {
  Widget::onResize(x, y, w, h);
  // Recalculate layout
}

// Self-registration: widget appears in Setup picker automatically.
// Replace deps.fontMgr with the actual constructor arguments needed.
REGISTER_WIDGET("${typeId}", "${name}", /*scrollable*/false, /*requiresKey*/false, {
  return std::make_unique<${name}>(0, 0, 0, 0, deps.fontMgr);
})
`;
    } else {
        return `#include "${name}.h"
#include <iostream>

${name}::${name}(NetworkManager &netMgr, std::shared_ptr<HamClockState> state)
    : netMgr_(netMgr), state_(state) {
}

void ${name}::fetch() {
  // SAFETY: Never capture raw 'this' in a fetchAsync lambda — the service may
  // be destroyed before the callback fires. Use the shared_ptr<AsyncState> pattern:
  //
  // struct AsyncState {
  //   std::mutex mutex;
  //   ${name}Data data;
  //   std::atomic<bool> ready{false};
  // };
  // std::shared_ptr<AsyncState> async_ = std::make_shared<AsyncState>();
  //
  // netMgr_.fetchAsync("https://api.example.com", [async = async_](std::string body) {
  //   std::lock_guard<std::mutex> lk(async->mutex);
  //   // parse body into async->data
  //   async->ready.store(true);
  // });
}

void ${name}::update() {
  // Periodic updates
}

bool ${name}::isReady() const {
  return true;
}
`;
    }
}

// ---------------------------------------------------------------------------
// Implementation
// ---------------------------------------------------------------------------

export async function scaffoldFeature(
    rootPath: string,
    name: string,
    type: "Widget" | "Service"
): Promise<{ files: string[]; instructions: string }> {
    // Determine paths
    const srcDir = resolve(rootPath, "src");
    let targetDir = "";

    if (type === "Widget") {
        targetDir = resolve(srcDir, "ui");
    } else {
        targetDir = resolve(srcDir, "services");
    }

    // Ensure directory exists
    if (!existsSync(targetDir)) {
        throw new Error(`Target directory does not exist: ${targetDir}`);
    }

    const headerPath = resolve(targetDir, `${name}.h`);
    const sourcePath = resolve(targetDir, `${name}.cpp`);

    // Check for collisions
    if (existsSync(headerPath) || existsSync(sourcePath)) {
        throw new Error(`File already exists for feature ${name}`);
    }

    // Generate content
    const headerContent = generateHeader(name, type);
    const sourceContent = generateSource(name, type);

    // Write files
    await writeFile(headerPath, headerContent);
    await writeFile(sourcePath, sourceContent);

    // Create instructions for registration
    let instructions = "";
    if (type === "Widget") {
        const typeId = toSnakeCase(name);
        instructions = `
Feature '${name}' created successfully!

One step to register this Widget:
1. Add 'src/ui/${name}.cpp' to CMakeLists.txt in the HAMCLOCK_SOURCES list.

The widget self-registers at startup via REGISTER_WIDGET("${typeId}", ...) and will
appear automatically in the Setup → Widgets picker and be available to all pane slots.

Customise the generated files:
- Adjust constructor arguments and factory lambda in REGISTER_WIDGET to pass
  the correct deps (deps.fontMgr, deps.solarStore, etc. from WidgetDeps.h).
- Set isScrollable=true if this widget is a scrollable list (pane 5 full-height mode).
- Set requiresConfigKey=true if the widget needs an API key to be configured.
- Update getDisplayName() in the header to a human-readable label.
`;
    } else {
        instructions = `
Feature '${name}' created successfully!

Next steps to register this Service:
1. Open 'src/main.cpp':
   - Add #include "services/${name}.h"
   - Instantiate it in the main loop:
     ${name} ${name.toLowerCase()}(netManager, state);
     ${name.toLowerCase()}.fetch();
`;
    }

    return { files: [headerPath, sourcePath], instructions };
}
