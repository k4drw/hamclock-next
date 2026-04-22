# Pane Customization & Rotation

HamClock-Next has six panes. Each pane can show one widget or cycle through a small list of them on a timer.

---

## Opening the Widget Picker

Click the **top strip** of any pane to open the widget picker for that pane.

![Widget selector open](images/modal-widget-selector.png)

The picker lists all available widgets. Click one to show it in the pane. If you later add more widgets, the pane will rotate through them in order.

---

## Rotation Lists

Each pane has a rotation list, which is just the order of widgets it will cycle through.

- A pane with only one widget never rotates.
- A pane with multiple widgets cycles through them in order.

### Rotation Interval

The `rotationIntervalS` setting controls how long each widget is shown before moving to the next one. The default is 30 seconds.

Configure the interval in [Setup & Configuration](Configuration.md).

### Sync Rotation

When `syncRotation` is enabled, all panes advance together instead of independently.

---

## Rotation Indicator

Panes with more than one widget in their list show a rotation indicator so you can tell there is more than one widget attached.

![Pane rotation indicator](images/pane-rotation-indicator.png)

Click the indicator arrows, if present, to move forward or backward through the pane's widgets without waiting for the timer.

---

## The Six Panes

| Pane   | Default Widget  | Position                 |
| ------ | --------------- | ------------------------ |
| Pane 1 | Solar           | Top bar, 1st slot        |
| Pane 2 | DX Cluster      | Top bar, 2nd slot        |
| Pane 3 | Live Spots      | Top bar, 3rd slot        |
| Pane 4 | Band Conditions | Top bar, 4th slot (62px) |
| Pane 5 | DE Info         | Left column, top         |
| Pane 6 | DX Info         | Left column, bottom      |

### Pane 4 — Limited Widget List

Pane 4 is the compact slot at the far right of the top bar. Because it is short, only a small set of widgets that fit in tight vertical space can go there:

- NCDXF Beacons
- Solar
- DE/DX Weather
- Band Conditions

If a widget you expect to see is missing from the Pane 4 picker, this is why — it simply needs more vertical space than that slot provides. Add it to one of the other five panes instead.

---

## Making a Widget Fill the Map

Any tile can be enlarged to cover the map area temporarily — useful when you want a bigger view of a chart, the DX Cluster list, or the Big Clock.

- **Click the maximize arrow** in the top-right corner of a pane, or
- **Double-click the pane's title bar.**

The pane expands to fill the map. Click the same control again (or press **Escape**) to shrink it back to normal size. Rotation pauses while a pane is maximized so the tile you are viewing does not cycle out from under you.

Not every widget benefits from being maximized. Ones that do — Aurora Graph, Band Conditions, Big Clock, DX Cluster, Live Spots, and Solar — have a dedicated maximized layout with bigger text and extra detail.

---

## Side Panel Mode

In **side panel mode**, the left column is replaced by a single tall panel. This is useful for widgets that benefit from vertical space, such as a long DX Cluster list or a detailed Satellite track.

| Mode  | Description                                                                             |
| ----- | --------------------------------------------------------------------------------------- |
| `dx`  | Shows a tall DX Info panel with entity details, bearing, distance, and propagation info |
| `sat` | Shows satellite ground track and pass timing for the selected satellite                 |

Set `panelMode` in Configuration to switch modes. In side panel mode, Panes 5 and 6 are replaced by the tall panel.

---

## Configuring Pane Rotations Directly

Pane rotation lists are stored in the configuration file as arrays of widget type names. You can edit them directly or use Presets to save and restore complete configurations.

See [Configuration Presets](Presets.md) for saving named configurations.

The relevant configuration fields are:

```json
{
  "pane1Rotation": ["solar", "sdo"],
  "pane2Rotation": ["dx_cluster"],
  "pane3Rotation": ["live_spots", "band_conditions"],
  "pane4Rotation": ["aurora", "history_kp"],
  "pane5Rotation": ["de_info"],
  "pane6Rotation": ["dx_info"],
  "rotationIntervalS": 30,
  "syncRotation": false
}
```
