#!/bin/bash

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" &> /dev/null && pwd)"
BINARY_PATH="$SCRIPT_DIR/ghost-panel"

get_top_panel_id() {
    for id in $(xdotool search --class "xfce4-panel" 2>/dev/null); do
        local info=$(xwininfo -id "$id" 2>/dev/null)
        local y_pos=$(echo "$info" | awk '/Absolute upper-left Y/ {print $4}')
        local width=$(echo "$info" | awk '/Width/ {print $2}')

        if [[ "$y_pos" -eq 0 ]] && [[ "$width" -gt 200 ]]; then
            echo "$id"
            return
        fi
    done
}

# Infinite loop to keep the C++ engine alive
while true; do
    PANEL_ID=$(get_top_panel_id)

    if [[ -n "$PANEL_ID" ]]; then
        echo "Found Panel ID: $PANEL_ID. Starting engine..."
        # Run the binary and WAIT for it to exit
        "$BINARY_PATH" "$PANEL_ID"
        echo "Engine stopped. Re-searching in 2 seconds..."
    else
        echo "Could not find panel. Retrying..."
    fi

    sleep 2
done
