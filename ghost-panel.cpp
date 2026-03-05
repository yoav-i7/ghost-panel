#include <X11/Xlib.h>
#include <X11/Xatom.h>
#include <X11/Xutil.h>
#include <X11/Xproto.h>
#include <iostream>
#include <cstdlib>
#include <unistd.h>
#include <string>
#include <vector>
#include <algorithm>
#include <signal.h>

const int THRESHOLD = 32;
const int DEBOUNCE_LIMIT = 6;
const std::string PANEL_PROP = "/panels/panel-1/autohide-behavior";

Window panelWin = None;

Atom net_active_window;
Atom net_wm_state;
Atom state_max_v;
Atom net_wm_window_type;
Atom type_menu;
Atom type_dropdown;
Atom type_popup;
Atom type_tooltip;

// --- X11 ERROR HANDLER ---
int myErrorHandler(Display *display, XErrorEvent *error) {
    (void)display;
    if (error->error_code == BadWindow || error->error_code == BadDrawable) {
        // If the window that died was specifically our top panel...
        if (error->resourceid == panelWin) {
            std::cerr << "FATAL: Panel window lost. Exiting for restart..." << std::endl;
            exit(1); 
        }
        // Otherwise, it was just a normal app closing (like a browser).
        return 0; 
    }
    return 0;
}

// Helper to check if a window is a "normal" app window
bool isNormalWindow(Display* display, Window win, Window panelWin) {
    if (win == None || win == panelWin) return false;

    XClassHint chinte;
    if (XGetClassHint(display, win, &chinte)) {
        std::string name = chinte.res_name ? chinte.res_name : "";
        std::string class_nm = chinte.res_class ? chinte.res_class : "";
        if (chinte.res_name) XFree(chinte.res_name);
        if (chinte.res_class) XFree(chinte.res_class);

        // Filter out desktop and panel
        if (class_nm.find("xfce4-panel") != std::string::npos || 
            class_nm.find("xfdesktop") != std::string::npos) {
            return false;
        }
    }
    return true;
}

// Helper to run the D-Bus command asynchronously
void runXfconf(const std::string& val) {
    pid_t pid = fork();

    if (pid == -1) {
        std::cerr << "Failed to fork process for xfconf-query\n";
        return;
    } else if (pid == 0) {
        // --- CHILD PROCESS ---
        const char* args[] = {
            "xfconf-query",
            "-c", "xfce4-panel",
            "-p", "/panels/panel-1/autohide-behavior",
            "-s", val.c_str(),
            NULL
        };

        freopen("/dev/null", "w", stderr);
        execvp(args[0], const_cast<char* const*>(args));
        exit(1);
    }
    // --- PARENT PROCESS ---
    // Returns immediately without waitpid()
}

// Gets the currently active window ID
Window getActiveWindow(Display* display, Window root) {
    Atom actual_type;
    int actual_format;
    unsigned long nitems, bytes_after;
    unsigned char* prop = NULL;

    if (XGetWindowProperty(display, root, net_active_window, 0, 1, False, AnyPropertyType,
                           &actual_type, &actual_format, &nitems, &bytes_after, &prop) == Success && prop != NULL) {
        Window activeWin = *((Window*)prop);
        XFree(prop);
        return activeWin;
    }
    return None;
}

// Checks if a specific window has the maximized property
bool isMaximized(Display* display, Window win) {
    if (win == None) return false;

    Atom actual_type;
    int actual_format;
    unsigned long nitems, bytes_after;
    unsigned char* prop = NULL;

    bool maximized = false;
    if (XGetWindowProperty(display, win, net_wm_state, 0, 1024, False, XA_ATOM,
                           &actual_type, &actual_format, &nitems, &bytes_after, &prop) == Success && prop != NULL) {
        Atom* atoms = (Atom*)prop;
        for (unsigned long i = 0; i < nitems; ++i) {
            if (atoms[i] == state_max_v) {
                maximized = true;
                break;
            }
        }
        XFree(prop);
    }
    return maximized;
}

// Helper to check if the mouse is hovering over a dropdown, popup, or tooltip
bool isMenuOrTooltip(Display* display, Window win, Window panelWin) {
    if (win == None) return false;
    if (win == panelWin) return true; // Hovering the panel itself is safe

    // 1. Override Redirect (Fastest catch for 95% of tooltips and popups)
    XWindowAttributes attrs;
    if (XGetWindowAttributes(display, win, &attrs) && attrs.override_redirect) return true;

    // 2. EWMH Window Type check (For GTK menus and standard drop-downs)
    Atom actual_type;
    int actual_format;
    unsigned long nitems, bytes_after;
    unsigned char* prop = NULL;
    bool isMenu = false;

    if (XGetWindowProperty(display, win, net_wm_window_type, 0, 1024, False, XA_ATOM,
                           &actual_type, &actual_format, &nitems, &bytes_after, &prop) == Success && prop != NULL) {
        Atom* atoms = (Atom*)prop;
        for (unsigned long i = 0; i < nitems; ++i) {
            if (atoms[i] == type_menu || atoms[i] == type_dropdown ||
                atoms[i] == type_popup || atoms[i] == type_tooltip) {
                isMenu = true;
                break;
            }
        }
        XFree(prop);
        if (isMenu) return true;
    }

    // 3. Fallback for specific XFCE menus (Whisker Menu, wrappers, etc.)
    XClassHint chinte;
    if (XGetClassHint(display, win, &chinte)) {
        std::string class_nm = chinte.res_class ? chinte.res_class : "";
        std::string name = chinte.res_name ? chinte.res_name : "";
        if (chinte.res_name) XFree(chinte.res_name);
        if (chinte.res_class) XFree(chinte.res_class);

        if (class_nm.find("xfce4-popup") != std::string::npos ||
            name.find("whiskermenu") != std::string::npos ||
            class_nm.find("Wrapper") != std::string::npos) {
            return true;
        }
    }
    return false;
}

int main(int argc, char* argv[]) {
    if (argc < 2) {
        std::cerr << "Usage: " << argv[0] << " <PANEL_ID>\n";
        return 1;
    }

    signal(SIGCHLD, SIG_IGN); // Ignore child processes to prevent zombies when using fork
    panelWin = std::stoul(argv[1]); // Convert string ID to X11 Window type

    Display* display = XOpenDisplay(NULL);
    if (!display) {
        std::cerr << "Cannot open display\n";
        return 1;
    }

    net_active_window = XInternAtom(display, "_NET_ACTIVE_WINDOW", False);
    net_wm_state = XInternAtom(display, "_NET_WM_STATE", False);
    state_max_v = XInternAtom(display, "_NET_WM_STATE_MAXIMIZED_VERT", False);
    
    net_wm_window_type = XInternAtom(display, "_NET_WM_WINDOW_TYPE", False);
    type_menu = XInternAtom(display, "_NET_WM_WINDOW_TYPE_MENU", False);
    type_dropdown = XInternAtom(display, "_NET_WM_WINDOW_TYPE_DROPDOWN_MENU", False);
    type_popup = XInternAtom(display, "_NET_WM_WINDOW_TYPE_POPUP_MENU", False);
    type_tooltip = XInternAtom(display, "_NET_WM_WINDOW_TYPE_TOOLTIP", False);

    XSetErrorHandler(myErrorHandler);
    Window root = DefaultRootWindow(display);
    XSelectInput(display, root, PropertyChangeMask);

    bool maxState = false;
    bool panelVisible = true;
    int counter = 0;

    std::cout << "Ghost Panel Started for Panel ID: " << panelWin << "\n";
    Window lastNormalWin = None;
    Window lastHoveredWin = None;
    	bool isCurrentlyMenu = false;

    XEvent event;
    while (true) {
        if (!maxState) {
            XNextEvent(display, &event);
            // Filter: We only care if a property changed
            if (event.type != PropertyNotify) continue; 
            // Filter: We only care if the active window changed or a window's state changed
            if (event.xproperty.atom != net_active_window && event.xproperty.atom != net_wm_state) continue; 
            Window active = getActiveWindow(display, root);
            if (active != lastNormalWin && isNormalWindow(display, active, panelWin)) lastNormalWin = active;
            if (isMaximized(display, lastNormalWin)) {
                maxState = true;
                if (panelVisible) {
                    XUnmapWindow(display, panelWin);
                    XFlush(display); // Force X server to update immediately
                    runXfconf("0");
                    panelVisible = false;
                }
                counter = DEBOUNCE_LIMIT;
            }
        } else {
            while (XPending(display) > 0) XNextEvent(display, &event);
            Window active = getActiveWindow(display, root);
            if (active != lastNormalWin && isNormalWindow(display, active, panelWin)) lastNormalWin = active;
            if (!isMaximized(display, lastNormalWin)) {
                maxState = false;
                if (!panelVisible) {
                    runXfconf("1");
                    XMapWindow(display, panelWin);
                    XFlush(display);
                    panelVisible = true;
                }
                continue;
            }

            // Native Mouse Tracking
            Window root_ret, child_ret;
            int rx, ry, wx, wy;
            unsigned int mask;
            XQueryPointer(display, root, &root_ret, &child_ret, &rx, &ry, &wx, &wy, &mask);

            if (child_ret != lastHoveredWin) {
                lastHoveredWin = child_ret;
                // Only run the heavy function once per window!
                isCurrentlyMenu = isMenuOrTooltip(display, child_ret, panelWin); 
            }

            if (ry <= 8) {
                if (!panelVisible) {
                    XMapWindow(display, panelWin);
                    XFlush(display);
                    panelVisible = true;
                }
                counter = 0;
            } else if (panelVisible && isCurrentlyMenu) {
                counter = 0; 
            } else if (ry > 8 && ry <= THRESHOLD) {
                // Buffer
            } else {
                if (panelVisible) {
                    if (counter < DEBOUNCE_LIMIT) {
                        counter++;
                    } else {
                        XUnmapWindow(display, panelWin);
                        XFlush(display);
                        panelVisible = false;
                    }
                }
            }
            usleep(50000); // 0.05 seconds
        }
    }

    XCloseDisplay(display);
    return 0;
}
