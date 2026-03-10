#!/bin/bash

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" &> /dev/null && pwd)"
BINARY="$SCRIPT_DIR/ghost-panel"

if pgrep -x "ghost-panel" > /dev/null; then
    echo "Found existing ghost-panel instances. Terminating them..."
    killall -q -9 ghost-panel
fi

# Load shared configuration
if [[ -f "$SCRIPT_DIR/gpanel.cfg" ]]; then
    source "$SCRIPT_DIR/gpanel.cfg"
else
    echo "Error: gpanel.cfg not found in $SCRIPT_DIR"
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

cleanup() {
    echo "Stopping Ghost Panel wrapper..."
    # If the binary is running in the background, kill it
    if [[ -n "$ENGINE_PID" ]]; then
        kill -9 "$ENGINE_PID" 2>/dev/null
    fi
    exit 0
}
trap cleanup SIGINT SIGTERM

# Infinite loop to keep the C++ engine alive
while true; do
    PANEL_ID=$(get_panel_id)

    if [[ -n "$PANEL_ID" ]]; then
        echo "Found Panel ID: $PANEL_ID. Starting engine..."
        # Run the binary and wait for it to exit
        "$BINARY" "$PANEL_ID" "$PANEL_HEIGHT" &
        ENGINE_PID=$!
        wait "$ENGINE_PID"
        echo "Engine stopped. Re-searching in 2 seconds..."
    else
        echo "Could not find panel. Retrying..."
    fi

    sleep 2
done
