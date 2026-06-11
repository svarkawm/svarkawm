#include "svarkawm.h"


void SvarkaWM::UpdateClientState(Client* c) {
        if (!c) return;
        Atom actual_type;
        int actual_format;
        unsigned long nitems, bytes_after;
        unsigned char *prop = nullptr;
        bool hidden = false;
        c->skip_taskbar = false;
        c->is_fullscreen = false;

        if (XGetWindowProperty(display, c->window, netatoms[NetWMState], 0, 1024, False, XA_ATOM,
                               &actual_type, &actual_format, &nitems, &bytes_after, &prop) == Success && prop) {
            Atom *atoms = (Atom *)prop;
            Atom modal = XInternAtom(display, "_NET_WM_STATE_MODAL", False);
            for (unsigned long i = 0; i < nitems; i++) {
                if (atoms[i] == netatoms[NetWMHidden]) hidden = true;
                if (atoms[i] == netatoms[NetWMSkipTaskbar]) c->skip_taskbar = true;
                if (atoms[i] == netatoms[NetWMFullscreen]) c->is_fullscreen = true;
                if (atoms[i] == modal) c->is_floating = true;
            }
            XFree(prop);
        }
        
        unsigned long *state_prop = nullptr;
        if (XGetWindowProperty(display, c->window, netatoms[WMState], 0, 2, False, netatoms[WMState],
                               &actual_type, &actual_format, &nitems, &bytes_after, (unsigned char**)&state_prop) == Success && state_prop) {
            if (nitems > 0 && state_prop[0] == IconicState) {
                hidden = true;
            }
            XFree(state_prop);
        }
        c->is_minimized = hidden;
    }

void SvarkaWM::SetFocus(Client* c) {
        if (!c) {
            XSetInputFocus(display, root, RevertToParent, CurrentTime);
            XDeleteProperty(display, root, netatoms[NetActiveWindow]);
            focused_client = nullptr;
        DrawBar();
            return;
        }
    if (c == focused_client) return;
        focused_client = c;
        last_focused[current_workspace] = c;
        
        for(const auto& cl_ptr : clients) {
            Client* cl = cl_ptr.get();
            if (cl->workspace == current_workspace && cl->window != bar_win) {
                XSetWindowBorder(display, cl->window, (cl == c && c->window != bar_win) ? focused_border_color : unfocused_border_color);
                UpdateClientOpacity(cl);
            }
        }

        Atom* protocols;
        int n;
        if (XGetWMProtocols(display, c->window, &protocols, &n)) {
            for (int i = 0; i < n; i++) {
                if (protocols[i] == netatoms[WMTakeFocus]) {
                    XEvent ev;
                    ev.type = ClientMessage;
                    ev.xclient.window = c->window;
                    ev.xclient.message_type = netatoms[WMProtocols];
                    ev.xclient.format = 32;
                    ev.xclient.data.l[0] = netatoms[WMTakeFocus];
                    ev.xclient.data.l[1] = CurrentTime;
                    XSendEvent(display, c->window, False, NoEventMask, &ev);
                }
            }
            XFree(protocols);
        }

        XSetInputFocus(display, c->window, RevertToParent, CurrentTime);
        XChangeProperty(display, root, netatoms[NetActiveWindow], XA_WINDOW, 32, PropModeReplace, (unsigned char *)&(c->window), 1);
        
        if (c->is_floating || ws_layout[current_workspace] == MONOCLE) {
            XRaiseWindow(display, c->window);
        }

    DrawBar();
    }

void SvarkaWM::CloseWindow(Window w) {
        Atom* protocols;
        int n;
        bool supports_delete = false;
        if (XGetWMProtocols(display, w, &protocols, &n)) {
            for (int i = 0; i < n; i++) {
                if (protocols[i] == netatoms[WMDelete])
                    supports_delete = true;
            }
            XFree(protocols);
        }

        if (supports_delete) {
            XEvent ev;
            ev.type = ClientMessage;
            ev.xclient.window = w;
            ev.xclient.message_type = netatoms[WMProtocols];
            ev.xclient.format = 32;
            ev.xclient.data.l[0] = netatoms[WMDelete];
            ev.xclient.data.l[1] = CurrentTime;
            XSendEvent(display, w, False, NoEventMask, &ev);
        } else {
            XKillClient(display, w);
        }
    }

void SvarkaWM::UpdateWindowTitle(Client* c) {
        Atom actual_type;
        int actual_format;
        unsigned long nitems, bytes_after;
        unsigned char *prop = nullptr;

        c->title = "Untitled Window";
        if (XGetWindowProperty(display, c->window, netatoms[NetWMName], 0, 1024, False, netatoms[UTF8String],
                               &actual_type, &actual_format, &nitems, &bytes_after, &prop) == Success && prop) {
            if (nitems > 0) c->title = std::string((char*)prop, nitems);
            XFree(prop);
        } else {
            XTextProperty tp;
            if (XGetWMName(display, c->window, &tp) && tp.value) {
                char **list;
                int count;
                if (Xutf8TextPropertyToTextList(display, &tp, &list, &count) == Success && count > 0 && *list) {
                    c->title = *list;
                    XFreeStringList(list);
                } else {
                    c->title = (char*)tp.value;
                }
                XFree(tp.value);
            }
        }

        if (c->title == "Untitled Window" || c->title.empty()) {
            XClassHint ch;
            if (XGetClassHint(display, c->window, &ch)) {
                if (ch.res_name) c->title = ch.res_name;
                else if (ch.res_class) c->title = ch.res_class;
                if (ch.res_name) XFree(ch.res_name);
                if (ch.res_class) XFree(ch.res_class);
            }
        }
        DrawBar();
    }

void SvarkaWM::Manage(Window w, XWindowAttributes* wa) {
        if (wa->override_redirect || wa->c_class == InputOnly) return;
        for (const auto& cl : clients) if (cl->window == w) return;
        
        static const Cursor cursor = XCreateFontCursor(display, XC_left_ptr);
        XDefineCursor(display, w, cursor);
        
        auto c_unique = std::make_unique<Client>();
        Client* c = c_unique.get();
        c->window = w;
        c->workspace = current_workspace;
        c->is_floating = false;
        c->is_minimized = false;
        c->is_fullscreen = false;
        c->skip_taskbar = false;
        c->x = wa->x;
        c->y = wa->y;
        c->w = wa->width;
        c->h = wa->height;

        XSizeHints hints;
        long msize;
        if (XGetWMNormalHints(display, w, &hints, &msize) && 
            (hints.flags & PMinSize) && (hints.flags & PMaxSize) &&
            hints.min_width == hints.max_width && hints.min_height == hints.max_height && hints.min_width > 0)
            c->is_floating = true;

        Window trans;
        if (XGetTransientForHint(display, w, &trans)) c->is_floating = true;
        
        Atom actual_type;
        int actual_format;
        unsigned long nitems, bytes_after;
        unsigned char *prop = nullptr;
        bool is_menu_type = false;
        bool type_set = false;

        if (XGetWindowProperty(display, w, netatoms[NetWMWindowType], 0, 8L, False, XA_ATOM, &actual_type, &actual_format, &nitems, &bytes_after, &prop) == Success && prop) {
            Atom *types = (Atom*)prop;
            for (unsigned long i = 0; i < nitems; i++) {
                type_set = true;
                if (types[i] == XInternAtom(display, "_NET_WM_WINDOW_TYPE_MENU", False) ||
                    types[i] == XInternAtom(display, "_NET_WM_WINDOW_TYPE_DROPDOWN_MENU", False) ||
                    types[i] == XInternAtom(display, "_NET_WM_WINDOW_TYPE_POPUP_MENU", False) ||
                    types[i] == XInternAtom(display, "_NET_WM_WINDOW_TYPE_COMBO", False) ||
                    types[i] == XInternAtom(display, "_NET_WM_WINDOW_TYPE_TOOLTIP", False)) {
                    c->is_floating = true;
                    is_menu_type = true;
                }
                if (types[i] == netatoms[NetWMWindowTypeDialog] || 
                    types[i] == netatoms[NetWMWindowTypeUtility] || 
                    types[i] == netatoms[NetWMWindowTypeNotification] ||
                    types[i] == netatoms[NetWMWindowTypeSplash] ||
                    types[i] == netatoms[NetWMWindowTypeToolbar]) {
                    c->is_floating = true;
                }
            }
            XFree(prop);
        }

        if (!type_set) {
            XChangeProperty(display, w, netatoms[NetWMWindowType], XA_ATOM, 32, PropModeReplace, (unsigned char *)&netatoms[NetWMWindowTypeNormal], 1);
        }

        XClassHint ch;
        if (XGetClassHint(display, w, &ch)) {
            std::string cls = ch.res_class ? ch.res_class : "";
            if (cls == "Gnome-calculator" || cls == "vlc" || cls == "Pavucontrol") {
                c->is_floating = true;
            }
            if (ch.res_name) XFree(ch.res_name);
            if (ch.res_class) XFree(ch.res_class);
        }

        UpdateWindowTitle(c);
        UpdateClientState(c);
        
        long ws_prop = (long)c->workspace;
        XChangeProperty(display, w, XInternAtom(display, "_NET_WM_DESKTOP", False), XA_CARDINAL, 32, PropModeReplace, (unsigned char *)&ws_prop, 1);

        long extents[4] = { (long)border_width, (long)border_width, (long)border_width, (long)border_width };
        XChangeProperty(display, w, XInternAtom(display, "_NET_FRAME_EXTENTS", False), XA_CARDINAL, 32, PropModeReplace, (unsigned char *)extents, 4);

        if (c->skip_taskbar) c->is_floating = true;
        clients.push_back(std::move(c_unique));
        UpdateClientList();

        XSelectInput(display, w, EnterWindowMask | FocusChangeMask | PropertyChangeMask | StructureNotifyMask);
        XSetWindowBorder(display, w, unfocused_border_color);
        XSetWindowBorderWidth(display, w, border_width);
        
        UpdateClientOpacity(c);
        Arrange();
        if (!c->is_minimized && c->workspace == current_workspace) {
            XMapWindow(display, w);
            if (!is_menu_type) SetFocus(c);
        }
    }

void SvarkaWM::Unmanage(Window w) {
        auto it = std::find_if(clients.begin(), clients.end(), [w](const std::unique_ptr<Client>& c){ return c->window == w; });
        if (it != clients.end()) {
            Client* c = it->get();
            
            for (unsigned int i = 0; i < total_workspaces; i++) {
                if (last_focused[i] == c) last_focused[i] = nullptr;
            }

            XSetWindowBorderWidth(display, c->window, 0);

            if (current_hovered_client == c) {
                current_hovered_client = nullptr;
                ShowTooltip(nullptr, 0, 0);
            }
            if (grab_client == c) grab_client = nullptr;
            if (focused_client == c) {
                focused_client = nullptr;
                Client* next_focus = nullptr;
                for (auto it_focus = clients.rbegin(); it_focus != clients.rend(); ++it_focus) {
                    if (it_focus->get() != c && (*it_focus)->workspace == current_workspace && !(*it_focus)->is_minimized) {
                        next_focus = it_focus->get();
                        break;
                    }
                }
                SetFocus(next_focus);
            }
            clients.erase(it);
            UpdateClientList();
            Arrange();
            XFlush(display);
        }
    }
