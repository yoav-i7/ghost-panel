#!/bin/bash

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" &> /dev/null && pwd)"

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

# Grab the ID in case we need to unhide it
get_panel_id() {
    for id in $(xdotool search --all --class "xfce4-panel" 2>/dev/null); do
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

# Check if our pure C++ engine is currently running
if pgrep -x "ghost-panel" > /dev/null; then
    # 1. Kill the wrapper and the process
    pkill -f "start-ghost-panel.sh"

    # 2. Force the panel back to its default visible state
    PANEL_ID=$(get_panel_id)
    if [ -n "$PANEL_ID" ]; then
        xdotool windowmap "$PANEL_ID" 2>/dev/null
    fi

    # Optional: Send a desktop notification so you know it worked
    notify-send -t 2000 "Ghost Panel" "Disabled (Locked Open)"
else
    # It's not running, so fire up our launcher script in the background while keeping the output if we run from a terminal
    nohup "$SCRIPT_DIR/start-ghost-panel.sh" > /dev/null 2>&1 & disown

    notify-send -t 2000 "Ghost Panel" "Enabled (Smart Autohide)"
fi
