# Pane Customization & Rotation

HamClock-Next has six rotatable panes. Each pane independently cycles through a configured list of widgets on a timer.

---

## Opening the Widget Picker

Click the **top strip** (title bar area) of any pane to open the widget picker for that pane.

![Widget selector open](images/modal-widget-selector.png)

The picker lists all available widgets. Click one to immediately display it in the pane. The selected widget is added to that pane's rotation list.

---

## Rotation Lists

Each pane has a rotation list — an ordered sequence of widgets it cycles through. When the rotation timer fires, the pane advances to the next widget in its list.

- A pane with only one widget in its list never rotates.
- A pane with multiple widgets cycles through them in order.

### Rotation Interval

The `rotationIntervalS` setting (default: 30 seconds) controls how long each widget is shown before advancing. This interval applies to all panes.

Configure the interval in [Setup & Configuration](Configuration.md).

### Sync Rotation

When `syncRotation` is enabled, all panes advance simultaneously rather than independently. This creates a synchronized "page turn" effect across the whole dashboard.

---

## Rotation Indicator

Panes with more than one widget in their rotation list show a rotation indicator — a small visual cue (dots or arrows) that indicates multiple widgets are available and rotation is active.

![Pane rotation indicator](images/pane-rotation-indicator.png)

Click the indicator arrows (if present) to manually advance or retreat through the pane's widget list without waiting for the timer.

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

---

## Side Panel Mode

In **side panel mode**, the **left column** (which normally contains Panes 5 and 6) is replaced by a single tall panel. This is useful for widgets that benefit from vertical space, such as a long DX Cluster list or a detailed Satellite track.

| Mode  | Description                                                                             |
| ----- | --------------------------------------------------------------------------------------- |
| `dx`  | Shows a tall DX Info panel with entity details, bearing, distance, and propagation info |
| `sat` | Shows satellite ground track and pass timing for the selected satellite                 |

Set `panelMode` in Configuration to switch modes. In side panel mode, Panes 5 and 6 are merged/replaced by the tall panel.

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
