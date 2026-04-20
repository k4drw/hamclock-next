# Getting Started

This page covers installing HamClock-Next and completing the first setup.

> **Developers and advanced users:** For build-from-source instructions, see [Building from Source](Building-from-Source.md).

---

## Install HamClock-Next

Download the latest release from:
- **[GitHub Releases](https://github.com/your-org/hamclock-next/releases)**
- **[SourceForge mirror](https://sourceforge.net/projects/hamclock-next/files/)** *(auto-synced from GitHub)*

### Linux

**Step 1 — Pick your package**

Release filenames follow this pattern:

```
hamclock-next_<version>_<variant>_<arch>.deb
hamclock-next-<version>-<variant>.<arch>.rpm
```

**Architecture (`<arch>`)** - which kind of processor your machine has:

| If `uname -m` prints… | Download the… | Typical hardware |
|---|---|---|
| `x86_64` | `amd64` package | Desktop PC, laptop, Intel NUC |
| `aarch64` | `arm64` package | Raspberry Pi 4 / 5, modern ARM SBCs |
| `armv7l` | `armhf` package | Raspberry Pi 3 (32-bit OS), Pi Zero 2W |

Not sure? Open a terminal and run:
```bash
uname -m
```

**Build variant (`<variant>`)** - how the app talks to your display:

| Variant | Use when… |
|---|---|
| `unified` | Use this if you have a normal desktop or want one package that also works on the console later |
| `fb0` | Use this if your Pi boots straight to a text prompt and you want HamClock-Next to start automatically |

**When in doubt, choose `unified`.** It is the easiest option.

---

**Step 2 — Install**

**Debian / Ubuntu / Raspberry Pi OS (.deb):**
```bash
sudo dpkg -i hamclock-next_*.deb
hamclock-next
```

**OpenSUSE / Fedora / RHEL (.rpm):**
```bash
sudo rpm -i hamclock-next-*.rpm
hamclock-next
```

---

**Step 3 - Raspberry Pi console mode**

If your Pi boots to the command line with no desktop, tell HamClock-Next to draw directly to the screen:

```bash
SDL_VIDEODRIVER=kmsdrm hamclock-next --fullscreen
```

> `SDL_VIDEODRIVER=kmsdrm` tells the program to draw straight to the screen instead of using a desktop window system.

If you installed the `fb0` package, a systemd service handles this automatically — HamClock-Next starts on boot with no extra configuration needed.

### macOS

Download `HamClock-Next-*.dmg`, open it, and drag **HamClock-Next** to your Applications folder. Launch it from there.

### Windows

Run `HamClock-Next-Setup.exe` and follow the installer. A shortcut is placed on your Desktop and Start Menu.

### Browser / Web (no installation)

Launch HamClock-Next with the `--live-web` flag, then open `http://localhost:8080/live` in any browser. This also enables remote control from another device on your network.

---

## First Launch

On first launch, the app opens the **Setup screen** so you can enter your station information.

If the Setup screen does not appear, click the **gear icon (⚙)** in the top-left corner of the Time Panel.

The Setup screen shows the **Live Web URL** where you can access HamClock-Next from a browser on another device. For example: `http://192.168.1.xxx:8080/live`. This makes it easy to set up remote control without looking up your IP address manually.

### PDF Manual

A printable PDF manual is available with each release on GitHub: `HamClock-Next-Manual.pdf`. It includes all the documentation in a single downloadable file. Find it on the [Releases](https://github.com/k4drw/hamclock-next/releases) page.

---

## Initial Setup

![Setup modal](images/modal-setup.png)

Fill in these fields on the **Identity** tab:

| Field | Description |
|-------|-------------|
| **Callsign** | Your amateur radio callsign (e.g., `W1AW`) |
| **Grid** | Your [Maidenhead grid locator](Glossary.md#maidenhead-grid-square) (e.g., `FN31`) |
| **Latitude** | Decimal degrees — auto-filled if you entered a grid locator |
| **Longitude** | Decimal degrees — auto-filled if you entered a grid locator |

After saving, HamClock-Next starts fetching data and shows the main dashboard.

**Why this matters:** Your callsign and location are used by weather widgets, distance filters, tide and tracking widgets, and propagation overlays.

---

## First-Run Tour

Once the dashboard is running:

1. **Time Panel** (top-left) - shows UTC or local time, your callsign and grid, and sunrise/sunset. Click the **gear icon** to reopen Setup.
2. **Six panes** - the dashboard is divided into six areas, and each one can show one or more widgets.
3. **Map** - the large center area shows the world map with optional overlays.
4. **Click the top strip of any pane** - opens the widget picker for that pane.
5. **Press K** - highlights every clickable region on screen.

See [Screen Layout](Layout.md) for a detailed annotated diagram.

---

## Keeping HamClock-Next Up to Date

HamClock-Next checks for updates automatically at startup. When a newer version is available, a notification appears on screen. You have three choices:

- **Update** — downloads the new package in the background. When it finishes, you can install it with the same `dpkg -i` or `rpm -i` command you used the first time.
- **Not Now** — dismisses the notification until next launch.
- **Skip** — suppresses the reminder for that specific version.

To check for updates manually at any time, open Setup → **Updates** tab.

---

## Next Steps

- **Set up DX Cluster, Live Spots, ONTA, or Marine?** See the [Widget Setup Guide](Widget-Setup.md) for plain-English configuration steps.
- **Customize your panes?** See [Pane Customization](Pane-Customization.md).
- **Save a layout you like?** See [Presets](Presets.md).
- **Full configuration reference (all JSON keys)?** See [Setup & Configuration](Configuration.md).
