#include <X11/Xlib.h>
#include <X11/Xatom.h>
#include <X11/Xutil.h>
#include <X11/Xproto.h>
#include <iostream>
#include <cstdlib>
#include <string>
#include <unordered_map>
#include <poll.h>

const int DEBOUNCE_LIMIT = 14;

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
bool isNormalWindow(Display* display, Window win) {
    if (win == None || win == panelWin) return false;

    XClassHint classHint;
    if (XGetClassHint(display, win, &classHint)) {
        std::string className = classHint.res_class ? classHint.res_class : "";
        if (classHint.res_name) XFree(classHint.res_name);
        if (classHint.res_class) XFree(classHint.res_class);

        // Filter out desktop and panel
        if (className.find("xfce4-panel") != std::string::npos || 
            className.find("xfdesktop") != std::string::npos) {
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
bool isMenuOrTooltip(Display* display, Window win, std::unordered_map<Window, bool>& cache) {
    if (win == None) return false;
    if (win == panelWin) return true; // Hovering over the panel itself is safe

    auto it = cache.find(win);
    if (it != cache.end()) return it->second;

    bool isMenu = false;

    // 1. Override Redirect (Fastest catch for 95% of tooltips and popups)
    XWindowAttributes attrs;
    if (XGetWindowAttributes(display, win, &attrs) && attrs.override_redirect) {
        isMenu = true;
    } else {
    // 2. EWMH Window Type check (For GTK menus and standard drop-downs)
        Atom actual_type;
        int actual_format;
        unsigned long nitems, bytes_after;
        unsigned char* prop = NULL;

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
        }

        // 3. Fallback for specific XFCE menus (Whisker Menu, wrappers, etc.)
        XClassHint classHint;
        if (XGetClassHint(display, win, &classHint)) {
            std::string className = classHint.res_class ? classHint.res_class : "";
            std::string name = classHint.res_name ? classHint.res_name : "";
            if (classHint.res_name) XFree(classHint.res_name);
            if (classHint.res_class) XFree(classHint.res_class);

            if (className.find("xfce4-popup") != std::string::npos ||
                name.find("whiskermenu") != std::string::npos ||
                className.find("Wrapper") != std::string::npos) {
                isMenu = true;
            }
        }
    }

    if (cache.size() > 1000) cache.clear();

    // Store in cache and return
    cache[win] = isMenu;
    return isMenu;
}

int main(int argc, char* argv[]) {
    if (argc < 3) {
    std::cerr << "Usage: " << argv[0] << " <PANEL_WINDOW_ID> <PANEL_SIZE>\n";
        return 1;
    }
    int hoverThreshold = 0;
    try {
        panelWin = std::stoul(argv[1]); // Convert string ID to X11 Window type
        hoverThreshold = std::stoi(argv[2]);
    } catch (const std::exception& e) {
        std::cerr << "FATAL: Invalid arguments provided.\n";
        return 1;
    }

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
    if (lastNormalWin != None && !isNormalWindow(display, lastNormalWin)) {
        lastNormalWin = None; 
    } else if (lastNormalWin != None) {
        XSelectInput(display, lastNormalWin, PropertyChangeMask | StructureNotifyMask);
    }

    bool panelVisible = true;
    int debounceCounter = 0;
    Window lastHoveredWin = None;
    bool isCurrentlyMenu = false;
    bool maxState = isMaximized(display, lastNormalWin);

    if (maxState) {
        XUnmapWindow(display, panelWin);
        XFlush(display);
        panelVisible = false;
        debounceCounter = DEBOUNCE_LIMIT;
    }

    std::unordered_map<Window, bool> menuCache;
    int x11_fd = ConnectionNumber(display);
    struct pollfd pfd;
    pfd.fd = x11_fd;
    pfd.events = POLLIN;

    while (true) {
        // Must flush output buffer before polling
        XFlush(display);

        // If not maximized, block indefinitely (-1). If maximized, wake up every 50ms to check mouse.
        int timeout = maxState ? 30 : -1;
        poll(&pfd, 1, timeout);

        // 1. Drain all pending X events cleanly
        while (XEventsQueued(display, QueuedAfterReading) > 0) {
            XEvent event;
            XNextEvent(display, &event);

            // Clean up tracking if the window is destroyed
            if (event.type == DestroyNotify) {
                if (event.xdestroywindow.window == lastNormalWin) lastNormalWin = None;
                continue;
            }

            // Panel mapping enforcement
            if (event.type == MapNotify && event.xmap.window == panelWin && !panelVisible) {
                XUnmapWindow(display, panelWin);
            }

            // Active window or state changed
            if (event.type == PropertyNotify) {
                if (event.xproperty.window == root && event.xproperty.atom == net_active_window) {
                    Window active = getActiveWindow(display, root);
                    
                    if (active != lastNormalWin && isNormalWindow(display, active)) {
                        if (lastNormalWin != None) XSelectInput(display, lastNormalWin, NoEventMask);
                        lastNormalWin = active;
                        XSelectInput(display, lastNormalWin, PropertyChangeMask | StructureNotifyMask);
                    }
                    bool newlyMaximized = isMaximized(display, lastNormalWin);
                    if (newlyMaximized != maxState) {
                        maxState = newlyMaximized;
                        if (maxState) {
                            if (panelVisible) {
                                XUnmapWindow(display, panelWin);
                                panelVisible = false;
                            }
                            debounceCounter = DEBOUNCE_LIMIT;
                        } else {
                            if (!panelVisible) {
                                XMapWindow(display, panelWin);
                                panelVisible = true;
                            }
                        }
                    }
                } else if (event.xproperty.window == lastNormalWin && event.xproperty.atom == net_wm_state) {
                    bool newlyMaximized = isMaximized(display, lastNormalWin);
                    if (newlyMaximized != maxState) {
                        maxState = newlyMaximized;
                        if (maxState) {
                            if (panelVisible) {
                                XUnmapWindow(display, panelWin);
                                panelVisible = false;
                            }
                            debounceCounter = DEBOUNCE_LIMIT;
                        } else {
                            if (!panelVisible) {
                                XMapWindow(display, panelWin);
                                panelVisible = true;
                            }
                        }
                    }
                }
            }
        }

        // 2. Mouse Tracking (Only executes if we are in maximized state)
        if (maxState) {
            Window root_ret, child_ret;
            int rx, ry, wx, wy;
            unsigned int mask;

            if (XQueryPointer(display, root, &root_ret, &child_ret, &rx, &ry, &wx, &wy, &mask)) {
                if (child_ret != lastHoveredWin) {
                    lastHoveredWin = child_ret;
                    isCurrentlyMenu = isMenuOrTooltip(display, child_ret, menuCache); 
                }

                if (ry <= 4) {
                    if (!panelVisible) {
                        XMapWindow(display, panelWin);
                        panelVisible = true;
                    }
                    debounceCounter = 0;
                } else if (panelVisible && isCurrentlyMenu) {
                    debounceCounter = 0;
                } else if (ry > 4 && ry <= hoverThreshold) {
                    // Buffer zone - do nothing
                } else {
                    if (panelVisible) {
                        if (debounceCounter < DEBOUNCE_LIMIT) {
                            debounceCounter++;
                        } else {
                            XUnmapWindow(display, panelWin);
                            panelVisible = false;
                        }
                    }
                }
            }
        }
    }

    XCloseDisplay(display);
    return 0;
}
