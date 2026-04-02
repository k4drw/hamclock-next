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
#   FORCE=1 ./capture_wiki_screenshots.sh       # overwrite existing images
#   PARTS=widgets,maximized ./capture_wiki_screenshots.sh  # run specific parts
#   PARTS=wiki ./capture_wiki_screenshots.sh    # only regenerate wiki MD files
#
# Output:
#   docs/wiki/images/widgets/<widget_name>.png      — one per widget, dark theme
#   docs/wiki/images/widgets/<widget>_maximized.png — widgets expanded over map
#   docs/wiki/images/widgets/time_panel.png         — TimePanel fixed element
#   docs/wiki/images/widgets/rss_banner.png         — RSS scrolling banner
#   docs/wiki/images/map_looks/<label>.png          — curated map combinations
#   docs/wiki/images/themes/<theme>.png             — theme gallery shots
#   docs/wiki/images/modal-setup.png                — setup modal (ui_docs)
#   docs/wiki/images/timepanel-presets-modal.png    — presets modal (ui_docs)
#   docs/wiki/images/modal-widget-selector.png      — widget selector modal (ui_docs)
#   docs/wiki/images/pane-rotation-indicator.png    — pane rotation indicator (ui_docs)
#   docs/wiki/images/key-highlight-mode.png         — keyboard highlight mode (ui_docs)
#   docs/wiki/images/layout-annotated.png           — annotated dashboard layout (ui_docs)
#
# Parts (for PARTS env var):
#   fixed_ui   — TimePanel + RSS banner (Part 0)
#   widgets    — widget gallery (Part 1+2)
#   maximized  — maximized pane screenshots (Part 3)
#   map_looks  — map look gallery (Part 4)
#   themes     — theme gallery (Part 5)
#   ui_docs    — UI modals and annotated layout (Part 6)
#   wiki       — regenerate wiki MD files (Part 7)
#   all        — all of the above (default)
#
# Requirements: curl, jq, ImageMagick v7 (magick)

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

HC_IP="${HC_IP:-127.0.0.1}"
HC_PORT="${HC_PORT:-8080}"
BASE_URL="http://${HC_IP}:${HC_PORT}"
DELAY="${DELAY:-3}"
OUTPUT_DIR="${OUTPUT_DIR:-docs/wiki/images}"
FORCE="${FORCE:-0}"
PARTS="${PARTS:-all}"
HC_AUTOSTART="${HC_AUTOSTART:-0}"

# ---------------------------------------------------------------------------
FAILED=()
die()    { echo "ERROR: $*" >&2; exit 1; }
hc_get() {
  if ! curl -sf "${BASE_URL}/$1" > /dev/null; then
    echo "  WARN: API call failed: /$1" >&2
    FAILED+=("API:/$1")
  fi
}
capture() {
  local path="$1"
  mkdir -p "$(dirname "$path")"
  if ! curl -sf "${BASE_URL}/get_capture" -o "$path"; then
    echo "  WARN: Capture failed → $path" >&2
    FAILED+=("capture:$path")
    return 1
  fi
  echo "  ✓ $path"
}
skip_or_capture() {
  local path="$1"
  if [ "$FORCE" = "0" ] && [ -f "$path" ]; then
    echo "  SKIP (exists) $path"
    return 0
  fi
  capture "$path"
}
has_value() {
  # has_value <jq_key> <value> — returns 0 if value is in capabilities array
  echo "$CAPS" | jq -e --arg v "$2" ".${1}[] | select(. == \$v)" > /dev/null 2>&1
}
is_scrollable() {
  # is_scrollable <widget_id> — returns 0 if widget is scrollable
  echo "$CAPS" | jq -e --arg v "$1" ".widget_meta[] | select(.id == \$v and .scrollable == true)" > /dev/null 2>&1
}
part_enabled() {
  [ "$PARTS" = "all" ] || echo "$PARTS" | tr ',' '\n' | grep -qx "$1"
}
debug_click() {
  local x="$1"
  local y="$2"
  hc_get "debug/click?x=${x}&y=${y}"
}
debug_key() {
  local k="$1"
  hc_get "debug/keypress?key=${k}"
}
# ---------------------------------------------------------------------------

command -v jq      >/dev/null 2>&1 || die "jq required (apt install jq / brew install jq)"
command -v curl    >/dev/null 2>&1 || die "curl required"
command -v magick  >/dev/null 2>&1 || die "ImageMagick v7 required (apt install imagemagick)"

echo "=== HamClock-Next Wiki Screenshot Tool ==="
echo "Server: ${BASE_URL}"
echo "FORCE=${FORCE}  PARTS=${PARTS}"
echo ""

if [ "$HC_AUTOSTART" = "1" ]; then
  if ! curl -sfL "${BASE_URL}/get_config.json" > /dev/null 2>&1; then
    echo "Starting HamClock-Next (HC_AUTOSTART=1)..."
    ./build/hamclock-next --live-web > /dev/null 2>&1 &
    HC_PID=$!
    # Update trap to include killer cleanup
    trap "echo 'Cleaning up server (PID ${HC_PID})...'; kill ${HC_PID}; restore_state" EXIT
    sleep 2
  fi
fi

curl -sfL "${BASE_URL}/get_config.json" > /dev/null || \
  die "Server not reachable at ${BASE_URL} — start hamclock-next first"

# Wait for the dashboard to finish initializing (using readiness probe).
echo -n "Waiting for dashboard ready..."
for i in $(seq 1 60); do
  READY=$(curl -sf "${BASE_URL}/api/sys/ready" || echo "offline")
  if [ "$READY" = "ready" ]; then
    echo " ok (${i}s)"
    sleep 2 # wait for layout and sub-widgets to sync
    break
  fi
  if [ "$i" = "60" ]; then
    die "Dashboard not ready after 60 s ($READY) — check logs"
  fi
  echo -n "."
  sleep 1
done

# CAPS discovery
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
SAVED_RSS=$(curl -sf   "${BASE_URL}/get_config.json" | jq -r '.rssEnabled // true')             || SAVED_RSS="true"
SAVED_RSS_PARAM=$( [ "$SAVED_RSS" = "true" ] && echo "1" || echo "0" )

restore_state() {
  echo ""
  echo "--- Restoring state ---"
  curl -sf "${BASE_URL}/set_theme?theme=${SAVED_THEME}"         > /dev/null || true
  curl -sf "${BASE_URL}/set_projection?type=${SAVED_PROJ}"      > /dev/null || true
  curl -sf "${BASE_URL}/set_prop_overlay?type=none"             > /dev/null || true
  curl -sf "${BASE_URL}/set_wx_overlay?type=none"               > /dev/null || true
  curl -sf "${BASE_URL}/api/panes/pause_all?paused=0"           > /dev/null || true
  curl -sf "${BASE_URL}/api/panes/collapse"                     > /dev/null || true
  curl -sf "${BASE_URL}/set_rss?enabled=${SAVED_RSS_PARAM}"     > /dev/null || true
  echo "  Done."
}
trap restore_state EXIT

if ! part_enabled wiki; then
  # Pause only when actually capturing images (not in wiki-only mode)
  hc_get "api/panes/pause_all?paused=1"
  sleep 1  # ensure pause takes effect before first solo
fi

# SCALE is no longer used by get_phys_rect (API provides px, py, pw, ph)
# but we keep it here for any manual math if needed.
compute_scale() {
  PANE1_RECT=$(curl -sf "${BASE_URL}/get_pane_rect?pane=1") || { echo "  WARN: Could not fetch pane 1 rect — scale defaults to 1.0" >&2; SCALE=1; return; }
  REND_W=$(echo "$PANE1_RECT" | jq -r '.renderer_w')
  REND_H=$(echo "$PANE1_RECT" | jq -r '.renderer_h')
  LOG_W=$(echo  "$PANE1_RECT" | jq -r '.logical_w')
  LOG_H=$(echo  "$PANE1_RECT" | jq -r '.logical_h')
  SCALE=$(awk "BEGIN { print ($REND_W/$LOG_W < $REND_H/$LOG_H) ? $REND_W/$LOG_W : $REND_H/$LOG_H }")
  echo "  Scale: ${SCALE}x"
}

get_phys_rect() {
  local PANE="$1"
  local RECT
  local XENDPOINT="/get_pane_rect?pane=${PANE}"
  [ "$PANE" = "tp" ] && XENDPOINT="/get_timepanel_rect"
  
  RECT=$(curl -sf "${BASE_URL}${XENDPOINT}") || { echo "  WARN: Could not fetch ${XENDPOINT}" >&2; echo "0x0+0+0"; return; }
  local PX; PX=$(echo "$RECT" | jq -r '.px')
  local PY; PY=$(echo "$RECT" | jq -r '.py')
  local PW; PW=$(echo "$RECT" | jq -r '.pw')
  local PH; PH=$(echo "$RECT" | jq -r '.ph')
  echo "${PW}x${PH}+${PX}+${PY}"
}

# ============================================================================
# Part 0 — Fixed UI Elements (TimePanel + RSS Banner)
# ============================================================================
if part_enabled fixed_ui; then
  echo "--- Part 0: Fixed UI Elements ---"
  hc_get "set_theme?theme=dark"
  hc_get "set_projection?type=equirectangular"
  hc_get "set_prop_overlay?type=none"
  hc_get "set_wx_overlay?type=none"
  compute_scale

  # TimePanel
  PHYS_RECT_TP=$(get_phys_rect tp)
  if [ -n "$PHYS_RECT_TP" ] && [ "$PHYS_RECT_TP" != "0x0+0+0" ]; then
    if [ "$FORCE" = "1" ] || [ ! -f "${OUTPUT_DIR}/widgets/time_panel.png" ]; then
      echo "  time_panel"
      sleep "${DELAY}"
      FULL="${OUTPUT_DIR}/widgets/.full_timepanel.png"
      if capture "${FULL}"; then
        magick "${FULL}" -crop "${PHYS_RECT_TP}" +repage "${OUTPUT_DIR}/widgets/time_panel.png" \
          && echo "  ✓ ${OUTPUT_DIR}/widgets/time_panel.png"
        rm -f "${FULL}"
      fi
    else
      echo "  SKIP (exists) ${OUTPUT_DIR}/widgets/time_panel.png"
    fi
  fi

  # RSS Banner (logical coords: x=139, y=412, w=660, h=68 — fixed in DashboardContext)
  RSS_PX=$(awk "BEGIN { printf \"%d\", 139 * $SCALE }")
  RSS_PY=$(awk "BEGIN { printf \"%d\", 412 * $SCALE }")
  RSS_PW=$(awk "BEGIN { printf \"%d\", 660 * $SCALE }")
  RSS_PH=$(awk "BEGIN { printf \"%d\", 68  * $SCALE }")
  if [ "$FORCE" = "1" ] || [ ! -f "${OUTPUT_DIR}/widgets/rss_banner.png" ]; then
    echo "  rss_banner"
    hc_get "set_rss?enabled=1"
    sleep "${DELAY}"
    FULL="${OUTPUT_DIR}/widgets/.full_rss.png"
    if capture "${FULL}"; then
      magick "${FULL}" -crop "${RSS_PW}x${RSS_PH}+${RSS_PX}+${RSS_PY}" +repage "${OUTPUT_DIR}/widgets/rss_banner.png" \
        && echo "  ✓ ${OUTPUT_DIR}/widgets/rss_banner.png"
      rm -f "${FULL}"
    fi
    hc_get "set_rss?enabled=${SAVED_RSS_PARAM}"
  else
    echo "  SKIP (exists) ${OUTPUT_DIR}/widgets/rss_banner.png"
  fi
fi

# ============================================================================
# Part 1 — Widget Gallery
# Solo each widget on pane 1 with a neutral dark/equirectangular backdrop.
# ============================================================================
if part_enabled widgets; then
  echo ""
  echo "--- Part 1: Widget Gallery ---"
  hc_get "set_theme?theme=dark"
  hc_get "set_projection?type=equirectangular"
  hc_get "set_prop_overlay?type=none"
  hc_get "set_wx_overlay?type=none"
  sleep "${DELAY}"

  compute_scale

  PHYS_RECT_1=$(get_phys_rect 1)
  PHYS_RECT_2=$(get_phys_rect 2)
  PHYS_RECT_3=$(get_phys_rect 3)

  TOPBAR_WIDGETS=()
  SCROLL_WIDGETS=()
  while IFS= read -r WIDGET; do
    case "$WIDGET" in
      de_info|dx_info|satellite)
        # Handled specifically later
        ;;
      *)
        if is_scrollable "$WIDGET"; then
          SCROLL_WIDGETS+=("$WIDGET")
        else
          TOPBAR_WIDGETS+=("$WIDGET")
        fi
        ;;
    esac
  done < <(echo "$CAPS" | jq -r '.widgets[]')

  WIDGET_COUNT=0

  # Process TopBar widgets in batches of 3
  for ((i=0; i<${#TOPBAR_WIDGETS[@]}; i+=3)); do
    W1="${TOPBAR_WIDGETS[i]}"
    W2="${TOPBAR_WIDGETS[i+1]:-}"
    W3="${TOPBAR_WIDGETS[i+2]:-}"

    # Check if all outputs already exist
    ALL_EXIST=1
    for W in "$W1" ${W2:+"$W2"} ${W3:+"$W3"}; do
      [ -f "${OUTPUT_DIR}/widgets/${W}.png" ] || ALL_EXIST=0
    done
    if [ "$FORCE" = "0" ] && [ "$ALL_EXIST" = "1" ]; then
      echo "  SKIP (exists) batch: $W1${W2:+, $W2}${W3:+, $W3}"
      WIDGET_COUNT=$((WIDGET_COUNT + 1))
      [ -n "$W2" ] && WIDGET_COUNT=$((WIDGET_COUNT + 1))
      [ -n "$W3" ] && WIDGET_COUNT=$((WIDGET_COUNT + 1))
      continue
    fi

    echo "  TopBar Batch: $W1${W2:+, $W2}${W3:+, $W3}"

    hc_get "set_pane?pane=1&action=solo&widget=${W1}"
    if [ -n "$W2" ]; then hc_get "set_pane?pane=2&action=solo&widget=${W2}"; fi
    if [ -n "$W3" ]; then hc_get "set_pane?pane=3&action=solo&widget=${W3}"; fi

    sleep "${DELAY}"
    FULL="${OUTPUT_DIR}/widgets/.full_topbar_${i}.png"
    capture "${FULL}" || continue

    if [ "$FORCE" = "1" ] || [ ! -f "${OUTPUT_DIR}/widgets/${W1}.png" ]; then
      magick "${FULL}" -crop "${PHYS_RECT_1}" +repage "${OUTPUT_DIR}/widgets/${W1}.png"
      echo "  ✓ ${OUTPUT_DIR}/widgets/${W1}.png"
    fi
    WIDGET_COUNT=$((WIDGET_COUNT + 1))

    if [ -n "$W2" ]; then
      if [ "$FORCE" = "1" ] || [ ! -f "${OUTPUT_DIR}/widgets/${W2}.png" ]; then
        magick "${FULL}" -crop "${PHYS_RECT_2}" +repage "${OUTPUT_DIR}/widgets/${W2}.png"
        echo "  ✓ ${OUTPUT_DIR}/widgets/${W2}.png"
      fi
      WIDGET_COUNT=$((WIDGET_COUNT + 1))
    fi

    if [ -n "$W3" ]; then
      if [ "$FORCE" = "1" ] || [ ! -f "${OUTPUT_DIR}/widgets/${W3}.png" ]; then
        magick "${FULL}" -crop "${PHYS_RECT_3}" +repage "${OUTPUT_DIR}/widgets/${W3}.png"
        echo "  ✓ ${OUTPUT_DIR}/widgets/${W3}.png"
      fi
      WIDGET_COUNT=$((WIDGET_COUNT + 1))
    fi

    rm -f "${FULL}"
  done

  # Process Scrollable widgets in Double-Height SidePanel
  for W in "${SCROLL_WIDGETS[@]}"; do
    if [ "$FORCE" = "0" ] && [ -f "${OUTPUT_DIR}/widgets/${W}.png" ]; then
      echo "  SKIP (exists) ${W} (double-height)"
      WIDGET_COUNT=$((WIDGET_COUNT + 1))
      continue
    fi
    echo "  ${W} (double-height SidePanel)"
    # Set Pane 5 to widget and CLEAR Pane 6 to trigger full height
    hc_get "set_config?pane4=${W}&pane5="
    sleep 1
    # Refresh rect for Pane 5 (now double height)
    PHYS_RECT_5_DOUBLE=$(get_phys_rect 5)
    sleep "${DELAY}"
    FULL="${OUTPUT_DIR}/widgets/.full_${W}.png"
    if capture "${FULL}"; then
      magick "${FULL}" -crop "${PHYS_RECT_5_DOUBLE}" +repage "${OUTPUT_DIR}/widgets/${W}.png"
      echo "  ✓ ${OUTPUT_DIR}/widgets/${W}.png"
      rm -f "${FULL}"
    fi
    WIDGET_COUNT=$((WIDGET_COUNT + 1))
  done

  # Part 2 — Standard SidePanel Widgets (Split mode)
  echo ""
  echo "--- Part 2: SidePanel Widgets ---"
  echo "  Standard SidePanel (de_info, dx_info)"
  hc_get "set_config?pane4=de_info&pane5=dx_info"
  sleep 1 # Wait for UI to rebuild panes!
  hc_get "set_satname?name=none"

  PHYS_RECT_5=$(get_phys_rect 5)
  PHYS_RECT_6=$(get_phys_rect 6)

  sleep "${DELAY}"
  FULL="${OUTPUT_DIR}/widgets/.full_sidepanel.png"
  capture "${FULL}" || true
  if [ -f "$FULL" ]; then
    if [ "$FORCE" = "1" ] || [ ! -f "${OUTPUT_DIR}/widgets/de_info.png" ]; then
      magick "${FULL}" -crop "${PHYS_RECT_5}" +repage "${OUTPUT_DIR}/widgets/de_info.png"
      echo "  ✓ ${OUTPUT_DIR}/widgets/de_info.png"
    else echo "  SKIP (exists) ${OUTPUT_DIR}/widgets/de_info.png"; fi
    WIDGET_COUNT=$((WIDGET_COUNT + 1))
    if [ "$FORCE" = "1" ] || [ ! -f "${OUTPUT_DIR}/widgets/dx_info.png" ]; then
      magick "${FULL}" -crop "${PHYS_RECT_6}" +repage "${OUTPUT_DIR}/widgets/dx_info.png"
      echo "  ✓ ${OUTPUT_DIR}/widgets/dx_info.png"
    else echo "  SKIP (exists) ${OUTPUT_DIR}/widgets/dx_info.png"; fi
    WIDGET_COUNT=$((WIDGET_COUNT + 1))
    rm -f "${FULL}"
  fi

  # Satellite
  echo "  satellite (in DXSatPane)"
  hc_get "set_config?pane4=de_info&pane5=dx_info" # Ensure split mode
  hc_get "set_satname?name=ISS"
  sleep "${DELAY}"
  FULL="${OUTPUT_DIR}/widgets/.full_satellite.png"
  capture "${FULL}" || true
  if [ -f "$FULL" ]; then
    if [ "$FORCE" = "1" ] || [ ! -f "${OUTPUT_DIR}/widgets/satellite.png" ]; then
      magick "${FULL}" -crop "${PHYS_RECT_6}" +repage "${OUTPUT_DIR}/widgets/satellite.png"
      echo "  ✓ ${OUTPUT_DIR}/widgets/satellite.png"
    else echo "  SKIP (exists) ${OUTPUT_DIR}/widgets/satellite.png"; fi
    WIDGET_COUNT=$((WIDGET_COUNT + 1))
    rm -f "${FULL}"
  fi
  hc_get "set_satname?name=none"

  echo "  Widget gallery: ${WIDGET_COUNT} images captured/checked"
fi

# ============================================================================
# Part 3 — Maximized Widget Screenshots
# Shows widgets expanded over the map area using /api/panes/expand
# ============================================================================
if part_enabled maximized; then
  echo ""
  echo "--- Part 3: Maximized Widgets ---"
  hc_get "set_theme?theme=dark"
  hc_get "set_projection?type=robinson"
  hc_get "set_prop_overlay?type=none"
  hc_get "set_wx_overlay?type=none"
  [ -z "${SCALE:-}" ] && compute_scale

  MAXIMIZED_WIDGETS=(solar aurora live_spots aurora_graph band_conditions dx_cluster)
  MAX_COUNT=0

  for W in "${MAXIMIZED_WIDGETS[@]}"; do
    OUT="${OUTPUT_DIR}/widgets/${W}_maximized.png"
    if [ "$FORCE" = "0" ] && [ -f "$OUT" ]; then
      echo "  SKIP (exists) $OUT"
      MAX_COUNT=$((MAX_COUNT + 1))
      continue
    fi
    echo "  ${W} (maximized)"
    hc_get "set_pane?pane=1&action=solo&widget=${W}"
    sleep 1
    hc_get "api/panes/expand?pane=1"
    sleep "${DELAY}"
    if capture "$OUT"; then
      MAX_COUNT=$((MAX_COUNT + 1))
    fi
    hc_get "api/panes/collapse"
    sleep 1
  done
  echo "  Maximized: ${MAX_COUNT} images captured/checked"
fi

# ============================================================================
# Part 4 — Map Looks
# Curated visual combinations. Values are validated against /get_capabilities
# at runtime — unsupported values in this build are skipped with a warning.
#
# Format: "projection|prop_overlay|wx_overlay|theme|output_label"
# ============================================================================
if part_enabled map_looks; then
  echo ""
  echo "--- Part 4: Map Looks ---"

  LOOKS=(
    "robinson|muf|none|dark|robinson_muf_dark"
    "azimuthal|aurora|none|midnight|azimuthal_aurora_midnight"
    "mercator|drap|wxmb|paper|mercator_drap_wxmb_paper"
    "dual_azimuthal|none|clouds_grib|glass|dual_az_clouds_glass"
    "robinson|none|clouds_grib|dark|robinson_clouds_dark"
    "azimuthal|none|none|amber|azimuthal_clean_amber"
    "robinson|none|none|matrix|robinson_matrix"
    "robinson|voacap|none|dark|robinson_voacap_dark"
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

    OUT="${OUTPUT_DIR}/map_looks/${LABEL}.png"
    if [ "$FORCE" = "0" ] && [ -f "$OUT" ]; then
      echo "  SKIP (exists) $OUT"
      LOOK_COUNT=$((LOOK_COUNT + 1))
      continue
    fi

    echo "  ${LABEL}"
    hc_get "set_projection?type=${PROJ}"
    hc_get "set_prop_overlay?type=${PROP}"
    hc_get "set_wx_overlay?type=${WX}"
    hc_get "set_theme?theme=${THEME}"
    sleep "${DELAY}"
    capture "$OUT" && LOOK_COUNT=$((LOOK_COUNT + 1))
  done
fi

# ============================================================================
# Part 5 — Theme Gallery
# One clean robinson/no-overlays shot per theme, fully dynamic.
# ============================================================================
if part_enabled themes; then
  echo ""
  echo "--- Part 5: Theme Gallery ---"
  hc_get "set_projection?type=robinson"
  hc_get "set_prop_overlay?type=none"
  hc_get "set_wx_overlay?type=none"
  # Restore a multi-widget pane so the shot shows a representative layout
  hc_get "set_pane?pane=1&action=solo&widget=solar"

  THEME_COUNT=0
  while IFS= read -r THEME; do
    OUT="${OUTPUT_DIR}/themes/${THEME}.png"
    if [ "$FORCE" = "0" ] && [ -f "$OUT" ]; then
      echo "  SKIP (exists) $OUT"
      THEME_COUNT=$((THEME_COUNT + 1))
      continue
    fi
    echo "  ${THEME}"
    hc_get "set_theme?theme=${THEME}"
    sleep "${DELAY}"
    capture "$OUT" && THEME_COUNT=$((THEME_COUNT + 1))
  done < <(echo "$CAPS" | jq -r '.themes[]')
fi

# ============================================================================
# Part 6 — UI Docs (Modals + Layout)
# ============================================================================
if part_enabled ui_docs; then
  echo ""
  echo "--- Part 6: UI Docs ---"
  hc_get "set_theme?theme=dark"
  hc_get "set_projection?type=equirectangular"
  [ -z "${SCALE:-}" ] && compute_scale

  # 1. Setup Modal (Gear icon in Time Panel)
  OUT_SETUP="${OUTPUT_DIR}/modal-setup.png"
  if [ "$FORCE" = "1" ] || [ ! -f "$OUT_SETUP" ]; then
    echo "  modal-setup"
    TP_RECT=$(curl -sf "${BASE_URL}/get_timepanel_rect" || echo '{"logical":{"x":661,"y":0,"w":139,"h":148}}')
    GEAR_X=$(echo "$TP_RECT" | jq '.logical.x + .logical.w - 20')
    GEAR_Y=$(echo "$TP_RECT" | jq '.logical.y + 20')
    debug_click $GEAR_X $GEAR_Y
    sleep 1
    capture "$OUT_SETUP"
    debug_key "Escape"
    sleep 0.5
  fi

  # 2. Presets Modal (Star icon in Time Panel)
  OUT_PRESETS="${OUTPUT_DIR}/timepanel-presets-modal.png"
  if [ "$FORCE" = "1" ] || [ ! -f "$OUT_PRESETS" ]; then
    echo "  modal-presets"
    TP_RECT=$(curl -sf "${BASE_URL}/get_timepanel_rect" || echo '{"logical":{"x":661,"y":0,"w":139,"h":148}}')
    STAR_X=$(echo "$TP_RECT" | jq '.logical.x + .logical.w - 50')
    STAR_Y=$(echo "$TP_RECT" | jq '.logical.y + 20')
    debug_click $STAR_X $STAR_Y
    sleep 1
    FULL="${OUTPUT_DIR}/.full_presets.png"
    if capture "$FULL"; then
      # Presets modal is centered, 420x330 logical
      PX=$(awk "BEGIN { printf \"%d\", (800-420)/2 * $SCALE }")
      PY=$(awk "BEGIN { printf \"%d\", (480-330)/2 * $SCALE }")
      PW=$(awk "BEGIN { printf \"%d\", 420 * $SCALE }")
      PH=$(awk "BEGIN { printf \"%d\", 330 * $SCALE }")
      magick "$FULL" -crop "${PW}x${PH}+${PX}+${PY}" +repage "$OUT_PRESETS"
      rm -f "$FULL"
    fi
    debug_key "Escape"
    sleep 0.5
  fi

  # 3. Widget Selector Modal
  OUT_SELECTOR="${OUTPUT_DIR}/modal-widget-selector.png"
  if [ "$FORCE" = "1" ] || [ ! -f "$OUT_SELECTOR" ]; then
    echo "  modal-widget-selector"
    P1_RECT=$(curl -sf "${BASE_URL}/get_pane_rect?pane=1" || echo '{"logical":{"x":0,"y":0,"w":130,"h":110}}')
    # Click top 10% of pane
    P1_X=$(echo "$P1_RECT" | jq '.logical.x + .logical.w/2')
    P1_Y=$(echo "$P1_RECT" | jq '.logical.y + 5')
    debug_click $P1_X $P1_Y
    sleep 1
    FULL="${OUTPUT_DIR}/.full_selector.png"
    if capture "$FULL"; then
      PX=$(awk "BEGIN { printf \"%d\", 20 * $SCALE }")
      PY=$(awk "BEGIN { printf \"%d\", 20 * $SCALE }")
      PW=$(awk "BEGIN { printf \"%d\", 760 * $SCALE }")
      PH=$(awk "BEGIN { printf \"%d\", 440 * $SCALE }")
      magick "$FULL" -crop "${PW}x${PH}+${PX}+${PY}" +repage "$OUT_SELECTOR"
      rm -f "$FULL"
    fi
    debug_key "Escape"
    sleep 0.5
  fi

  # 4. Pane rotation indicator
  OUT_ROT="${OUTPUT_DIR}/pane-rotation-indicator.png"
  if [ "$FORCE" = "1" ] || [ ! -f "$OUT_ROT" ]; then
    echo "  pane-rotation-indicator"
    hc_get "set_pane?pane=1&action=solo&widget=solar"
    hc_get "set_pane?pane=1&action=add&widget=band_conditions"
    sleep 1
    FULL="${OUTPUT_DIR}/.full_rot.png"
    if capture "$FULL"; then
      P1_RECT=$(curl -sf "${BASE_URL}/get_pane_rect?pane=1" || echo '{"logical":{"x":0,"y":0,"w":130,"h":110}}')
      # Indicator is at top of pane
      PX=$(echo "$P1_RECT" | jq ".logical.x + .logical.w/2 - 30")
      PY=$(echo "$P1_RECT" | jq ".logical.y")
      PX_PX=$(awk "BEGIN { printf \"%d\", $PX * $SCALE }")
      PY_PX=$(awk "BEGIN { printf \"%d\", $PY * $SCALE }")
      PW_PX=$(awk "BEGIN { printf \"%d\", 60 * $SCALE }")
      PH_PX=$(awk "BEGIN { printf \"%d\", 15 * $SCALE }")
      magick "$FULL" -crop "${PW_PX}x${PH_PX}+${PX_PX}+${PY_PX}" +repage "$OUT_ROT"
      rm -f "$FULL"
    fi
  fi

  # 5. K-mode highlight
  OUT_K="${OUTPUT_DIR}/key-highlight-mode.png"
  if [ "$FORCE" = "1" ] || [ ! -f "$OUT_K" ]; then
    echo "  key-highlight-mode"
    debug_key "K"
    sleep 0.5
    capture "$OUT_K"
    debug_key "K"
    sleep 0.5
  fi

  # 6. Annotated Layout
  OUT_LAYOUT="${OUTPUT_DIR}/layout-annotated.png"
  if [ "$FORCE" = "1" ] || [ ! -f "$OUT_LAYOUT" ]; then
    echo "  layout-annotated"
    FULL="${OUTPUT_DIR}/.full_layout.png"
    if capture "$FULL"; then
       magick "$FULL" \
         -fill none -stroke red -strokewidth 3 \
         -draw "rectangle $(awk "BEGIN { printf \"%d,%d %d,%d\", 139*$SCALE, 0*$SCALE, 799*$SCALE, 148*$SCALE }")" \
         -annotate +$(awk "BEGIN { printf \"%d\", 145*$SCALE }")+$(awk "BEGIN { printf \"%d\", 25*$SCALE }") "Time Panel" \
         -stroke blue \
         -draw "rectangle $(awk "BEGIN { printf \"%d,%d %d,%d\", 139*$SCALE, 149*$SCALE, 799*$SCALE, 479*$SCALE }")" \
         -annotate +$(awk "BEGIN { printf \"%d\", 145*$SCALE }")+$(awk "BEGIN { printf \"%d\", 175*$SCALE }") "Map & Overlays" \
         -stroke green \
         -draw "rectangle $(awk "BEGIN { printf \"%d,%d %d,%d\", 0*$SCALE, 0*$SCALE, 138*$SCALE, 479*$SCALE }")" \
         -annotate +$(awk "BEGIN { printf \"%d\", 5*$SCALE }")+$(awk "BEGIN { printf \"%d\", 25*$SCALE }") "Panes" \
         "$OUT_LAYOUT"
       rm -f "$FULL"
    fi
  fi
fi

# ============================================================================
# Part 7 — Wiki MD Generation
# Regenerate Widget-Gallery.md, Map-and-Overlays.md (theme section),
# and Widgets.md (map looks section) inline from captured images.
# ============================================================================
if part_enabled wiki; then
  echo ""
  echo "--- Part 7: Wiki MD Generation ---"
  WIKI_DIR="docs/wiki"

  # ---- 6a: Widget-Gallery.md ----
  WIDGET_IMG_DIR="${OUTPUT_DIR}/widgets"
  WIDGET_GALLERY="${WIKI_DIR}/Widget-Gallery.md"
  WIDGET_TYPE_H="src/core/WidgetType.h"

  if [ ! -f "${WIDGET_TYPE_H}" ]; then
    echo "  WARN: ${WIDGET_TYPE_H} not found — skipping Widget-Gallery.md"
  elif [ ! -d "${WIDGET_IMG_DIR}" ]; then
    echo "  WARN: ${WIDGET_IMG_DIR} not found — skipping Widget-Gallery.md"
  else
    echo "  Generating ${WIDGET_GALLERY} ..."

    # Parse slug→display_name from WidgetType.h widgetTypeDisplayName()
    # Format: "case WidgetType::FOO: return "Foo Name";"
    declare -A WIDGET_NAMES
    declare -a WIDGET_SLUGS

    if [ "$(echo "$CAPS" | jq -r '.widget_meta // empty')" ]; then
      # Use server-provided metadata (the new way)
      while IFS= read -r ITEM; do
        SLUG=$(echo "$ITEM" | jq -r '.id')
        NAME=$(echo "$ITEM" | jq -r '.displayName')
        WIDGET_NAMES["$SLUG"]="$NAME"
        WIDGET_SLUGS+=("$SLUG")
      done < <(echo "$CAPS" | jq -c '.widget_meta[] | select(.requiresKey == false)')
      # Sort by display name for consistent gallery
      IFS=$'\n' WIDGET_SLUGS=($(for s in "${WIDGET_SLUGS[@]}"; do echo "${WIDGET_NAMES[$s]}|$s"; done | sort | cut -d'|' -f2))
      unset IFS
    else
      # Fallback: Parse from WidgetType.h (old way)
      while IFS= read -r LINE; do
        SLUG=$(echo "$LINE" | sed -n 's/.*widgetTypeToString.*"\([^"]*\)".*/\1/p')
        [ -n "$SLUG" ] && WIDGET_SLUGS+=("$SLUG")
      done < <(grep 'return "' "${WIDGET_TYPE_H}" | grep -v '//' | head -80)

      IN_DISPLAY=0
      while IFS= read -r LINE; do
        if echo "$LINE" | grep -q "widgetTypeDisplayName"; then IN_DISPLAY=1; fi
        if [ "$IN_DISPLAY" = "1" ]; then
          SLUG=$(echo "$LINE" | sed -n 's/.*WidgetType::\([A-Z_]*\).*/\1/p' | tr '[:upper:]' '[:lower:]' | tr '_' '_')
          NAME=$(echo "$LINE" | sed -n 's/.*return "\([^"]*\)".*/\1/p')
          if [ -n "$SLUG" ] && [ -n "$NAME" ]; then
            WIDGET_NAMES["$SLUG"]="$NAME"
          fi
          if echo "$LINE" | grep -q "^}" && [ "$IN_DISPLAY" = "1" ]; then IN_DISPLAY=0; fi
        fi
      done < "${WIDGET_TYPE_H}"
    fi

    # Build gallery sections
    GALLERY_ROWS=""
    MISSING_LIST=""
    ROW_BUF=""
    ROW_IDX=0
    for SLUG in "${WIDGET_SLUGS[@]}"; do
      IMG="${WIDGET_IMG_DIR}/${SLUG}.png"
      DNAME="${WIDGET_NAMES[$SLUG]:-$SLUG}"
      if [ -f "$IMG" ]; then
        CELL="**${DNAME}**<br>![${SLUG}](images/widgets/${SLUG}.png)"
        if [ "$ROW_IDX" = "0" ]; then
          ROW_BUF="| ${CELL}"
          ROW_IDX=1
        else
          GALLERY_ROWS+="| | |"$'\n'"|---|---|"$'\n'"${ROW_BUF} | ${CELL} |"$'\n'
          ROW_BUF=""
          ROW_IDX=0
        fi
      else
        MISSING_LIST+="- \`${SLUG}\` — ${DNAME}"$'\n'
      fi
    done
    # Flush odd row
    if [ "$ROW_IDX" = "1" ]; then
      GALLERY_ROWS+="| | |"$'\n'"|---|---|"$'\n'"${ROW_BUF} |  |"$'\n'
    fi

    # Maximized section
    MAX_ROWS=""
    MAX_BUF=""
    MAX_IDX=0
    for IMG in "${WIDGET_IMG_DIR}"/*_maximized.png; do
      [ -f "$IMG" ] || continue
      BASENAME=$(basename "$IMG" .png)
      SLUG="${BASENAME%_maximized}"
      DNAME="${WIDGET_NAMES[$SLUG]:-$SLUG}"
      CELL="**${DNAME} (maximized)**<br>![${SLUG}](images/widgets/${BASENAME}.png)"
      if [ "$MAX_IDX" = "0" ]; then
        MAX_BUF="| ${CELL}"
        MAX_IDX=1
      else
        MAX_ROWS+="| | |"$'\n'"|---|---|"$'\n'"${MAX_BUF} | ${CELL} |"$'\n'
        MAX_BUF=""
        MAX_IDX=0
      fi
    done
    if [ "$MAX_IDX" = "1" ]; then
      MAX_ROWS+="| | |"$'\n'"|---|---|"$'\n'"${MAX_BUF} |  |"$'\n'
    fi

    # Write Widget-Gallery.md
    {
      echo "# Widget Gallery"
      echo ""
      echo "Visual reference for all HamClock-Next widgets. Run \`capture_wiki_screenshots.sh\` to populate or refresh screenshots."
      echo ""
      echo "## All Widgets"
      echo ""
      if [ -n "$GALLERY_ROWS" ]; then
        echo "$GALLERY_ROWS"
      else
        echo "_No widget screenshots captured yet. Run \`capture_wiki_screenshots.sh\`._"
      fi
      if [ -n "$MAX_ROWS" ]; then
        echo ""
        echo "## Maximized Widgets"
        echo ""
        echo "Widgets expanded to fill the map area (via the maximize button on each pane)."
        echo ""
        echo "$MAX_ROWS"
      fi
      # Fixed UI
      if [ -f "${WIDGET_IMG_DIR}/time_panel.png" ] || [ -f "${WIDGET_IMG_DIR}/rss_banner.png" ]; then
        echo ""
        echo "## Fixed UI Elements"
        echo ""
        echo "| | |"
        echo "|---|---|"
        TP_CELL=""
        RSS_CELL=""
        [ -f "${WIDGET_IMG_DIR}/time_panel.png" ] && TP_CELL="**Time Panel**<br>![time_panel](images/widgets/time_panel.png)"
        [ -f "${WIDGET_IMG_DIR}/rss_banner.png" ] && RSS_CELL="**RSS Banner**<br>![rss_banner](images/widgets/rss_banner.png)"
        echo "| ${TP_CELL} | ${RSS_CELL} |"
      fi
      if [ -n "$MISSING_LIST" ]; then
        echo ""
        echo "## Not Yet Captured"
        echo ""
        echo "$MISSING_LIST"
      fi
    } > "${WIDGET_GALLERY}"
    echo "  ✓ ${WIDGET_GALLERY}"
  fi

  # ---- 6b: Map-and-Overlays.md theme section ----
  MAP_MD="${WIKI_DIR}/Map-and-Overlays.md"
  THEMES_DIR="${OUTPUT_DIR}/themes"
  if [ -f "${MAP_MD}" ]; then
    THEME_TABLE=""
    if [ -d "${THEMES_DIR}" ]; then
      T_BUF=""
      T_IDX=0
      for IMG in "${THEMES_DIR}"/*.png; do
        [ -f "$IMG" ] || continue
        STEM=$(basename "$IMG" .png)
        LABEL=$(echo "$STEM" | sed 's/_/ /g' | awk '{for(i=1;i<=NF;i++) $i=toupper(substr($i,1,1)) substr($i,2); print}')
        CELL="**${LABEL}**<br>![${STEM}](images/themes/${STEM}.png)"
        if [ "$T_IDX" = "0" ]; then
          T_BUF="| ${CELL}"
          T_IDX=1
        else
          THEME_TABLE+="| | |"$'\n'"|---|---|"$'\n'"${T_BUF} | ${CELL} |"$'\n'
          T_BUF=""
          T_IDX=0
        fi
      done
      if [ "$T_IDX" = "1" ]; then
        THEME_TABLE+="| | |"$'\n'"|---|---|"$'\n'"${T_BUF} |  |"$'\n'
      fi
    fi
    if [ -z "$THEME_TABLE" ]; then
      THEME_TABLE="_No theme screenshots yet. Run \`capture_wiki_screenshots.sh\`._"$'\n'
    fi

    # Replace bounded block using awk
    awk -v replacement="${THEME_TABLE}" '
      /<!-- BEGIN THEME GALLERY -->/ { print; print replacement; inside=1; next }
      /<!-- END THEME GALLERY -->/   { inside=0 }
      !inside                        { print }
    ' "${MAP_MD}" > "${MAP_MD}.tmp" && mv "${MAP_MD}.tmp" "${MAP_MD}"
    echo "  ✓ ${MAP_MD} (theme gallery updated)"
  else
    echo "  WARN: ${MAP_MD} not found — skip theme gallery update"
  fi

  # ---- 6c: Widgets.md map looks section ----
  WIDGETS_MD="${WIKI_DIR}/Widgets.md"
  MAP_LOOKS_DIR="${OUTPUT_DIR}/map_looks"
  if [ -f "${WIDGETS_MD}" ]; then
    ML_TABLE=""
    if [ -d "${MAP_LOOKS_DIR}" ]; then
      ML_BUF=""
      ML_IDX=0
      for IMG in "${MAP_LOOKS_DIR}"/*.png; do
        [ -f "$IMG" ] || continue
        STEM=$(basename "$IMG" .png)
        CELL="![${STEM}](images/map_looks/${STEM}.png)"
        if [ "$ML_IDX" = "0" ]; then
          ML_BUF="| ${CELL}"
          ML_IDX=1
        else
          ML_TABLE+="| | |"$'\n'"|---|---|"$'\n'"${ML_BUF} | ${CELL} |"$'\n'
          ML_BUF=""
          ML_IDX=0
        fi
      done
      if [ "$ML_IDX" = "1" ]; then
        ML_TABLE+="| | |"$'\n'"|---|---|"$'\n'"${ML_BUF} |  |"$'\n'
      fi
    fi
    if [ -z "$ML_TABLE" ]; then
      ML_TABLE="_No map look screenshots yet. Run \`capture_wiki_screenshots.sh\`._"$'\n'
    fi

    awk -v replacement="${ML_TABLE}" '
      /<!-- BEGIN MAP LOOKS -->/ { print; print replacement; inside=1; next }
      /<!-- END MAP LOOKS -->/   { inside=0 }
      !inside                    { print }
    ' "${WIDGETS_MD}" > "${WIDGETS_MD}.tmp" && mv "${WIDGETS_MD}.tmp" "${WIDGETS_MD}"
    echo "  ✓ ${WIDGETS_MD} (map looks updated)"
  else
    echo "  WARN: ${WIDGETS_MD} not found — skip map looks update"
  fi
fi

# ============================================================================
echo ""
echo "=== Complete ==="
if [ ${#FAILED[@]} -gt 0 ]; then
  echo "  WARNINGS — ${#FAILED[@]} failure(s):"
  for F in "${FAILED[@]}"; do echo "    - $F"; done
else
  echo "  No failures."
fi
echo ""
