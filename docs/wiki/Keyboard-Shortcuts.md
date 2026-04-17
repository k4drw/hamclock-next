# Keyboard Shortcuts

HamClock-Next is mostly controlled with the mouse or touch screen. The keyboard is optional.

---

## Global Shortcuts

| Key | Action |
|-----|--------|
| `?` | Toggle the built-in help panel. |
| `K` | Highlight clickable areas on the screen. |
| `O` | Show or hide the debug overlay. |
| `F11` | Toggle **fullscreen mode**. |
| `Ctrl+Q` | Quit HamClock-Next (native builds). |

Closing the window (SDL_QUIT / OS close button) also exits cleanly.

---

## ? Key — In-App Help Panel

Pressing **?** (Shift+/) toggles the built-in help panel.

When open:
- A scrollable overlay lists keyboard shortcuts, mouse/touch controls, and every widget with a short description.
- Scroll with **↑ / ↓ arrow keys**, **PageUp / PageDown**, or the mouse wheel.
- Press **?** again or **Escape** to close.

The help panel is always up to date because widget descriptions are loaded at runtime.

---

## Satellite Tracking Shortcuts

When the **Satellite** widget is active (not the map overlay):
- Use **↑ / ↓ arrow keys** or the mouse wheel to scroll through the list of upcoming passes.
- Use **PageUp / PageDown** to jump by full pages.

---

## K Key — Interactive Region Highlight Mode

Pressing **K** toggles highlight mode.

![K key highlight mode](images/key-highlight-mode.png)

When active:
- Every interactive region on screen is outlined in cyan
- Hovering over an action region shows a tooltip with the action name
- Hovering over a widget body shows its description
- Press **K** again to dismiss

This mode is especially useful for:
- Discovering hidden controls on unfamiliar widgets
- Learning what each widget does at a glance
- Orientation when first setting up the dashboard
- Debugging widget action coverage

### Coverage Note

Not all widgets currently return full action lists. Some show empty or partial action tooltips in K mode, but all widgets do show their description tooltip when hovered.

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
| `Arrows` | (In Widget Picker) Navigate the 4-column grid of tiles |
| `Space` | (In Widget Picker) Toggle or select the highlighted tile |

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
