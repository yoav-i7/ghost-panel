#!/bin/bash
# gpanel-lib.sh — Shared helpers for ghost-panel scripts.

# Finds the Window ID of the primary XFCE panel.
# A primary panel spans most of a monitor's edge (>500px wide or tall).
get_panel_id() {
    for id in $(xdotool search --class "xfce4-panel" 2>/dev/null); do
        local info width height
        info=$(xwininfo -id "$id" 2>/dev/null)
        width=$(echo "$info" | awk '/Width/ {print $2}')
        height=$(echo "$info" | awk '/Height/ {print $2}')

        if [[ "$width" -gt 500 ]] || [[ "$height" -gt 500 ]]; then
            echo "$id"
            return
        fi
    done
}
