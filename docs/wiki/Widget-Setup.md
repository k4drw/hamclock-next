# Widget Setup Guide

This guide explains how to configure each widget in plain language. If you know amateur radio but don't consider yourself a computer expert, start here.

**Before anything else:** Open Setup (click the ⚙ gear icon in the top-left corner) and go to the **Identity** tab. Enter your callsign, grid locator, and location. Many widgets use your QTH automatically — if this isn't set, weather, distance filters, tides, and tracking widgets won't know where you are.

---

## Widgets That Work Automatically

These widgets fetch data on their own the moment you add them to a pane. No configuration needed.

**Space Weather**
- Solar, Aurora, Aurora Graph, DRAP, Solar Storm, DST Index, Ionosonde
- Band Conditions, NCDXF Beacons
- History Flux, History KP, History SSN
- K-Index Trend, SFI 30-Day Trend, NOAA Severity Scales, Space Weather Alerts

**Weather & Environment**
- DE Weather, DX Weather, Forecast, Hurricane, Lightning, Tropo, Meteor

**Tracking** *(uses your QTH location)*
- Moon, EME Tool, Greyline DX, Greyline Windows, Santa Tracker

**Info & Utilities**
- DE Info, DX Info, Stopwatch, Sys Info, ADIF Log, Contests, VOACAP DE-DX
- Solar Cycle 25 Tracker, DXCC Progress

**Timers** *(configure once inside the widget)*
- Countdown — click the gear icon in the panel to set a label and target time
- Reminder — click the gear icon to set reminder text and time

---

## Band Conditions

Shows a color-coded summary of HF band conditions from 160m to 10m. It works automatically — no configuration needed.

**Expanded detail view:**
Click the pane's expand arrow (or double-click the pane title bar) to maximize the Band Conditions tile. In the maximized view, a detailed propagation logic breakdown appears below the band summary — it shows the specific factors (solar flux index, K-index, time of day, path geometry) that produced each band's rating. This is useful if you want to understand *why* a band is shown as open or closed, not just that it is.

---

## DX Cluster

The DX Cluster connects to a global spotting network and shows you who's working what, right now.

**It works immediately** using a public cluster (dxusa.net) — just add it to a pane.

**To use your club's cluster or a preferred server:**
Setup → **Spotting** tab → DX Cluster section → enter the hostname and port number.

**To include spots from WSJT-X on your own computer:**
Setup → **Spotting** tab → enable **WSJT-X Mode**. HamClock listens for UDP packets on port 2237 (the WSJT-X default).

**Filtering and Performance:**
- **Band filter**: Click a band label in the color legend at the bottom of the DX Cluster panel. Click it again to clear the filter.
- **Duplicate Hiding**: (New) Go to Setup → **Spotting** tab and enable **Hide Duplicates**. Only the latest spot for any given callsign on a specific band will be shown, reducing clutter during busy events.

**Tip:** The cluster shows mode badges (CW, SSB, FT8, FT4, RTTY) based on the spot frequency.

**DXCC Award Tracking (N / B / W badges):**
If you have an ADIF log loaded (see the ADIF widget), each cluster spot is automatically tagged:
- **N** — you have *never* worked that DXCC entity. New one for your log.
- **B** — you've worked that entity before, but not on this band.
- **W** — you've worked it on this band but it's not yet confirmed (unworked/unconfirmed).

No badge means you have a confirmed QSO with that entity on this band already. This makes it easy to spot what's worth chasing at a glance without checking a separate log.

---

## Live Spots (PSK Reporter / RBN / WSPR)

Live Spots shows you signals being heard around the world, updated continuously.

**It works immediately** — the default source is PSK Reporter.

**In-Widget Configuration:**
- Click a **band label** directly in the widget header to toggle that band on/off.
- Click the **Source** name (e.g., PSK) to cycle between PSK Reporter, RBN (Reverse Beacon Network), and WSPR.

**Configuration Screen Options:**
Setup → **Spotting** tab → Live Spots section:
- **Use Callsign**: Show actual callsigns instead of grid locators.
- **Max Age**: Controls how many minutes old a spot can be before it disappears.

---

## On The Air (ONTA) — POTA & SOTA Activators

ONTA shows operators activating parks (POTA) or summits (SOTA). Clicking a spot plots the path on the map.

- **Filters**: Click the gear icon on the ONTA panel to choose **POTA**, **SOTA**, or **All**.
- **Distance**: Set a maximum distance filter in the ONTA settings to only see activators within range of your QTH.
- **Legend**: Double-height mode shows a band color legend at the bottom for quick reference.

---

## Watchlist & Alerts

The Watchlist lets you monitor specific callsigns in the DX cluster stream and get notified when they appear.

**Setting it up:**
1. Open Setup → **Spotting** tab → Watchlist section.
2. Enter the callsigns you want to watch, separated by commas or spaces (e.g., `VK2TT, JA1ABC, VP9GE`). You can paste up to 256 characters at once.
3. Save.

**What happens when a watched callsign is spotted:**
- An on-screen alert fires in the Alerts tile (if you have it in a pane).
- A voice alert plays if `flite` is installed (see [Voice Alerts](#voice-alerts-text-to-speech) below).

**You do not need the Watchlist tile visible for alerts to fire.** Monitoring runs in the background at all times — even if you have no pane showing the Watchlist widget. This means you can run a DX Cluster in your main pane while still receiving alerts for your watched callsigns without keeping a separate tile for it.

**Paste in bulk:** You can paste a full list of callsigns directly into the Watchlist setup field — commas, spaces, or a mix both work as separators.

---

## Greyline DX

Shows DXCC entities whose sunrise or sunset is happening right now — the moments when HF propagation to that country is most likely to be open.

Each row shows a countdown in minutes to the greyline peak. As the window gets close, the row changes color to draw your attention. Entities near peak are the highest priority for a contact — the window is only about 30 minutes wide.

The tile works automatically as soon as you add it to a pane. It uses your QTH and astronomical calculations — no configuration needed.

---

## World Clock

Display up to four specific timezones with labels.

1. Add **World Clock** to a pane.
2. Click the **gear icon** inside the widget.
3. For each of the 4 slots:
   - Enter a **City Label** (e.g., "London").
   - Set the **UTC Offset** (e.g., 0, -5, +1).
4. Save to see the new times.

---

## Big Clock

A full-pane clock for high visibility from across the room.

1. Add **Big Clock** to a pane.
2. Click the **gear icon** to configure:
   - **Mode**: Digital or Analog.
   - **Format**: 12-hour or 24-hour.
   - **UTC**: Lock to UTC or use your default local timezone.
   - **Sec/Date**: Toggle seconds and date display.
   - **Color**: Adjust the hue slider to match your theme.

---

## Marine (Tides & Buoys)

The Marine widget shows tidal data and sea conditions from the nearest NOAA station.

- **Auto-Lookup**: Click the **Find Closest** button in the Marine setup (gear icon) to automatically identify and select the nearest NOAA tide station and buoy.
- **Manual Entry**: Enter specific NOAA station or buoy IDs if you prefer a different location.

---

## SDO (Solar Dynamics Observatory)

SDO shows live solar imagery in various wavelengths.

Click the **gear icon** in the SDO panel to:
- Choose a **wavelength** (0193, 0171, 0304, 1600, 1700, HMIB, HMIIC).
- Enable **Rotating Display** to cycle wavelengths automatically.
- Enable **PFSS Overlay** for magnetic field lines.
- Enable **Movie Mode** for a time-lapse loop.

---

## Satellite Tracking

1. Add the Satellite widget to a pane.
2. Click the **gear icon** in the panel → select a satellite from the list.
3. The map shows the ground track and footprint automatically.

**Rotator Control:** If using a rotator via `rotctld`, enable **Auto-Track** in the Rotator settings (Setup → Network) to point your antenna at the target automatically during a pass.

---

## Rig Control (Hamlib)

Shows your rig's frequency and mode, and allows tuning from HamClock.

- **Requirements**: Requires `rigctld` (Hamlib) running on your system.
- **Sync**: Enable **Auto-Tune** in Setup → Rig to automatically set your rig's frequency when you click a DX Cluster spot or band button.

---

## Aux Clock

Shows a second clock next to your local time.

- **Quick Cycle**: Click the widget itself to cycle through common timezones (UTC, EST, CST, MST, PST, CET, JST, AEST).
- **Custom Offset**: Set a custom offset and label in Setup → Appearance → Aux Clock.
- **Sidereal Mode**: Advanced users can enable Sidereal time display through the configuration menu.

---

## RSS News Ticker

A scrolling news headline strip can appear along the bottom edge of the screen, pulling headlines from any RSS feed (amateur radio news sites, space weather alerts, etc.).

**To enable it:**
- REST API: `GET /set_rss?enabled=1`
- Or add `"rssEnabled": true` to your config file manually.

**To choose a feed:** Set `"rssUrl"` in your config file to the URL of any RSS feed. The default feed covers amateur radio news headlines.

**To disable it:** `GET /set_rss?enabled=0` or set `"rssEnabled": false`.

The ticker is silent — it only shows text. If the feed can't be reached, the strip disappears rather than showing an error.

---

## Alarms

HamClock-Next can play an audio alert at a time you choose — useful as a morning reminder to check into a net, or to mark the start of a contest.

**Daily alarm** — fires every day at the same time:
1. Open Setup → **Timers** tab.
2. Set the hour and minute.
3. Choose **UTC** or **local time**.
4. Enable the alarm and save.

**One-time alarm** — fires once at a specific date and time, then disables itself:
1. Open Setup → **Timers** tab.
2. Enable the one-time alarm and enter the target date/time.
3. Save.

If no sound plays when the alarm fires, check that your system audio is working and that HamClock-Next was not built with `--no-audio`. See [Building from Source](Building-from-Source.md) if you built it yourself.

---

## Voice Alerts (Text-to-Speech)

HamClock-Next can speak alerts out loud using your computer's speakers.

Spoken alerts fire for:
- **Watchlist hits** — a callsign you're monitoring appears in the DX cluster
- **Solar flares** — a significant X-ray event is detected
- **Countdown reaching zero** — your Countdown timer expires
- **Calendar reminders** — a Calendar event you've set is due

**To enable voice alerts:**
1. Install `flite` on your system:
   - **Linux / Raspberry Pi**: `sudo apt install flite`
   - **macOS**: `brew install flite`
   - **Windows**: voice alerts are not available on Windows at this time.
2. Restart HamClock-Next. If `flite` is found, voice alerts are active automatically.
3. To silence all voice alerts temporarily, use the global **audio mute** setting (Setup → **Audio** tab or REST API `/set_config?audio_mute=1`).

---

## Map Navigation

- **Zoom**: Use the **mouse wheel** to zoom in (up to 10x) and out.
- **Pan**: **Left-click and drag** the map while zoomed.
- **Reset**: **Double-right-click** anywhere on the map to reset zoom/pan.
- **Pins**: Spotting pins (DX Cluster, PSK) are only visible when the corresponding widget is active in a pane.
