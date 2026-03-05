#!/bin/bash

PANEL_PROP="/panels/panel-1/autohide-behavior"
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" &> /dev/null && pwd)"

# Dependency checks
command -v xdotool >/dev/null 2>&1 || { echo >&2 "Error: xdotool is required but not installed. Aborting."; exit 1; }
command -v xwininfo >/dev/null 2>&1 || { echo >&2 "Error: x11-utils (xwininfo) is required but not installed. Aborting."; exit 1; }

# Grab the ID just in case we need to unhide it
get_top_panel_id() {
    for id in $(xdotool search --class "xfce4-panel" 2>/dev/null); do
        local y_pos=$(xwininfo -id "$id" 2>/dev/null | awk '/Absolute upper-left Y/ {print $4}')
        if [[ "$y_pos" -eq 0 ]]; then
            echo "$id"
            return
        fi
    done
}

# Check if our pure C++ engine is currently running
if pgrep -x "ghost-panel" > /dev/null; then
    # 1. Kill the wrapper and the process
    pkill -f "start-ghost-panel.sh"
    killall ghost-panel

    # 2. Force the panel back to its default visible state
    PANEL_ID=$(get_top_panel_id)
    if [ -n "$PANEL_ID" ]; then
        xdotool windowmap "$PANEL_ID" 2>/dev/null
    fi
    xfconf-query -c xfce4-panel -p "$PANEL_PROP" -s 1 2>/dev/null

    # Optional: Send a desktop notification so you know it worked
    notify-send -t 2000 "Ghost Panel" "Disabled (Locked Open)"
else
    # It's not running, so fire up our launcher script in the background while keeping the output if we run from a terminal
    nohup "$SCRIPT_DIR/start-ghost-panel.sh" > /dev/null 2>&1 & disown

    notify-send -t 2000 "Ghost Panel" "Enabled (Smart Autohide)"
fi
