
import { z } from "zod";
import { writeFile, mkdir } from "fs/promises";
import { resolve } from "path";
import { existsSync } from "fs";

// ---------------------------------------------------------------------------
// Templates
// ---------------------------------------------------------------------------

function generateHeader(name: string, type: "Widget" | "Service"): string {
    const guard = name.toUpperCase() + "_H";

    if (type === "Widget") {
        return `#pragma once

#include "Widget.h"
#include "../core/HamClockState.h"
#include "FontManager.h"
#include <memory>
#include <string>

class ${name} : public Widget {
public:
  ${name}(int x, int y, int w, int h, FontManager &fontMgr, std::shared_ptr<HamClockState> state);
  ~${name}() override = default;

  void update() override;
  void render(SDL_Renderer *renderer) override;
  void onResize(int x, int y, int w, int h) override;
  
  std::string getName() const override { return "${name}"; }
  std::vector<std::string> getActions() const override;
  nlohmann::json getDebugData() const override;

private:
  FontManager &fontMgr_;
  std::shared_ptr<HamClockState> state_;
  
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
        return `#include "${name}.h"
#include "RenderUtils.h"

${name}::${name}(int x, int y, int w, int h, FontManager &fontMgr, std::shared_ptr<HamClockState> state)
    : Widget(x, y, w, h), fontMgr_(fontMgr), state_(state) {
}

void ${name}::update() {
  // Check for updates, handle timers, etc.
}

void ${name}::render(SDL_Renderer *renderer) {
  // Draw background
  SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
  SDL_Rect rect = {x_, y_, width_, height_};
  SDL_RenderFillRect(renderer, &rect);
  
  // Draw border
  SDL_SetRenderDrawColor(renderer, 100, 100, 100, 255);
  SDL_RenderDrawRect(renderer, &rect);
  
  // Draw content
  if (!fontMgr_.ready()) return;
  
  // Example text
  // int tw, th;
  // SDL_Texture* tex = fontMgr_.renderText(renderer, "Hello", {255,255,255,255}, 12, &tw, &th);
  // ...
}

void ${name}::onResize(int x, int y, int w, int h) {
  Widget::onResize(x, y, w, h);
  // Recalculate layout
}

std::vector<std::string> ${name}::getActions() const {
  return {};
}

nlohmann::json ${name}::getDebugData() const {
  nlohmann::json j;
  // j["state"] = "active";
  return j;
}
`;
    } else {
        return `#include "${name}.h"
#include <iostream>

${name}::${name}(NetworkManager &netMgr, std::shared_ptr<HamClockState> state)
    : netMgr_(netMgr), state_(state) {
}

void ${name}::fetch() {
  std::cout << "${name}: Fetching data..." << std::endl;
  // netMgr_.fetchAsync("https://api.example.com", [this](std::string data) {
  //   // Parse data
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
        instructions = `
Feature '${name}' created successfully!

Next steps to register this Widget:
1. Open 'src/core/WidgetType.h' and add a new enum value:
   ${name.toUpperCase()},

2. Open 'src/ui/WidgetSelector.cpp':
   - Add #include "${name}.h"
   - In createWidget(), add a case:
     case WidgetType::${name.toUpperCase()}: return std::make_unique<${name}>(x, y, w, h, fontMgr, state);
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
