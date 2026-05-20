---
name: hc-new-widget
description: Add widgets and features to HamClock-Next using REGISTER_WIDGET pattern. Uses hamclock-bridge MCP for code scaffolding, templates, and checklists. Guides design → code → verify → document workflow.
---

# Add Feature to HamClock-Next

Workflow for new widgets, providers, data stores, and services. Delegates code generation to MCP tools to avoid duplication.

---

## Design Interview

Before writing code, understand the feature scope:

**What are you building?**
- Widget (renders data, inherits from Widget)
- Provider (fetches data, runs async)
- Data store (plain struct, thread-safe access)
- Service (lifecycle component, singleton)
- API endpoint (WebServer REST route)

**Scope & Complexity Check**
- 1-2 sentence description
- Component count? (Provider + Panel? New data struct?)
- External data fetch? Auth needed?
- Map/graph rendering? Modal dialogs?

**If complexity high** (6+ components, 3+ async dependencies, graph overlay):
- Suggest splitting into phases or simpler variant
- Example: "Start with static display (no external data). Once working, add real-time provider."

**If acceptable** → proceed to **Scaffolding**.

---

## Base Class Selection

Before scaffolding, choose your base class:

- **Widget**: Custom layouts (maps, charts, lists from scratch, space weather displays). Standard override set: `update()`, `render()`, plus optional `onMouseDown`, `onMouseUp`, `onRightClick`, `onMouseMove`, `onKeyDown`, `onTextInput`, `onMouseWheel`, `onResize`.
- **ListPanel**: Tabular/list-style data (LoTWSyncPanel, AlertsPanel, log viewers). Constructor: `ListPanel(x, y, w, h, fontMgr, title_string, headers_vector)`. Required override: `populateRows(std::vector<ListPanel::Row>&)`.

Both require: `update()`, `render(SDL_Renderer*)`, `getName()`, `getDisplayName()`, `typeId()`. Optional: `isScrollable()`, `requiresConfigKey()`, `getDebugData()`.

---

## Scaffolding

### Step 1: Get Boilerplate Template

Call MCP to fetch C++ boilerplate for your feature type:

```bash
# Ask Claude to call:
# mcp__hamclock-bridge__get_scaffolding_template(name: "YourFeatureName")

# Returns:
# - Data struct template (src/core/{Name}Data.h)
# - Provider skeleton (src/services/{Name}Provider.cpp/.h)
# - Panel skeleton (src/ui/{Name}Panel.cpp/.h)
# - Store wrapper (src/core/{Name}Store.h)
```

### Step 2: Generate & Write Files

Call MCP to create files in the repository:

```bash
# mcp__hamclock-bridge__scaffold_feature(name: "YourFeatureName", type: "Widget")

# Writes directly to src/core/, src/services/, src/ui/
# Returns: file paths, integration checklist
```

### Step 3: REGISTER_WIDGET Integration

Add to your widget's `.cpp` file:

```cpp
REGISTER_WIDGET("{snake_case}", "{Display Name}", true, false, {
  auto p = std::make_unique<{Name}Panel>(0, 0, 0, 0, deps...);
  // Setup calls if needed (setFilter, setObserver, etc.)
  return p;
})
```

**Key points:**
- `REGISTER_WIDGET` macro handles self-registration at startup
- Pass `deps` struct containing FontManager, stores, providers, etc. (see `src/ui/WidgetDeps.h` for all 50+ fields)
- Post-creation callbacks remain in DashboardContext (unchanged)
- No manual if/else chain in DashboardContext—registry handles creation
- **CRITICAL**: Both CMakeLists.txt AND REGISTER_WIDGET macro required — missing either causes silent failure

### Step 4: CMakeLists Integration

```cmake
# Add to HAMCLOCK_SOURCES in CMakeLists.txt
list(APPEND HAMCLOCK_SOURCES
  src/ui/{Name}Panel.cpp
  src/services/{Name}Provider.cpp  # if fetching data
)
```

### Step 5: Build & Verify

```bash
cmake --build build --target hamclock-next -j10
```

✅ Clean build? Continue.
❌ Errors? Fix before next step.

---

## Standards Verification

### C++ Safety (cpp-safety-audit patterns)

**Raw Pointers & Lifetime**
- ❌ Don't capture raw `this` in async callbacks
- ✅ Use `shared_ptr<AsyncState>` or `alive_` flag

Example safe async pattern:
```cpp
auto alive = alive_;
NetworkManager::getInstance().fetchAsync(url,
  [this, alive](std::string response) {
    if (!alive->load(std::memory_order_acquire)) return;
    // Safe to touch members now
  });
```

**Thread Safety**
- ✅ Use `std::atomic<>` for shared state
- ✅ Mutex-protect map/vector access
- ❌ No raw pointers to heap objects shared between threads

**Resource Leaks**
- ✅ SDL_Texture → `SDL_DestroyTexture` in dtor
- ✅ All heap → smart pointers (`shared_ptr`, `unique_ptr`)

### Theming (No Hardcoded Colors)

```cpp
void YourPanel::render(SDL_Renderer* renderer) {
  ThemeColors themes = getThemeColors(theme_);
  
  // Use theme, never hardcode SDL_Color{r, g, b, a}
  SDL_SetRenderDrawColor(renderer, themes.bg.r, themes.bg.g, themes.bg.b, 255);
  SDL_Rect rect = {...};
  SDL_RenderFillRect(renderer, &rect);
}
```

Available tokens: `bg`, `text`, `textDim`, `accent`, `success`, `warning`, `danger`, `border`, `rowStripe1`, `rowStripe2`.

### Config Persistence

**If state survives restart:**

1. Add field to `AppConfig` struct (`src/core/ConfigManager.h`)
2. Load from JSON in `ConfigManager::load()`
3. Save to JSON in `ConfigManager::save()`
4. Widget load/save via `ConfigManager::getInstance()`

Example:
```cpp
// Load on first run
AppConfig cfg = ConfigManager::getInstance().getConfig();
textInput_.setValue(cfg.widgetNotes);

// Save on change
cfg.widgetNotes = textInput_.getValue();
ConfigManager::getInstance().save(cfg);
```

### TextBox Support (if text input)

Required methods in Panel class:
```cpp
bool onKeyDown(SDL_Keycode key, Uint16 mod) override {
  if (activeTextBox_) {
    if (key == SDLK_RETURN) { submitInput(); return true; }
    if (key == SDLK_ESCAPE) { cancelInput(); return true; }
    // ... other keys (Backspace, Ctrl-A/C/V/X/Z/Y)
  }
  return false;
}

bool onTextInput(const char* text) override {
  if (activeTextBox_) { appendChar(text); return true; }
  return false;
}
```

### Error Handling (External APIs)

```cpp
void {Name}Provider::fetch() {
  auto alive = alive_;
  NetworkManager::getInstance().fetchAsync(url,
    [this, alive](std::string response) {
      if (!alive->load(std::memory_order_acquire)) return;
      
      // Empty response
      if (response.empty()) {
        LOG_W("{Name}Provider", "Empty response");
        data_.fetched = true;
        data_.valid = false;  // Show "unavailable" not "loading"
        return;
      }
      
      // Parse error
      try {
        auto j = nlohmann::json::parse(response);
        // Extract, validate, update data_
        data_.valid = true;
        data_.fetched = true;
      } catch (const std::exception& e) {
        LOG_E("{Name}Provider", "Parse: %s", e.what());
        data_.valid = false;
        data_.fetched = true;
      }
    });
}
```

Widget render logic:
```cpp
void {Name}Panel::render(SDL_Renderer *renderer) {
  auto data = store_->get();
  
  if (!data.fetched) {
    cat->drawText(renderer, "Loading...", x_, y_, themes.textDim, FontStyle::Micro);
  } else if (!data.valid) {
    cat->drawText(renderer, "API unavailable", x_, y_, themes.warning, FontStyle::Micro);
  } else {
    renderData(renderer, data);
  }
}
```

### Store Class (Thread-Safe Data Access)

```cpp
// src/core/{Name}Store.h
class {Name}Store {
public:
  static std::shared_ptr<{Name}Store> instance();
  {Name}Data get() const;      // Snapshot copy (thread-safe)
  void update(const {Name}Data& data);
private:
  mutable std::mutex mutex_;
  {Name}Data data_;
};

// src/core/{Name}Store.cpp
std::shared_ptr<{Name}Store> {Name}Store::instance() {
  static auto inst = std::make_shared<{Name}Store>();
  return inst;
}

{Name}Data {Name}Store::get() const {
  std::lock_guard<std::mutex> lock(mutex_);
  return data_;  // Copy
}

void {Name}Store::update(const {Name}Data& data) {
  std::lock_guard<std::mutex> lock(mutex_);
  data_ = data;
}
```

### Undo/Redo (if text editing)

```cpp
// In {Name}Panel.h
private:
  std::vector<std::string> undoStack_, redoStack_;
  static constexpr int MAX_UNDO = 20;
  
  void pushUndo() {
    if (undoStack_.size() >= MAX_UNDO) undoStack_.erase(undoStack_.begin());
    undoStack_.push_back(noteInput_.getValue());
    redoStack_.clear();
  }
```

```cpp
// In onKeyDown()
if (ctrl && key == SDLK_z && !undoStack_.empty()) {
  redoStack_.push_back(noteInput_.getValue());
  noteInput_.setValue(undoStack_.back());
  undoStack_.pop_back();
  return true;
}

if (ctrl && key == SDLK_y && !redoStack_.empty()) {
  undoStack_.push_back(noteInput_.getValue());
  noteInput_.setValue(redoStack_.back());
  redoStack_.pop_back();
  return true;
}
```

### Modal Dialogs

```cpp
// In render()
if (modalActive_) renderModal(renderer);

void {Name}Panel::renderModal(SDL_Renderer *renderer) {
  // Semi-transparent overlay
  SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
  SDL_SetRenderDrawColor(renderer, 0, 0, 0, 128);
  SDL_RenderFillRect(renderer, &overlay);
  
  // Modal box (centered)
  SDL_SetRenderDrawColor(renderer, themes.bg.r, themes.bg.g, themes.bg.b, 255);
  SDL_RenderFillRect(renderer, &modalBox);
  
  // Title + content
  auto *cat = fontMgr_.catalog();
  cat->drawText(renderer, "Title", x, y, themes.accent, FontStyle::SmallBold);
  
  // OK/Cancel buttons
  okRect_ = {...};
  cancelRect_ = {...};
}

bool {Name}Panel::onMouseUp(int mx, int my, Uint16 mod, int clicks) {
  if (modalActive_) {
    if (point_in_rect(mx, my, okRect_)) { submitModal(); return true; }
    if (point_in_rect(mx, my, cancelRect_)) { cancelModal(); return true; }
  }
  return false;
}

bool {Name}Panel::onKeyDown(SDL_Keycode key, Uint16 mod) {
  if (modalActive_) {
    if (key == SDLK_RETURN) { submitModal(); return true; }
    if (key == SDLK_ESCAPE) { cancelModal(); return true; }
    return modalInput_.onKeyDown(key, mod);
  }
  return false;
}
```

### Map Overlay Integration

```cpp
// In {Name}Panel.h
public:
  void renderMapOverlay(SDL_Renderer *renderer, const MapContext& ctx);
  bool isMapOverlayActive() const { return overlayActive_; }
```

```cpp
// In MapWidget::render()
if (spaceWxPanel && spaceWxPanel->isMapOverlayActive()) {
  spaceWxPanel->renderMapOverlay(renderer, mapCtx);
}
```

### WASM Build Guards

```cpp
// Desktop-only code (curl, file I/O, native dialogs)
#ifndef __EMSCRIPTEN__
  NetworkManager::getInstance().fetchAsync(url, callback);
#else
  // Web: use Emscripten fetch API
  emscripten_fetch_attr_t attr;
  emscripten_fetch_attr_init(&attr);
  emscripten_fetch(&attr, url.c_str());
#endif
```

### Rate Limiting & Backoff

For providers that hit rate-limited APIs:

```cpp
// In {Name}Provider::update()
time_t now = time(nullptr);
if (now < nextFetch_) return;  // Still backoff window

// Fetch attempt
bool success = fetch(...);
if (!success) {
  // Exponential backoff: 30s → 60s → 120s (cap at 600s)
  backoffSeconds_ = std::min(600, backoffSeconds_ * 2);
  nextFetch_ = now + backoffSeconds_;
} else {
  // Reset on success
  backoffSeconds_ = 30;
  nextFetch_ = now + 300;  // Normal 5min interval
}
```

---

## Debugging Patterns

Common failure modes and diagnostics:

**Widget doesn't appear in dashboard:**
- Check `/debug/widgets`: curl http://localhost:8080/debug/widgets | jq '.widgets[] | .typeId'
- Missing? REGISTER_WIDGET not reached (check CMakeLists.txt or macro syntax)
- Wrong name? Check typeId string matches config

**Widget appears but stays blank (no data):**
- Is provider running? Check provider::update() called (add LOG_I("Fetching..."))
- Is store populated? Call getDebugData() to inspect latest() data
- Is render() reading from store? Verify store.mutex lock + getLatest() call

**Async callback crashes:**
- Did you capture raw `this`? Use `alive_` flag check before touching members
- Did you forget thread-safe access? Use store->mutex() or atomic

**Memory leak on exit:**
- Check SDL_Texture cleanup in dtor (use MemoryMonitor::destroyTexture)
- Check event data passed via SDL_PushEvent is freed in handler
- Check shared_ptrs not stored in global state

**Texture constantly recreates (perf issue):**
- Use StatusCache pattern: cache texture, invalidate only on data change
- Example: LoTWSyncPanel caches row textures per fetch generation

---

## Final Checklist

Call MCP to get feature-specific checklist:

```bash
# mcp__hamclock-bridge__new_feature_checklist(
#   name: "YourFeatureName",
#   type: "widget"  # or "endpoint", "config_field"
# )
```

Returns: Exact registration steps for your feature type.

**Manual verification:**
- [ ] Code compiles: `cmake --build build --target hamclock-next -j10`
- [ ] Widget in `/debug/widgets` endpoint
- [ ] No hardcoded colors (all `ThemeColors`)
- [ ] Async callbacks: no raw `this` (use `alive_` flag)
- [ ] Error handling: empty response, parse errors, timeout
- [ ] Config persistence: AppConfig + ConfigManager
- [ ] Textures cleaned up in dtor
- [ ] MCP endpoints updated (if new REST route)

---

## Documentation Updates

### 1. Commit Message
Write to `working_docs/commit-message.txt`:
```
feat(widgets): Add {Feature} widget with {capability}

- Widget displays {what it shows}
- Provider fetches from {data source}
- Integrates with {related component}
- REGISTER_WIDGET pattern, fully themed
```

### 2. PROJECT_STATUS.md
Add to `working_docs/PROJECT_STATUS.md`:
```
[YYYY-MM-DD] Implemented {Feature}: {description}
```

### 3. CHANGELOG.md
Prepend:
```markdown
## [v1.7.0] — YYYY-MM-DD

### Added
- **{Feature} Widget** — {one-sentence description}
  - Sub-item 1
  - Sub-item 2
```

### 4. RELEASE_NOTES_vX.X.md
If v1.X exists, update `working_docs/RELEASE_NOTES_v1.X.md`:
```markdown
## {Feature} Widget

**What it does:**
{Brief description}

**How to use:**
{User-facing instructions}

**Technical notes:**
{API/config details if relevant}
```

---

## References

- **WidgetDeps struct**: `src/ui/WidgetDeps.h` — all 50+ dependency fields available to factory lambdas
- **Widget base class**: `src/ui/Widget.h` — virtual methods, optional overrides
- **ListPanel for tables**: `src/ui/ListPanel.h` — when to use, constructor pattern
- **MCP Code Generation**: `scaffold_feature`, `get_scaffolding_template`, `new_feature_checklist`
- **C++ Safety**: Run `/cpp-safety-audit` to scan for lifetime bugs
- **Code Review**: Run `/code-reviewer` for design audit
- **Project Context**: `/mcp__hamclock-bridge__project_context` for architecture overview
- **Widget Examples**: `src/ui/DXClusterPanel.cpp`, `src/ui/SpaceWeatherPanel.cpp`, `src/ui/LoTWSyncPanel.cpp` (ListPanel example)
- **Theming**: `src/core/Theme.h` — all color tokens
- **Debugging**: `/debug/widgets` endpoint shows all registered widgets; `getDebugData()` in your panel for state inspection
