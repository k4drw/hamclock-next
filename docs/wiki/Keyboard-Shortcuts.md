# Keyboard Shortcuts

HamClock-Next uses minimal keyboard shortcuts at the global level. Most interaction is mouse/touch-based.

---

## Global Shortcuts

| Key | Action |
|-----|--------|
| `K` | Toggle **highlight mode** — draws cyan bounding boxes around all interactive regions. |
| `O` | Toggle **debug overlay** — shows real-time FPS, CPU, and network statistics. |
| `F11` | Toggle **fullscreen mode**. |
| `Ctrl+Q` | Quit HamClock-Next (native builds). |

Closing the window (SDL_QUIT / OS close button) also exits cleanly.

---

## K Key — Interactive Region Highlight Mode

Pressing **K** with no modal open toggles highlight mode.

![K key highlight mode](images/key-highlight-mode.png)

When active:
- Every interactive region on screen is outlined in **cyan**
- Each box displays a **tooltip label** identifying what it is (e.g., "Open Setup", "Next Widget", "DX Cluster Toggle")
- Press **K** again to dismiss

This mode is especially useful for:
- Discovering hidden controls on unfamiliar widgets
- Orientation when first setting up the dashboard
- Debugging widget action coverage (developer use)

### Coverage Note

Not all widgets currently return full action lists — some show empty or partial tooltips in K mode. Improving tooltip coverage is an ongoing effort. See the stretch goal in the project roadmap.

---

## In-Dialog Keyboard Input

Within modals and setup screens, standard text-editing keys apply:

| Key | Action |
|-----|--------|
| `Tab` | Move to next input field |
| `Enter` / `Return` | Confirm / close modal |
| `Escape` | Cancel / close modal |
| `Backspace` | Delete character before cursor |
| `Left` / `Right` | Move cursor in text field |
| `Home` / `End` | Jump to start/end of field |

These apply inside:
- Setup screen (callsign, grid, lat/lon fields)
- DX Cluster Setup (host, port, login)
- Preset name entry
- Countdown label / target time

---

## Mouse / Touch Controls

All primary interaction in HamClock-Next is pointer-based:

| Action | Effect |
|--------|--------|
| Click pane top strip | Open widget picker for that pane |
| Click ⚙ (gear) in Time Panel | Open Setup modal |
| Click ★ (star) in Time Panel | Open Presets modal |
| Click map overlay control | Cycle map overlays |
| Click SDO wavelength button | Open SDO wavelength picker |
| Click pane arrows | Manually advance/retreat widget rotation |
| Click K-mode box | (No action — display only) |
