#!/bin/bash
# HamClock-Next Wiki Screenshot Automation
#
# Queries /get_capabilities from the running server to dynamically discover
# all widget types, projections, themes, and overlays — no hardcoded lists.
#
# Usage:
#   ./capture_wiki_screenshots.sh               # defaults: localhost:8080
#   HC_IP=192.168.1.10 ./capture_wiki_screenshots.sh
#   DELAY=5 OUTPUT_DIR=/tmp/hc_shots ./capture_wiki_screenshots.sh
#
# Output:
#   docs/wiki/images/widgets/<widget_name>.png   — one per widget, dark theme
#   docs/wiki/images/map_looks/<label>.png       — curated map combinations
#
# Requirements: curl, jq

WINDOW_W="${WINDOW_W:-1280}"
WINDOW_H="${WINDOW_H:-800}"

if command -v wmctrl >/dev/null 2>&1; then
  if wmctrl -i -r $(wmctrl -l | grep HamClock-Next | awk '{print $1}') -e 0,-1,-1,"${WINDOW_W}","${WINDOW_H}" 2>/dev/null; then
    echo "Window resized to ${WINDOW_W}×${WINDOW_H}"
    sleep 1  # let SDL repaint at new size
  else
    echo "WARN: wmctrl could not find 'HamClock-Next' window — screenshots use current size"
  fi
else
  echo "WARN: wmctrl not installed — screenshots use current window size (apt install wmctrl)"
fi

set -euo pipefail

HC_IP="${HC_IP:-127.0.0.1}"
HC_PORT="${HC_PORT:-8080}"
BASE_URL="http://${HC_IP}:${HC_PORT}"
DELAY="${DELAY:-3}"
OUTPUT_DIR="${OUTPUT_DIR:-docs/wiki/images}"

# ---------------------------------------------------------------------------
die()    { echo "ERROR: $*" >&2; exit 1; }
hc_get() { curl -sf "${BASE_URL}/$1" > /dev/null || die "API call failed: /$1"; }
capture() {
  local path="$1"
  mkdir -p "$(dirname "$path")"
  curl -sf "${BASE_URL}/get_capture" -o "$path" || die "Capture failed → $path"
  echo "  ✓ $path"
}
has_value() {
  # has_value <jq_key> <value> — returns 0 if value is in capabilities array
  echo "$CAPS" | jq -e --arg v "$2" ".${1}[] | select(. == \$v)" > /dev/null 2>&1
}
# ---------------------------------------------------------------------------

command -v jq      >/dev/null 2>&1 || die "jq required (apt install jq / brew install jq)"
command -v curl    >/dev/null 2>&1 || die "curl required"
command -v magick  >/dev/null 2>&1 || die "ImageMagick v7 required (apt install imagemagick)"

echo "=== HamClock-Next Wiki Screenshot Tool ==="
echo "Server: ${BASE_URL}"
echo ""

curl -sfL "${BASE_URL}/get_config.json" > /dev/null || \
  die "Server not reachable at ${BASE_URL} — start hamclock-next first"

CAPS=$(curl -sf "${BASE_URL}/get_capabilities") || \
  die "/get_capabilities unavailable — rebuild server first (WebServer_Routes.cpp)"

echo "Capabilities discovered:"
echo "  $(echo "$CAPS" | jq '.widgets      | length') widgets"
echo "  $(echo "$CAPS" | jq '.projections  | length') projections"
echo "  $(echo "$CAPS" | jq '.themes       | length') themes"
echo "  $(echo "$CAPS" | jq '.prop_overlays| length') prop overlays"
echo "  $(echo "$CAPS" | jq '.wx_overlays  | length') wx overlays"
echo ""

# Save current visual state for restore on exit
SAVED_THEME=$(curl -sfL "${BASE_URL}/get_config.json" | jq -r '.theme      // "dark"')         || SAVED_THEME="dark"
SAVED_PROJ=$(curl -sf  "${BASE_URL}/get_config.json" | jq -r '.projection // "equirectangular"') || SAVED_PROJ="equirectangular"

restore_state() {
  echo ""
  echo "--- Restoring state ---"
  curl -sf "${BASE_URL}/set_theme?theme=${SAVED_THEME}"    > /dev/null || true
  curl -sf "${BASE_URL}/set_projection?type=${SAVED_PROJ}" > /dev/null || true
  curl -sf "${BASE_URL}/set_prop_overlay?type=none"        > /dev/null || true
  curl -sf "${BASE_URL}/set_wx_overlay?type=none"          > /dev/null || true
  curl -sf "${BASE_URL}/api/panes/pause_all?paused=0"      > /dev/null || true
  echo "  Done."
}
trap restore_state EXIT

hc_get "api/panes/pause_all?paused=1"
sleep 1  # ensure pause takes effect before first solo

# ============================================================================
# Part 1 — Widget Gallery
# Solo each widget on pane 1 with a neutral dark/equirectangular backdrop.
# ============================================================================
echo "--- Part 1: Widget Gallery ---"
hc_get "set_theme?theme=dark"
hc_get "set_projection?type=equirectangular"
hc_get "set_prop_overlay?type=none"
hc_get "set_wx_overlay?type=none"
sleep "${DELAY}"

# Query pane 1 geometry to compute HiDPI scale
PANE1_RECT=$(curl -sf "${BASE_URL}/get_pane_rect?pane=1") || die "Could not fetch pane 1 rect"
REND_W=$(echo "$PANE1_RECT" | jq -r '.renderer_w')
REND_H=$(echo "$PANE1_RECT" | jq -r '.renderer_h')

# Scale: widget coords are uniformly scaled from 800x480 logical space
SCALE=$(awk "BEGIN { print ($REND_W/800 < $REND_H/480) ? $REND_W/800 : $REND_H/480 }")

get_phys_rect() {
  local PANE="$1"
  local RECT=$(curl -sf "${BASE_URL}/get_pane_rect?pane=${PANE}") || die "Could not fetch pane ${PANE} rect"
  local X=$(echo "$RECT" | jq -r '.x')
  local Y=$(echo "$RECT" | jq -r '.y')
  local W=$(echo "$RECT" | jq -r '.w')
  local H=$(echo "$RECT" | jq -r '.h')
  local PX=$(awk "BEGIN { printf \"%d\", $X * $SCALE }")
  local PY=$(awk "BEGIN { printf \"%d\", $Y * $SCALE }")
  local PW=$(awk "BEGIN { printf \"%d\", $W * $SCALE }")
  local PH=$(awk "BEGIN { printf \"%d\", $H * $SCALE }")
  echo "${PW}x${PH}+${PX}+${PY}"
}

PHYS_RECT_1=$(get_phys_rect 1)
PHYS_RECT_2=$(get_phys_rect 2)
PHYS_RECT_3=$(get_phys_rect 3)

echo "  Scale: ${SCALE}x"

TOPBAR_WIDGETS=()
while IFS= read -r WIDGET; do
  case "$WIDGET" in
    de_info|dx_info|dx_cluster|live_spots|on_the_air)
      # Handled specifically later
      ;;
    *)
      TOPBAR_WIDGETS+=("$WIDGET")
      ;;
  esac
done < <(echo "$CAPS" | jq -r '.widgets[]')

WIDGET_COUNT=0

# Process TopBar widgets in batches of 3
for ((i=0; i<${#TOPBAR_WIDGETS[@]}; i+=3)); do
  W1="${TOPBAR_WIDGETS[i]}"
  W2="${TOPBAR_WIDGETS[i+1]:-}"
  W3="${TOPBAR_WIDGETS[i+2]:-}"

  echo "  TopBar Batch: $W1${W2:+, $W2}${W3:+, $W3}"

  hc_get "set_pane?pane=1&action=solo&widget=${W1}"
  if [ -n "$W2" ]; then hc_get "set_pane?pane=2&action=solo&widget=${W2}"; fi
  if [ -n "$W3" ]; then hc_get "set_pane?pane=3&action=solo&widget=${W3}"; fi

  sleep "${DELAY}"
  FULL="${OUTPUT_DIR}/widgets/.full_topbar_${i}.png"
  capture "${FULL}"

  magick "${FULL}" -crop "${PHYS_RECT_1}" +repage "${OUTPUT_DIR}/widgets/${W1}.png"
  WIDGET_COUNT=$((WIDGET_COUNT + 1))

  if [ -n "$W2" ]; then
    magick "${FULL}" -crop "${PHYS_RECT_2}" +repage "${OUTPUT_DIR}/widgets/${W2}.png"
    WIDGET_COUNT=$((WIDGET_COUNT + 1))
  fi

  if [ -n "$W3" ]; then
    magick "${FULL}" -crop "${PHYS_RECT_3}" +repage "${OUTPUT_DIR}/widgets/${W3}.png"
    WIDGET_COUNT=$((WIDGET_COUNT + 1))
  fi
  
  rm -f "${FULL}"
done

# Standard SidePanel Widgets
echo "  Standard SidePanel (de_info, dx_info)"
hc_get "set_config?side_panel_mode=default"
sleep 1 # Wait for UI to rebuild panes!
hc_get "set_satellite.txt?name=none"

PHYS_RECT_5=$(get_phys_rect 5)
PHYS_RECT_6=$(get_phys_rect 6)

sleep "${DELAY}"
FULL="${OUTPUT_DIR}/widgets/.full_sidepanel.png"
capture "${FULL}"
magick "${FULL}" -crop "${PHYS_RECT_5}" +repage "${OUTPUT_DIR}/widgets/de_info.png"
WIDGET_COUNT=$((WIDGET_COUNT + 1))
magick "${FULL}" -crop "${PHYS_RECT_6}" +repage "${OUTPUT_DIR}/widgets/dx_info.png"
WIDGET_COUNT=$((WIDGET_COUNT + 1))
rm -f "${FULL}"

# Satellite
echo "  satellite (in DXSatPane)"
hc_get "set_satellite.txt?name=ISS"
sleep "${DELAY}"
FULL="${OUTPUT_DIR}/widgets/.full_satellite.png"
capture "${FULL}"
magick "${FULL}" -crop "${PHYS_RECT_6}" +repage "${OUTPUT_DIR}/widgets/satellite.png"
WIDGET_COUNT=$((WIDGET_COUNT + 1))
rm -f "${FULL}"
hc_get "set_satellite.txt?name=none"

# Double Tall SidePanel Widgets
for W in dx_cluster live_spots on_the_air; do
  echo "  ${W} (double-tall SidePanel)"
  hc_get "set_config?side_panel_mode=${W}"
  sleep 1
  PHYS_RECT_5_DOUBLE=$(get_phys_rect 5)
  sleep "${DELAY}"
  
  FULL="${OUTPUT_DIR}/widgets/.full_${W}.png"
  capture "${FULL}"
  magick "${FULL}" -crop "${PHYS_RECT_5_DOUBLE}" +repage "${OUTPUT_DIR}/widgets/${W}.png"
  WIDGET_COUNT=$((WIDGET_COUNT + 1))
  rm -f "${FULL}"
done
hc_get "set_config?side_panel_mode=default"

# ============================================================================
# Part 2 — Map Looks
# Curated visual combinations. Values are validated against /get_capabilities
# at runtime — unsupported values in this build are skipped with a warning.
#
# Format: "projection|prop_overlay|wx_overlay|theme|output_label"
# ============================================================================
echo ""
echo "--- Part 2: Map Looks ---"

LOOKS=(
  "robinson|muf|none|dark|robinson_muf_dark"
  "azimuthal|aurora|none|midnight|azimuthal_aurora_midnight"
  "mercator|drap|wxmb|paper|mercator_drap_wxmb_paper"
  "dual_azimuthal|none|clouds_grib|glass|dual_az_clouds_glass"
  "robinson|none|clouds_grib|dark|robinson_clouds_dark"
  "azimuthal|none|none|amber|azimuthal_clean_amber"
  "robinson|none|none|matrix|robinson_matrix"
)

LOOK_COUNT=0
LOOK_SKIPPED=0
for LOOK in "${LOOKS[@]}"; do

  IFS='|' read -r PROJ PROP WX THEME LABEL <<< "${LOOK}"

  SKIP=0
  has_value projections  "$PROJ"  || { echo "  SKIP ${LABEL}: projection '${PROJ}' not available";   SKIP=1; }
  has_value prop_overlays "$PROP" || { echo "  SKIP ${LABEL}: prop_overlay '${PROP}' not available"; SKIP=1; }
  has_value wx_overlays  "$WX"    || { echo "  SKIP ${LABEL}: wx_overlay '${WX}' not available";     SKIP=1; }
  has_value themes       "$THEME" || { echo "  SKIP ${LABEL}: theme '${THEME}' not available";       SKIP=1; }
  if [ "$SKIP" -eq 1 ]; then
    LOOK_SKIPPED=$((LOOK_SKIPPED + 1))
    continue
  fi

  echo "  ${LABEL}"
  hc_get "set_projection?type=${PROJ}"
  hc_get "set_prop_overlay?type=${PROP}"
  hc_get "set_wx_overlay?type=${WX}"
  hc_get "set_theme?theme=${THEME}"
  sleep "${DELAY}"
  capture "${OUTPUT_DIR}/map_looks/${LABEL}.png"
  LOOK_COUNT=$((LOOK_COUNT + 1))
done

# ============================================================================
# Part 3 — Theme Gallery
# One clean robinson/no-overlays shot per theme, fully dynamic.
# ============================================================================
echo ""
echo "--- Part 3: Theme Gallery ---"
hc_get "set_projection?type=robinson"
hc_get "set_prop_overlay?type=none"
hc_get "set_wx_overlay?type=none"
# Restore a multi-widget pane so the shot shows a representative layout
hc_get "set_pane?pane=1&action=solo&widget=solar"

THEME_COUNT=0
while IFS= read -r THEME; do
  echo "  ${THEME}"
  hc_get "set_theme?theme=${THEME}"
  sleep "${DELAY}"
  capture "${OUTPUT_DIR}/themes/${THEME}.png"
  THEME_COUNT=$((THEME_COUNT + 1))
done < <(echo "$CAPS" | jq -r '.themes[]')

# ============================================================================
echo ""
echo "=== Complete ==="
echo "  Widget gallery: ${OUTPUT_DIR}/widgets/  (${WIDGET_COUNT} images)"
echo "  Map looks:      ${OUTPUT_DIR}/map_looks/ (${LOOK_COUNT} images, ${LOOK_SKIPPED} skipped)"
echo "  Theme gallery:  ${OUTPUT_DIR}/themes/    (${THEME_COUNT} images)"
echo ""
echo "Next: invoke /hc-wiki to regenerate docs/wiki/ from these screenshots"
