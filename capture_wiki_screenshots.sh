#!/bin/bash

# Configuration
HC_IP="192.168.1.152"
HC_PORT="8080"
BASE_URL="http://${HC_IP}:${HC_PORT}"
NUM_ROTATIONS=5
DELAY=5 # Increased for RPi 3B mesh/data processing

echo "--- HamClock-Next Wiki Screenshot Tool (Multi-Look Edition) ---"

# Define the "looks" we want to showcase
# Format: "projection|prop_overlay|wx_overlay|label"
LOOKS=(
    "robinson|muf|none|robinson_muf"
    "dual_azimuthal|none|clouds|dual_az_clouds"
    "azimuthal|aurora|none|azimuthal_aurora"
    "equirectangular|drap|wx_mb|flat_drap_isobars"
)

# 1. Pause rotation
echo "Pausing rotation..."
curl -s "${BASE_URL}/api/panes/pause_all?paused=1" > /dev/null

# 2. Looks Loop
for LOOK in "${LOOKS[@]}"
do
    IFS='|' read -r PROJ PROP WX LABEL <<< "$LOOK"

    echo "--- Setting Look: $LABEL ($PROJ, $PROP, $WX) ---"
    # Use new surgical endpoints to avoid dashboard reset
    curl -s "${BASE_URL}/set_projection?type=${PROJ}" > /dev/null
    curl -s "${BASE_URL}/set_prop_overlay?type=${PROP}" > /dev/null
    curl -s "${BASE_URL}/set_wx_overlay?type=${WX}" > /dev/null
    echo "  Waiting ${DELAY}s for mesh/data update..."
    sleep $DELAY

    # 3. Rotation Loop
    for ((i=1; i<=NUM_ROTATIONS; i++))
    do
        echo "  Rotation $i of $NUM_ROTATIONS..."
        
        # Get active widgets for the filename
        RAW_WIDGETS=$(curl -s "${BASE_URL}/get_active_pane.txt")
        
        # Process into a filename-safe string
        CLEAN_NAMES=$(echo "$RAW_WIDGETS" | sed 's/Pane[0-9]  //g' | tr '\n' '_' | sed 's/__/_/g' | sed 's/_$//' | tr '[:upper:]' '[:lower:]' | sed 's/ /_/g')
        
        TIMESTAMP=$(date +"%H%M%S")
        FILENAME="hc_${LABEL}_${CLEAN_NAMES}_${TIMESTAMP}.jpg"
        
        echo "    Capturing: $FILENAME"
        curl -s "${BASE_URL}/get_capture" -o "$FILENAME"
        
        if [ $i -lt $NUM_ROTATIONS ]; then
            echo "    Advancing to next rotation..."
            curl -s "${BASE_URL}/api/panes/rotate_all" > /dev/null
            echo "    Waiting ${DELAY}s for render..."
            sleep $DELAY
        fi
    done
done

# 4. Resume rotation
echo "Resuming rotation..."
curl -s "${BASE_URL}/api/panes/pause_all?paused=0" > /dev/null

echo "--- Done! All looks and rotations captured. ---"
