# Keyboard Shortcuts

HamClock-Next uses minimal keyboard shortcuts at the global level. Most interaction is mouse/touch-based.

---

## Global Shortcuts

| Key | Action |
|-----|--------|
| `?` | Toggle **in-app help panel** — scrollable overlay of all shortcuts and widget descriptions. |
| `K` | Toggle **highlight mode** — draws cyan bounding boxes around all interactive regions. |
| `O` | Toggle **debug overlay** — shows real-time FPS, CPU, and network statistics. |
| `F11` | Toggle **fullscreen mode**. |
| `Ctrl+Q` | Quit HamClock-Next (native builds). |

Closing the window (SDL_QUIT / OS close button) also exits cleanly.

---

## ? Key — In-App Help Panel

Pressing **?** (Shift+/) with no modal open toggles the built-in help panel.

When open:
- A scrollable overlay lists all **keyboard shortcuts**, **mouse/touch controls**, and every **widget** with its one-line description.
- Scroll with **↑ / ↓ arrow keys**, **PageUp / PageDown**, or the **mouse wheel**.
- Press **?** again or **Escape** to close.

The help panel is always up-to-date — widget descriptions are loaded directly from the widget registry at runtime.

---

## Satellite Tracking Shortcuts

When the **Satellite** widget is active (not the map overlay):
- Use **↑ / ↓ arrow keys** or the **mouse wheel** to scroll through the list of upcoming passes.
- Use **PageUp / PageDown** to jump by full pages.

---

## K Key — Interactive Region Highlight Mode

Pressing **K** with no modal open toggles highlight mode.

![K key highlight mode](images/key-highlight-mode.png)

When active:
- Every interactive region on screen is outlined in **cyan**
- Hovering over an action region shows a **yellow tooltip** with the action name (e.g., "Open Setup", "Next Widget")
- Hovering over a widget body shows its **description** as a second tooltip line in blue
- Press **K** again to dismiss

This mode is especially useful for:
- Discovering hidden controls on unfamiliar widgets
- Learning what each widget does at a glance
- Orientation when first setting up the dashboard
- Debugging widget action coverage (developer use)

### Coverage Note

Not all widgets currently return full action lists — some show empty or partial action tooltips in K mode. All widgets do show their description tooltip when hovered. Improving action coverage is an ongoing effort.

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
- In-widget configuration (World Clock labels, Marine station IDs, etc.)

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
