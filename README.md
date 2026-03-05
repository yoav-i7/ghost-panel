# Ghost Panel 👻

A lightweight C++ tool and bash wrapper to fix XFCE's panel autohide behavior. 

By default, an autohiding XFCE panel will simply overlap your maximized windows when it unhides. My program forces the panel to dynamically resize your maximized windows to make room for the panel when it appears, and expand them back when it hides.

## Features
* **Low Resource Footprint:** Instead of heavy polling, the engine blocks on native X11 `XNextEvent` calls. It uses effectively 0% CPU when windows aren't maximized.
* **Menu Aware:** Checks X11 window properties so the panel doesn't annoyingly hide while you are using dropdown menus, tooltips, or right-click contexts.
* **Crash Resilient:** The included bash wrapper automatically handles panel crashes or reloads, keeping the C++ engine attached to the correct panel ID.

## Prerequisites
You'll need standard C++ build tools, X11 headers, and a couple of X11 utilities.

**Debian/Ubuntu:**
```bash
sudo apt update
sudo apt install g++ make libx11-dev xdotool x11-utils
```

## Installation
1. Clone or download this repository.
2. Compile the C++ engine:
   ```bash
   make
   ```
3. (Optional) Install the binary to ~/.local/bin:
   ```bash
   make install
   ```

## Usage
**Do not run the compiled `ghost-panel` binary directly.** You need to run the bash wrapper, which dynamically finds your active XFCE panel ID and feeds it to the engine.

1. Make the scripts executable:
   ```bash
   chmod +x start-ghost-panel.sh toggle-ghost-panel.sh
   ```
2. Start the daemon:
   ```bash
   ./start-ghost-panel.sh
   ```

**Autostart & Toggling:**
Add `start-ghost-panel.sh` to your XFCE 'Session and Startup' > 'Application Autostart' menu. 
If you want to temporarily disable the autohide behavior and lock the panel open, run `toggle-ghost-panel.sh`.

## Troubleshooting: Screen Tearing
Because this tool rapidly modifies X11 window properties, you might see screen tearing when the panel toggles if your.
* I highly recommend using **Picom** instead of XFCE's default `xfwm4` compositor.
* In your `picom.conf`, ensure `vsync = true`, `use-damage = false` and `xrender-sync-fence = true` are set.