# Getting Started

This page covers installing HamClock-Next on your system and completing initial setup.

> **Developers and advanced users:** For build-from-source instructions, see [Building from Source](Building-from-Source.md).

---

## Install HamClock-Next

Download the latest release from:
- **[GitHub Releases](https://github.com/your-org/hamclock-next/releases)**
- **[SourceForge mirror](https://sourceforge.net/projects/hamclock-next/files/)** *(auto-synced from GitHub)*

### Linux

**Debian / Ubuntu (.deb):**
```bash
sudo dpkg -i hamclock-next_*.deb
hamclock-next
```

**OpenSUSE / Fedora / RHEL (.rpm):**
```bash
sudo rpm -i hamclock-next-*.rpm
hamclock-next
```

**AppImage (any Linux distribution):**
```bash
chmod +x HamClock-Next-*.AppImage
./HamClock-Next-*.AppImage
```

**Raspberry Pi:** The Linux packages above work on Raspberry Pi OS. For console/framebuffer use (no desktop):
```bash
SDL_VIDEODRIVER=kmsdrm hamclock-next --fullscreen
```

### macOS

Download `HamClock-Next-*.dmg`, open it, and drag **HamClock-Next** to your Applications folder. Launch it from there.

### Windows

Run `HamClock-Next-Setup.exe` and follow the installer. A shortcut is placed on your Desktop and Start Menu.

### Browser / Web (no installation)

Launch HamClock-Next with the `--live-web` flag, then open `http://localhost:8080/live.html` in any browser. This also enables remote control from another device on your network.

---

## First Launch

On first launch, the application opens directly to the **Setup screen** where you enter your station information.

If the Setup screen doesn't appear, click the **gear icon (⚙)** in the top-left corner of the Time Panel at any time.

---

## Initial Setup

![Setup modal](images/modal-setup.png)

Fill in these fields on the **Identity** tab:

| Field | Description |
|-------|-------------|
| **Callsign** | Your amateur radio callsign (e.g., `W1AW`) |
| **Grid** | Your Maidenhead grid locator (e.g., `FN31`) |
| **Latitude** | Decimal degrees — auto-filled if you entered a grid locator |
| **Longitude** | Decimal degrees — auto-filled if you entered a grid locator |

After saving, HamClock-Next starts fetching data and displays the main dashboard.

**Why this matters:** Your callsign and location are used by weather widgets, the ONTA distance filter, the Marine tide widget, the Moon and Asteroid trackers, and propagation overlays. Set them first and everything else falls into place.

---

## First-Run Tour

Once the dashboard is running:

1. **Time Panel** (top-left) — shows UTC and local time, your callsign and grid, sunrise/sunset. Click the **gear icon** to reopen Setup.
2. **Six panes** — the dashboard is divided into six rotatable panes, each cycling through one or more widgets.
3. **Map** — the large center area shows the world map with configurable overlays.
4. **Click the top strip of any pane** — opens the widget picker so you can choose what that pane displays.
5. **Press K** — highlights every interactive region on screen with cyan boxes and tooltips.

See [Screen Layout](Layout.md) for a detailed annotated diagram.

---

## Next Steps

- **Set up DX Cluster, Live Spots, ONTA, or Marine?** See the [Widget Setup Guide](Widget-Setup.md) for plain-English configuration steps.
- **Customize your panes?** See [Pane Customization](Pane-Customization.md).
- **Save a layout you like?** See [Presets](Presets.md).
- **Full configuration reference (all JSON keys)?** See [Setup & Configuration](Configuration.md).
