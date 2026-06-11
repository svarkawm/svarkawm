#include "svarkawm.h"


void SvarkaWM::SetupAtoms() {
        netatoms[NetSupported] = XInternAtom(display, "_NET_SUPPORTED", False);
        netatoms[NetWMName] = XInternAtom(display, "_NET_WM_NAME", False);
        netatoms[NetWMCheck] = XInternAtom(display, "_NET_SUPPORTING_WM_CHECK", False);
        netatoms[NetActiveWindow] = XInternAtom(display, "_NET_ACTIVE_WINDOW", False);
        netatoms[NetWMState] = XInternAtom(display, "_NET_WM_STATE", False);
        netatoms[NetWMFullscreen] = XInternAtom(display, "_NET_WM_STATE_FULLSCREEN", False);
        netatoms[NetWMWindowType] = XInternAtom(display, "_NET_WM_WINDOW_TYPE", False);
        netatoms[NetWMWindowTypeDialog] = XInternAtom(display, "_NET_WM_WINDOW_TYPE_DIALOG", False);
        netatoms[NetWMWindowTypeNormal] = XInternAtom(display, "_NET_WM_WINDOW_TYPE_NORMAL", False);
        netatoms[NetClientList] = XInternAtom(display, "_NET_CLIENT_LIST", False);
        netatoms[NetNumberOfDesktops] = XInternAtom(display, "_NET_NUMBER_OF_DESKTOPS", False);
        netatoms[NetCurrentDesktop] = XInternAtom(display, "_NET_CURRENT_DESKTOP", False);
        netatoms[NetWMIcon] = XInternAtom(display, "_NET_WM_ICON", False);
        netatoms[NetWMSkipTaskbar] = XInternAtom(display, "_NET_WM_STATE_SKIP_TASKBAR", False);
        netatoms[NetWMHidden] = XInternAtom(display, "_NET_WM_STATE_HIDDEN", False);
        netatoms[WMProtocols] = XInternAtom(display, "WM_PROTOCOLS", False);
        netatoms[WMDelete] = XInternAtom(display, "WM_DELETE_WINDOW", False);
        netatoms[WMTakeFocus] = XInternAtom(display, "WM_TAKE_FOCUS", False);
        netatoms[WMState] = XInternAtom(display, "WM_STATE", False);
        netatoms[UTF8String] = XInternAtom(display, "UTF8_STRING", False);
        netatoms[NetWMWindowTypeUtility] = XInternAtom(display, "_NET_WM_WINDOW_TYPE_UTILITY", False);
        netatoms[NetWMWindowTypeNotification] = XInternAtom(display, "_NET_WM_WINDOW_TYPE_NOTIFICATION", False);
        netatoms[NetWMWindowTypeSplash] = XInternAtom(display, "_NET_WM_WINDOW_TYPE_SPLASH", False);
        netatoms[NetWMWindowTypeToolbar] = XInternAtom(display, "_NET_WM_WINDOW_TYPE_TOOLBAR", False);
        netatoms[NetWMOpacity] = XInternAtom(display, "_NET_WM_WINDOW_OPACITY", False);

        XSetWindowAttributes wa;
        wa.override_redirect = True;
        wm_check_win = XCreateWindow(display, root, -1, -1, 1, 1, 0, 
                                    DefaultDepth(display, screen), InputOutput, 
                                    DefaultVisual(display, screen), CWOverrideRedirect, &wa);

        XChangeProperty(display, wm_check_win, netatoms[NetWMCheck], XA_WINDOW, 32, PropModeReplace, (unsigned char *)&wm_check_win, 1);
        XChangeProperty(display, wm_check_win, netatoms[NetWMName], netatoms[UTF8String], 8, PropModeReplace, (unsigned char *)"svarkawm", 8);
        XChangeProperty(display, root, netatoms[NetWMCheck], XA_WINDOW, 32, PropModeReplace, (unsigned char *)&wm_check_win, 1);

        long desktops = total_workspaces;

        XMapWindow(display, wm_check_win);
        XChangeProperty(display, root, netatoms[NetSupported], XA_ATOM, 32, PropModeReplace, (unsigned char *)netatoms, NetLast);
        XChangeProperty(display, root, netatoms[NetNumberOfDesktops], XA_CARDINAL, 32, PropModeReplace, (unsigned char *)&desktops, 1);
        XDeleteProperty(display, root, netatoms[NetClientList]);
        UpdateCurrentDesktopProperty();
    }

void SvarkaWM::UpdateCurrentDesktopProperty() {
        long current_ds = current_workspace;
        XChangeProperty(display, root, netatoms[NetCurrentDesktop], XA_CARDINAL, 32, PropModeReplace, (unsigned char *)&current_ds, 1);
    }

void SvarkaWM::UpdateClientList() {
        XDeleteProperty(display, root, netatoms[NetClientList]);
        for (const auto& c : clients) {
            XChangeProperty(display, root, netatoms[NetClientList], XA_WINDOW, 32, PropModeAppend, (unsigned char *)&(c->window), 1);
        }
        DrawBar();
    }
