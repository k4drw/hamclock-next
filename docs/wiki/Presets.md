# Configuration Presets

Presets let you save a complete dashboard setup and bring it back with one click. Use them to switch between operating modes, such as a contest setup and an evening DX setup.

---

## Opening the Presets Modal

Click the **star icon (★)** in the Time Panel to open the Presets window.

![Presets modal](images/timepanel-presets-modal.png)

---

## What a Preset Saves

Each preset saves a snapshot of the parts of the screen most people adjust often:

| Field | Description |
|-------|-------------|
| Pane 1–6 widget rotations | The full widget rotation list for each pane |
| `rotationIntervalS` | How long each widget is displayed before rotating |
| `propOverlay` | Active propagation overlay (MUF, VOACAP, DRAP, etc.) |
| `weatherOverlay` | Active weather overlay (`none`, `wxmb`, `clouds_grib`) |
| `mapStyle` | Map tile style (nasa, terrain, countries) |
| `mapNightLights` | Night lights on/off |
| `showGrid` | Grid lines on/off |
| `gridType` | Grid type (latlon or maidenhead) |
| `propBand` | Propagation band (e.g., 20m) |
| `propMode` | Propagation mode (e.g., SSB) |
| `propPower` | Transmitter power in watts |

Presets do **not** save identity fields such as callsign, grid, latitude/longitude, or service credentials.

---

## Saving a Preset

1. Configure the dashboard as desired (choose widgets, overlays, map style)
2. Click **★** to open Presets modal
3. Click **Save Current**
4. Enter a name for the preset (e.g., `Contest`, `Evening DX`, `Morning Check`)
5. Click **OK**

The preset appears in the list and is ready to use right away.

---

## Applying a Preset

1. Click **★** to open Presets modal
2. Scroll to find the desired preset
3. Click the preset row to select it
4. Click **Apply**

HamClock-Next switches all panes, overlays, and map settings to match the preset.

---

## Deleting a Preset

1. Click **★** to open Presets modal
2. Select the preset to delete
3. Click **Delete**

The preset is removed immediately. This action cannot be undone.

---

## Built-in Presets

HamClock-Next ships with several built-in presets that you can apply immediately from the Presets modal:

### Prop Firehose

The **Prop Firehose** preset fills the screen with propagation and space weather tiles. It is useful when you want a dedicated monitoring view.

What it loads:
- **Pane 1**: Solar → Solar Storm → Solar Cycle → Ionosonde
- **Pane 2**: Solar Impact Timeline → SFI 30-Day Trend → NOAA Space Wx → Tropo
- **Pane 3**: Aurora → Aurora Graph → VOACAP DE-DX → DRAP
- **Pane 4**: Solar (compact) → Band Conditions → NCDXF Beacons
- **Pane 5**: DE Info
- **Pane 6**: DX Info

Apply it the same way as any other preset: click **★**, select **Prop Firehose**, then click **Apply**.

---

### DE Station Status

The **DE Station Status** preset focuses on your own station achievements and QSO management. It displays award progress, geographic coverage, and QSO records in a rotating view.

What it loads:
- **Pane 1**: DXCC Progress → WAS Progress → Grid Progress
- **Pane 2**: Zone Heatmap → ClubLog Most Wanted
- **Pane 3**: WAC Radar → Alerts
- **Pane 4**: Band Conditions
- **Pane 5**: DE Info
- **Pane 6**: LoTW Sync → ADIF Tracking

Apply it the same way as any other preset: click **★**, select **DE Station Status**, then click **Apply**.

---

### DX

The **DX** preset optimizes the display for DX hunting and chasing. It combines DX Cluster integration, live spotting networks, and band conditions for real-time propagation awareness.

What it loads:
- **Pane 1**: DX Cluster
- **Pane 2**: DX Peditions
- **Pane 3**: Live Spots
- **Pane 4**: Band Conditions
- **Pane 5**: DE Info
- **Pane 6**: DX Info

---

### Contest

The **Contest** preset provides essential tools for contesting: live spot tracking, band conditions, and quick propagation monitoring.

What it loads:
- **Pane 1**: DX Cluster
- **Pane 2**: Live Spots
- **Pane 3**: Band Conditions
- **Pane 4**: Solar (compact)
- **Pane 5**: DE Info
- **Pane 6**: DX Info

---

### Satellite

The **Satellite** preset focuses on satellite operating and EME (Earth-Moon-Earth) communications. It includes lunar phase, tracking tools, and rotator control.

What it loads:
- **Pane 1**: Moon
- **Pane 2**: EME Tool
- **Pane 3**: Gimbal (Rotator Control)
- **Pane 4**: Solar (compact)
- **Pane 5**: Satellite (with polar plot and pass arcs)
- **Pane 6**: DE Info

---

### Environment/WX

The **Environment/WX** preset displays weather and environmental data for both your station and the DX location. Useful for monitoring tropospheric propagation and local conditions.

What it loads:
- **Pane 1**: DE Weather
- **Pane 2**: DX Weather
- **Pane 3**: Forecast
- **Pane 4**: DE Weather (alternate view)
- **Pane 5**: Environmental Temp
- **Pane 6**: Environmental Humidity

---

## Presets in Configuration

Presets are stored in the JSON configuration file under the `presets` array:

```json
{
  "presets": [
    {
      "name": "Contest",
      "pane1Rotation": ["SOLAR", "BAND_CONDITIONS"],
      "pane2Rotation": ["DX_CLUSTER"],
      "pane3Rotation": ["LIVE_SPOTS"],
      "pane4Rotation": ["CONTESTS"],
      "pane5Rotation": ["DE_INFO"],
      "pane6Rotation": ["DX_INFO"],
      "rotationIntervalS": 20,
      "propOverlay": "MUF_RT",
      "weatherOverlay": "None",
      "mapStyle": "nasa",
      "mapNightLights": true,
      "showGrid": false,
      "gridType": "latlon",
      "propBand": "20m",
      "propMode": "SSB",
      "propPower": 100
    }
  ]
}
```
