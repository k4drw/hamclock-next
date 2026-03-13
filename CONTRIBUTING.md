# Contributing to HamClock-Next

HamClock-Next is a community-maintained SDL2/C++20 rewrite of HamClock, dedicated to the memory of its original author Elwood Downey WB0OEW (Silent Key 29 January 2026).

---

## Quick Start

```bash
# Build (requires CMake, SDL2, SDL_ttf, SDL_image, libcurl, libpredict)
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --target hamclock-next

# Run
./build/hamclock-next

# WASM (requires Emscripten)
./scripts/build-wasm.sh

# Windows cross-build (requires dockcross)
./scripts/build-win64.sh
```

Full build instructions: see [Getting Started](docs/wiki/Getting-Started.md).

---

## Branch & PR Workflow

| Branch | Purpose |
| ------ | ------- |
| `master` | Stable releases & hotfixes |
| `v1.0` | Current release line |
| `feature/*` | New work and enhancements |

1. Fork the repository.
2. Branch from the current stable branch (e.g. `v1.0`) or `master` for hotfixes.
3. Keep changes focused — one logical change per PR.
4. Reference any parity item or issue in the PR description.

---

## Coding Conventions

### Architecture

- **Widget pair pattern**: every new widget needs a `{Name}Panel.cpp/.h` (rendering) and `{Name}Provider.cpp/.h` (data fetching). See `src/ui/AuroraGraphPanel.cpp` for a complete example.
- Data structures live in `src/core/` as `{Name}Data.h` or `{Name}Store.h`.
- Providers fetch via `NetworkManager` — never make direct network calls from a Panel.
- All `render()` calls happen on the main thread. Providers run on background threads. Use `std::mutex` or `std::atomic` for shared state.

### Required Widget Interface Overrides

Every `Widget` subclass must override:

```cpp
std::string getName() const override { return "My Widget"; }
void render(SDL_Renderer *renderer) override;
void onResize(int x, int y, int w, int h) override; // call Widget::onResize() first
```

Optional overrides as needed: `update()`, `onMouseUp()`, `onMouseWheel()`, `onMouseMove()`, `onKeyDown()`.

### Forbidden Patterns

- **No blocking loops** — no `while(!done)` or `sleep()` on the main thread. Use state machines for dialogs.
- **No hover-only UI** — all interactions must work on touchscreens (click/tap only).
- **No hardcoded colors** — use `getThemeColors(theme_)` from `src/core/Theme.h`.
- **No hardcoded pixel positions** — use `x_`, `y_`, `width_`, `height_` from `Widget` base class.
- **No direct SDL_ttf calls** — use `FontManager` / `FontCatalog`. Never load your own TTF file.
- **No `std::cout`** — use the project logger or `SDL_Log`.
- **No new global state** — extend `AppConfig` for config, `AppContext` for runtime state.

### UI Standards

- Button labels: `Done` and `Cancel` (PascalCase, not OK/Apply).
- Main modal buttons: 100×34 px. Small widget setup buttons: 80×28 px.
- All interactive regions should be registered via `UIRegistry` so `/debug/widgets` can enumerate them.
- Touch targets must be at least 30×30 px.

---

## Parity Policy

Feature parity with the original HamClock is at **100% (82/82)**. New PRs must not regress any parity item.

New features beyond parity are welcome but should:
1. Not degrade performance on Raspberry Pi (the primary embedded target).
2. Work in all three build targets: Linux framebuffer, WASM browser, and Windows x64.
3. Be documented in `docs/wiki/` and listed in `CHANGELOG.md`.

Parity status is tracked in `.mcp/data/parity_v2.json`.

---

## AI-Assisted Development

This project uses AI coding assistants (Claude Code) for implementation. AI-generated changes follow the same review standards as human contributions. The `CLAUDE.md` file at the repo root defines the operational rules for AI agents working in this codebase — reviewers may reference it when evaluating AI-authored PRs.

---

## Commit Message Format

```
type: brief imperative description

- bullet list of specific changes
- one line per logical change
```

Types: `feat`, `fix`, `docs`, `refactor`, `chore`. No trailing period on the title line. No `TODO` or placeholder text in commit messages.

---

## Documentation

After any user-visible change, update:
- `docs/wiki/` — the relevant feature doc
- `CHANGELOG.md` — add to `[Unreleased]` section
- `working_docs/PROJECT_STATUS.md` — required by project rules

---

## License

HamClock-Next is released under the GNU General Public License (GPL). By contributing you agree your changes will be distributed under the same license. See `LICENSE.md`.
