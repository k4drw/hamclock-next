# HamClock-Next Usage Guide

HamClock-Next modernizes the interface of the original HamClock. While most features will feel familiar, the interaction model has been optimized for multi-function panes, precise map control, and easier configuration.

## 🏁 Quick Start

1. **Launch**: Run `./hamclock-next`.
2. **Setup**: On the first run, or by clicking the **Gear Icon** in the Time Panel, you will enter the Setup screen.
3. **Finish Setup**: Click "Save" to enter the main dashboard.

---

## 🧭 Navigation & Interaction

### Pane Management (The Content Grid)
The screen is divided into a grid of panes. Each pane can host multiple widgets that rotate automatically.

- **Select Widgets (Top 10%)**: Click the **Title Area (top 10%)** of any pane to open the **Widget Selector**.
  - **Grayed Out Widgets**: In the selector, widgets that are already active in another pane are grayed out and cannot be selected.
  - **Multi-Selection**: You can select multiple widgets for one pane. The pane will then cycle through them automatically.
- **Widget Configuration (Lower Half)**: Clicking in the **lower half** of a widget will open its specific configuration menu (if available).

### World Map Interaction
The map is the primary tool for propagation and location tracking.

- **Set DX Location (Normal Click)**: Click any coordinate on the map to set it as your **DX (Target)** location.
- **Set DE Location (Shift + Click)**: Hold **Shift** and click on the map to set your **DE (Home)** location and grid square.
- **Map View Swap**: Just like other panes, click the top 10% of the map to swap backgrounds (e.g., Seasonal vs. Satellite) or data overlays.

---

## 📡 Feature-Specific Interaction

### DX Cluster
- **Manual Plotting**: By default, the map remains clean.
- **Plot a Spot**: Click on a specific row in the DX Cluster list to immediately plot that station on the map and set it as your current DX target. Click it again to clear the highlight.

### Live Spots (PSK Reporter)
- **Selection Needed**: By default, Live Spots show nothing to keep the map uncluttered.
- **Configuration**: Click the **lower half** of the Live Spots widget to select which bands you wish to visualize.

### Time Panel
- **Gear Icon**: Clicking the gear in the bottom-right opens the global **Main Setup Screen**.
- **Callsign Editing**: Click directly on your **Callsign** to open the inline nameplate editor and color palette.

### Setup → Services Tab
- **GPS Toggle**: Enable the GPS toggle to synchronize the DE location from a running `gpsd` daemon (Linux/Raspberry Pi). Requires `gpsd` to be installed and running with a connected GPS receiver.

---

## ⌨️ Keyboard Shortcuts

| Key            | Action                                       |
| :------------- | :------------------------------------------- |
| `F11`          | Toggle Fullscreen Mode                       |
| `o`            | Toggle Debug Overlay (performance metrics)   |
| `k`            | Cycle Layout Alignment (Left, Center, Right) |
| `q` / `Ctrl+Q` | Quit Application                             |

---

## 🖥️ Command Line Options

| Option | Description |
| :----- | :---------- |
| `-f`, `--fullscreen` | Launch in fullscreen mode |
| `-s`, `--software` | Force software rendering (no OpenGL/MSAA) |
| `--log-level <level>` | Set log verbosity: `debug`, `info`, `warn`, `error` (default: `warn`) |
| `-h`, `--help` | Show help message |

---

## 🛠️ Data & Persistence

- **Auto-Save**: All changes (callsign, selected widgets, map choices) are saved to `config.json` immediately.
- **Headless Mode**: Access the live dashboard remotely via `http://<your-ip>:8080`. The web view refreshes at 1 FPS to remain lightweight.
