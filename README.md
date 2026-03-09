# Ghost Panel 👻

A lightweight C++ tool and bash wrapper to change XFCE's panel autohide behavior. 

By default, an autohiding XFCE panel will simply overlap your maximized windows when it unhides. My program forces the panel to dynamically resize your maximized windows to make room for the panel when it appears, and expand them back when it hides.

## ⚠️ Limitations
* **Single Monitor Only:** This tool currently only supports single-monitor setups. Multi-monitor support requires complex X11 coordinate tracking that is not yet implemented.

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
1. Fetch and compile the project:
   ```bash
   git clone https://github.com/yoav-i7/ghost-panel.git
   cd ghost-panel
   make
   ```

2. Configure the wrapper:
   Open `config.cfg` in a text editor and edit the configuration variables to match your XFCE setup if necessary. By default, it targets Panel 1 with a height of 28 pixels.

3. (Optional) Install the binary and scripts to ~/.local/bin:
   ```bash
   make install
   ```
**Note:** If you choose *not* to run `make install`, you will need to manually make the scripts executable (`chmod +x *.sh`) and run `./start-ghost-panel.sh` directly from your cloned directory.


## Usage
**Do not run the compiled `ghost-panel` binary directly.** You need to run the bash wrapper, which dynamically finds your active XFCE panel ID and feeds it to the engine.
```bash
./start-ghost-panel.sh
```

**Autostart & Toggling:**
Add `start-ghost-panel.sh` (after installing) to your XFCE 'Session and Startup' > 'Application Autostart' menu. 
If you want to temporarily disable the autohide behavior and lock the panel open, run `toggle-ghost-panel.sh`.

## Recommendations
Because this tool rapidly modifies X11 window properties, you might see screen tearing when the panel toggles.
* I highly recommend using **Picom** instead of XFCE's default `xfwm4` compositor.
* In your `picom.conf`, ensure `vsync = true`, `use-damage = false` and `xrender-sync-fence = true` are set.
* I also recommend to enable slide-in animations for the panel and geometry-change animations for all normal windows in picom to make the panel slide in smoothly instead of appearing and disappearing.
