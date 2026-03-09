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

const int DEBOUNCE_LIMIT = 8;

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
    if (argc < 3) {
    std::cerr << "Usage: " << argv[0] << " <PANEL_WINDOW_ID> <PANEL_SIZE>\n";
        return 1;
    }

    panelWin = std::stoul(argv[1]); // Convert string ID to X11 Window type
    int THRESHOLD = std::stoi(argv[2]);

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
    XSelectInput(display, panelWin, StructureNotifyMask);

    std::cout << "Ghost Panel Started for Panel ID: " << panelWin << "\n";

    Window lastNormalWin = getActiveWindow(display, root);
    if (lastNormalWin != None && !isNormalWindow(display, lastNormalWin, panelWin)) {
        lastNormalWin = None; 
    } else if (lastNormalWin != None) {
        XSelectInput(display, lastNormalWin, PropertyChangeMask);
    }

    bool panelVisible = true;
    int counter = 0;
    Window lastHoveredWin = None;
    bool isCurrentlyMenu = false;
    bool maxState = isMaximized(display, lastNormalWin);

    if (maxState) {
        XUnmapWindow(display, panelWin);
        XFlush(display);
        panelVisible = false;
        counter = DEBOUNCE_LIMIT;
    }

    XEvent event;
    while (true) {
        if (!maxState) {
            XNextEvent(display, &event);
            // Filter: We only care if a property changed and (the active window changed or a window's state changed)
            if (event.type != PropertyNotify || 
               (event.xproperty.atom != net_active_window && event.xproperty.atom != net_wm_state)) {
                continue; 
            }
            // Drain all events that are relevant to the active window or window change
            while (XPending(display) > 0) {
                XEvent nextEvent;
                XPeekEvent(display, &nextEvent);
                
                if (nextEvent.type == PropertyNotify && 
                   (nextEvent.xproperty.atom == net_active_window || nextEvent.xproperty.atom == net_wm_state)) {
                    XNextEvent(display, &nextEvent);
                } else {
                    break;
                }
            }

            Window active = getActiveWindow(display, root);
            if (active != lastNormalWin && isNormalWindow(display, active, panelWin)) {
                if (lastNormalWin != None) XSelectInput(display, lastNormalWin, NoEventMask);
                lastNormalWin = active;
                XSelectInput(display, lastNormalWin, PropertyChangeMask);
            }
            if (isMaximized(display, lastNormalWin)) {
                maxState = true;
                if (panelVisible) {
                    XUnmapWindow(display, panelWin);
                    XFlush(display);
                    panelVisible = false;
                }
                counter = DEBOUNCE_LIMIT;
            }
        } else {
            while (XPending(display) > 0) {
                XNextEvent(display, &event);
                if (event.type == MapNotify && event.xmap.window == panelWin && !panelVisible) {
                    XUnmapWindow(display, panelWin);
                    XFlush(display);
                }
                if (event.type == PropertyNotify && 
                   (event.xproperty.atom == net_active_window || event.xproperty.atom == net_wm_state)) {
                    Window active = getActiveWindow(display, root);
                    if (active != lastNormalWin && isNormalWindow(display, active, panelWin)) {
                        if (lastNormalWin != None) XSelectInput(display, lastNormalWin, NoEventMask);
                        lastNormalWin = active;
                        XSelectInput(display, lastNormalWin, PropertyChangeMask);
                    }
                }
            }

            if (!isMaximized(display, lastNormalWin)) {
                maxState = false;
                if (!panelVisible) {
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