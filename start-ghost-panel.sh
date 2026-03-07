#!/bin/bash

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" &> /dev/null && pwd)"
BINARY="$SCRIPT_DIR/ghost-panel"

# Load shared configuration
if [[ -f "$SCRIPT_DIR/config.cfg" ]]; then
    source "$SCRIPT_DIR/config.cfg"
else
    echo "Error: config.cfg not found in $SCRIPT_DIR"
    exit 1
fi

# Dependency checks
command -v xdotool >/dev/null 2>&1 || { echo >&2 "Error: xdotool is required but not installed. Aborting."; exit 1; }
command -v xwininfo >/dev/null 2>&1 || { echo >&2 "Error: x11-utils (xwininfo) is required but not installed. Aborting."; exit 1; }

get_panel_id() {
    for id in $(xdotool search --class "xfce4-panel" 2>/dev/null); do
        local info=$(xwininfo -id "$id" 2>/dev/null)
        local width=$(echo "$info" | awk '/Width/ {print $2}')
        local height=$(echo "$info" | awk '/Height/ {print $2}')

        # A primary panel usually spans most of a monitor's edge.
        # If it's wider than 500px (Top/Bottom) OR taller than 500px (Left/Right)
        if [[ "$width" -gt 500 ]] || [[ "$height" -gt 500 ]]; then
            echo "$id"
            return
        fi
    done
}

# Infinite loop to keep the C++ engine alive
while true; do
    PANEL_ID=$(get_panel_id)

    if [[ -n "$PANEL_ID" ]]; then
        echo "Found Panel ID: $PANEL_ID. Starting engine..."
        # Run the binary and wait for it to exit
        "$BINARY" "$PANEL_ID" "$PANEL_NUMBER" "$PANEL_HEIGHT"
        echo "Engine stopped. Re-searching in 2 seconds..."
    else
        echo "Could not find panel. Retrying..."
    fi

    sleep 2
done
