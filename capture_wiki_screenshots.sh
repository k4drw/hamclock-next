#!/bin/bash

# Configuration
HC_IP="127.0.0.1"
HC_PORT="8080"
BASE_URL="http://${HC_IP}:${HC_PORT}"
NUM_ROTATIONS=6
DELAY=5 # Increased for RPi 3B mesh/data processing

echo "--- HamClock-Next v1.0 Wiki Screenshot Tool ---"

# Define the "looks" we want to showcase for the v1.0 release
# Format: "projection|prop_overlay|wx_overlay|theme|label"
# Available Themes: dark, glass, midnight, amber, paper, matrix
LOOKS=(
    "robinson|muf|none|dark|v1_dark_robinson_muf"
    "azimuthal|aurora|none|midnight|v1_midnight_azimuthal_aurora"
    "mercator|drap|wx_mb|paper|v1_paper_mercator_drap_isobars"
    "dual_azimuthal|none|clouds_grib|glass|v1_glass_dual_az_clouds"
    "robinson|none|clouds_grib|dark|v1_dark_robinson_clouds_layered"
    "azimuthal|none|none|amber|v1_amber_clean"
    "robinson|none|none|matrix|v1_matrix_robinson"
)

# 1. Pause rotation
echo "Pausing rotation..."
curl -s "${BASE_URL}/api/panes/pause_all?paused=1" > /dev/null

# 2. Looks Loop
for LOOK in "${LOOKS[@]}"
do
    IFS='|' read -r PROJ PROP WX THEME LABEL <<< "$LOOK"

    echo "--- Setting Look: $LABEL ($PROJ, $PROP, $WX, $THEME) ---"
    
    # Apply visuals
    curl -s "${BASE_URL}/set_projection?type=${PROJ}" > /dev/null
    curl -s "${BASE_URL}/set_prop_overlay?type=${PROP}" > /dev/null
    curl -s "${BASE_URL}/set_wx_overlay?type=${WX}" > /dev/null
    curl -s "${BASE_URL}/set_theme?theme=${THEME}" > /dev/null
    
    echo "  Waiting ${DELAY}s for mesh/data/theme update..."
    sleep $DELAY

    # 3. Rotation Loop
    for ((i=1; i<=NUM_ROTATIONS; i++))
    do
        # Get active widgets for the filename to make sorting easier
        RAW_WIDGETS=$(curl -s "${BASE_URL}/get_active_pane.txt")
        
        # Process into a filename-safe string (e.g. "sat_muf_dx_de")
        CLEAN_NAMES=$(echo "$RAW_WIDGETS" | sed 's/Pane[0-9]  //g' | tr '\n' '_' | sed 's/__/_/g' | sed 's/_$//' | tr '[:upper:]' '[:lower:]' | sed 's/ /_/g')
        
        TIMESTAMP=$(date +"%H%M%S")
        FILENAME="docs/wiki/images/v1_0/hc_${LABEL}_rot${i}_${CLEAN_NAMES}.png"
        
        echo "    Capturing Rotation $i: $FILENAME"
        mkdir -p docs/wiki/images/v1_0
        curl -s "${BASE_URL}/get_capture" -o "$FILENAME"
        
        if [ $i -lt $NUM_ROTATIONS ]; then
            echo "    Advancing to next rotation..."
            curl -s "${BASE_URL}/api/panes/rotate_all" > /dev/null
            sleep $DELAY
        fi
    done
done

# 4. Resume rotation
echo "Resuming rotation..."
curl -s "${BASE_URL}/api/panes/pause_all?paused=0" > /dev/null

echo "--- Done! All v1.0 looks and rotations captured to docs/wiki/images/v1_0/ ---"
