# Screen Layout

HamClock-Next divides the screen into a fixed set of regions. Understanding the layout is the first step to customizing your dashboard.

![Full dashboard with labeled regions](images/v1_0/hc_v1_dark_robinson_muf_rot1_asteroids_forecast_wx_alerts_ncdxf_de_info_satellite.png)
<!-- TODO: annotated screenshot needed -->

---

## Regions at a Glance

HamClock-Next uses a **Top Bar** and a **Left Side Panel** to maximize map area.

```
+----------+----------+----------+----------+-------+
| Time     | Pane 1   | Pane 2   | Pane 3   | Pane4 |
| Panel    | (rot)    | (rot)    | (rot)    | (stat)|
+----------+----------+----------+----------+-------+
|          |                                        |
| Pane 5   |                                        |
| (rot)    |                                        |
|          |             MAP AREA                   |
+----------+             (center)                   |
|          |                                        |
| Pane 6   |                                        |
| (rot)    |                                        |
|          |        RSS Ticker (optional)           |
+---------------------------------------------------+
```

> **Note on Pane 4**: This is a narrow status pane (62px) typically used for the NCDXF Beacon widget or system status.

> Layout proportions vary by screen resolution and panel mode. The exact arrangement is determined at startup based on the window dimensions.

---

## Time Panel

The Time Panel occupies the top-left corner and is **not** a rotatable pane — it always displays the same content.

![Time panel detail](images/widgets/time_panel.jpg)


Contents:

| Element              | Description                                  |
| -------------------- | -------------------------------------------- |
| **UTC clock**        | Current UTC time, large digits               |
| **Local clock**      | Local time based on your configured timezone |
| **DE callsign**      | Your callsign as entered in Setup            |
| **DE grid**          | Your Maidenhead grid locator                 |
| **Sunrise / Sunset** | Today's SR/SS times for your location        |
| **Gear icon (⚙)**    | Opens the Setup modal                        |
| **Star icon (★)**    | Opens the [Presets modal](Presets.md)        |

---

## The Six Panes

Panes 1–6 are rotatable widget containers. Each pane independently cycles through a configured list of widgets.

- **Click the top strip** of any pane to open the widget picker
- **Rotation** advances automatically on a configurable timer (default 30 seconds)
- Panes can be configured to rotate in sync with each other

See [Pane Customization](Pane-Customization.md) for details.

---

## Map

The large center area displays the world map. It supports:

- **Mercator** and **Azimuthal** projections
- Propagation overlays (MUF, VOACAP, DRAP, and more)
- Weather overlays (cloud cover, surface pressure)
- Night shadow (gray line)
- Grid lines (latitude/longitude or Maidenhead)
- Beacon markers (NCDXF/IBP)
- Satellite ground tracks

See [Map & Overlays](Map-and-Overlays.md) for details.

---

In **side panel mode**, the left column (Panes 5 and 6) is replaced by a tall panel showing either:

- **DX Info** — information about your current DX entity target
- **Satellite** — ground track and pass information for a tracked satellite

The panel mode is controlled by the `panelMode` configuration field.

---

## Interactive Regions and the K Key

Pressing **K** toggles highlight mode. All interactive screen regions light up with cyan bounding boxes and a tooltip label.

![K key highlight mode active](images/key-highlight-mode.png)
<!-- TODO: screenshot needed -->

This is the fastest way to discover clickable controls, especially on unfamiliar widgets.
