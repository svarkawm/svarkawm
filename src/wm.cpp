#include "svarkawm.h"
#include <cstring>
#include <csignal>
#include <sys/wait.h>
#include <algorithm>


SvarkaWM::SvarkaWM() {
        display = XOpenDisplay(nullptr);
        signal(SIGCHLD, SigChildHandler);
        std::cerr << "svarkawm: Attempting to open X display." << std::endl;
        if (!display) {
            std::cerr << "Failed to open X display" << std::endl;
            exit(1);
        }
        current_workspace = 0;
        bar_win = None;
        wm_check_win = None;
        tooltip_window = None;
        xft_font = nullptr;
        bar_draw = nullptr;
        tooltip_draw = nullptr;
        focused_client = nullptr;
        grab_client = nullptr;
        current_hovered_client = nullptr;
        status_running = false;
        std::memset(&xft_focus_color, 0, sizeof(XftColor));
        std::memset(&xft_unfocus_color, 0, sizeof(XftColor));
        std::memset(&xft_tooltip_color, 0, sizeof(XftColor));
        root = DefaultRootWindow(display);
        focused_border_color = 0x4444ff;
        unfocused_border_color = 0x222222;
        bar_bg_color = 0x111111;
        border_width = 2;
        gappx = 5;
        bar_h = 24;
        bar_font_name = "JetBrains Mono:size=10:antialias=true";
        bar_text_color_hex = "#ffffff";
        bar_text_inactive_color_hex = "#aaaaaa";
        status_start_x = 0;
        status_update_interval_ms = 500;
        status_show_bat = true;
        status_show_cpu = true;
        status_show_ram = true;
        status_show_vol = true;
        status_show_time = true;
        top_padding = 0;
        bottom_padding = 0;
        show_bar = true;
        opacity_focus = 0xffffffffUL;
        opacity_unfocus = 0xffffffffUL;
        opacity_bar = 0xffffffffUL;

        running = false;
        screen = DefaultScreen(display);
        visual = DefaultVisual(display, screen);
        cmap = DefaultColormap(display, screen);
        sw = DisplayWidth(display, screen); 
        sh = DisplayHeight(display, screen);

        for (unsigned int i = 0; i < total_workspaces; i++) {
            ws_layout[i] = TILE;
            ws_mfact[i] = 0.55f;
            last_focused[i] = nullptr;
        }
    }

SvarkaWM::~SvarkaWM() {
        status_running = false;
        if (status_thread.joinable()) status_thread.join();
        
        if (display && visual && cmap) {
            if (xft_focus_color.pixel)
            XftColorFree(display, visual, cmap, &xft_focus_color);
            if (xft_unfocus_color.pixel)
            XftColorFree(display, visual, cmap, &xft_unfocus_color);
        }

        if (xft_font) XftFontClose(display, xft_font);
        if (bar_draw) XftDrawDestroy(bar_draw);
        if (tooltip_window != None) {
            XftColorFree(display, visual, cmap, &xft_tooltip_color);
            if (tooltip_draw) XftDrawDestroy(tooltip_draw);
            XDestroyWindow(display, tooltip_window);
        }
        XCloseDisplay(display);
    }

void SvarkaWM::Scan() {
        unsigned int num;
        Window d1, d2, *wins = nullptr;
        XWindowAttributes wa;
        if (XQueryTree(display, root, &d1, &d2, &wins, &num)) {
            for (unsigned int i = 0; i < num; i++) {
                if (XGetWindowAttributes(display, wins[i], &wa) && !wa.override_redirect && 
                    wa.map_state == IsViewable && wa.c_class != InputOnly)
                    Manage(wins[i], &wa);
            }
            if (wins) XFree(wins);
        }
    }

void SvarkaWM::Run() {
        running = true;
        LoadConfig();
        status_running = true;
        status_thread = std::thread(&SvarkaWM::StatusUpdateLoop, this);

        XSetErrorHandler(OnWMDetector);
        XSelectInput(display, root, SubstructureRedirectMask | SubstructureNotifyMask | KeyPressMask | ButtonPressMask | ButtonReleaseMask | StructureNotifyMask);
        
        static const Cursor cursor = XCreateFontCursor(display, XC_left_ptr);
        XDefineCursor(display, root, cursor);
        
        SetupAtoms();
        SetupBar();
        GrabKeys();
        Scan();
        DrawBar();
        XSync(display, False);
        XSetErrorHandler(OnXError);

        for (const auto& cmd : autostart) Spawn(cmd);
        
        XEvent ev;
        XWindowAttributes wa;
        auto last_draw = std::chrono::steady_clock::now();

        while (running) {
            while (XEventsQueued(display, QueuedAfterReading) > 0) {
                XNextEvent(display, &ev);
                if (ev.type == KeyPress) {
                    const KeySym sym = XLookupKeysym(&ev.xkey, 0);
                    const auto it = keybinds.find({ev.xkey.state & ~(LockMask | Mod2Mask), sym});
                    if (it != keybinds.end()) HandleInternalCommand(it->second);
                } 
                else if (ev.type == MapRequest) {
                    auto it = std::find_if(clients.begin(), clients.end(), [&](const std::unique_ptr<Client>& c){ return c->window == ev.xmaprequest.window; });
                    if (it != clients.end()) {
                        (*it)->is_minimized = false;
                        
                        // Удаляем HIDDEN при разворачивании
                        XDeleteProperty(display, it->get()->window, netatoms[NetWMState]);
                        
                        XMapWindow(display, it->get()->window);
                        SetFocus(it->get());
                        Arrange();
                    } else if (XGetWindowAttributes(display, ev.xmaprequest.window, &wa)) {
                        Manage(ev.xmaprequest.window, &wa);
                    }
                } 
                else if (ev.type == UnmapNotify) {
                    auto it = std::find_if(clients.begin(), clients.end(), [&](const std::unique_ptr<Client>& c){ return c->window == ev.xunmap.window; });
                    if (it != clients.end()) {
                        Client* c = it->get();
                        // Помечаем как свернутое только если оно на текущем воркспейсе
                        if (c->workspace == current_workspace) {
                            c->is_minimized = true;
                            if (focused_client == c) {
                                focused_client = nullptr;
                                Client* next_focus = nullptr;
                                for (auto it_focus = clients.rbegin(); it_focus != clients.rend(); ++it_focus) {
                                    if (it_focus->get() != c && (*it_focus)->workspace == current_workspace && !(*it_focus)->is_minimized) {
                                        next_focus = it_focus->get(); break;
                                    }
                                }
                                SetFocus(next_focus);
                            }
                            Arrange();
                        }
                    }
                }
                else if (ev.type == DestroyNotify) {
                    Unmanage(ev.xdestroywindow.window);
                } 
                else if (ev.type == ConfigureRequest) {
                    XConfigureRequestEvent *cre = &ev.xconfigurerequest;
                    auto it = std::find_if(clients.begin(), clients.end(), [&](const std::unique_ptr<Client>& c){ return c->window == cre->window; });
                    
                    if (it != clients.end()) {
                        Client* c = (*it).get();
                        if (c->is_floating) {
                            if (c->workspace == current_workspace) {
                                if (cre->value_mask & CWX) c->x = cre->x;
                                if (cre->value_mask & CWY) c->y = cre->y;
                            }
                            if (cre->value_mask & CWWidth) c->w = cre->width;
                            if (cre->value_mask & CWHeight) c->h = cre->height;
                        }
                        XConfigureEvent ce;
                        ce.type = ConfigureNotify;
                        ce.display = display;
                        ce.event = c->window;
                        ce.window = c->window;
                        ce.x = c->x; ce.y = c->y;
                        ce.width = c->w; ce.height = c->h;
                        ce.border_width = border_width;
                        ce.above = None;
                        ce.override_redirect = False;
                        XSendEvent(display, c->window, False, StructureNotifyMask, (XEvent *)&ce);
                    }
                    
                    XWindowChanges wc;
                    wc.x = cre->x; wc.y = cre->y;
                    wc.width = cre->width; wc.height = cre->height;
                    wc.border_width = cre->border_width;
                    wc.sibling = cre->above;
                    wc.stack_mode = cre->detail;
                    XConfigureWindow(display, cre->window, cre->value_mask, &wc);
                    
                    if (it != clients.end()) Arrange();
                }
                else if (ev.type == ConfigureNotify) {
                    if (ev.xconfigure.window == root) {
                        sw = ev.xconfigure.width;
                        sh = ev.xconfigure.height;
                        if (bar_win != None) XMoveResizeWindow(display, bar_win, 0, 0, sw, bar_h);
                        SetupBarResources();
                        Arrange();
                    }
                }
                else if (ev.type == PropertyNotify) {
                    auto it = std::find_if(clients.begin(), clients.end(), [&](const std::unique_ptr<Client>& c){ return c->window == ev.xproperty.window; });
                    if (it != clients.end()) {
                    if (ev.xproperty.atom == netatoms[NetWMName] || ev.xproperty.atom == XA_WM_NAME) {
                        UpdateWindowTitle(it->get());
                    } else {
                        UpdateClientState(it->get());
                        Arrange();
                    }
                    }
                    else if (ev.xproperty.window == root) DrawBar();
                }
                else if (ev.type == ClientMessage) {
                    if (ev.xclient.message_type == netatoms[NetWMState]) {

                        long action = ev.xclient.data.l[0];
                        Atom prop1 = ev.xclient.data.l[1];
                        
                        auto it = std::find_if(clients.begin(), clients.end(), [&](const std::unique_ptr<Client>& c){ return c->window == ev.xclient.window; });
                        if (it != clients.end()) {
                            Client* c = it->get();
                            bool changed = false;

                            Atom actual_type;
                            int actual_format;
                            unsigned long nitems, bytes_after;
                            unsigned char *prop_data = nullptr;
                            std::vector<Atom> current_states;

                            if (XGetWindowProperty(display, c->window, netatoms[NetWMState], 0, 1024, False, XA_ATOM,
                                                   &actual_type, &actual_format, &nitems, &bytes_after, &prop_data) == Success && prop_data) {
                                for (unsigned long i = 0; i < nitems; ++i) {
                                    current_states.push_back(((Atom*)prop_data)[i]);
                                }
                                XFree(prop_data);
                            }

                            if (prop1 == netatoms[NetWMFullscreen]) {
                                auto fs_it = std::find(current_states.begin(), current_states.end(), netatoms[NetWMFullscreen]);
                                bool is_currently_fullscreen = (fs_it != current_states.end());

                                if (action == _NET_WM_STATE_ADD && !is_currently_fullscreen) {
                                    current_states.push_back(netatoms[NetWMFullscreen]); changed = true;
                                } else if (action == _NET_WM_STATE_REMOVE && is_currently_fullscreen) {
                                    current_states.erase(fs_it); changed = true;
                                } else if (action == _NET_WM_STATE_TOGGLE) {
                                    if (is_currently_fullscreen) current_states.erase(fs_it);
                                    else current_states.push_back(netatoms[NetWMFullscreen]);
                                    changed = true;
                                }
                            }

                            if (changed) {
                                if (current_states.empty()) {
                                    XDeleteProperty(display, c->window, netatoms[NetWMState]);
                                } else {
                                    XChangeProperty(display, c->window, netatoms[NetWMState], XA_ATOM, 32, PropModeReplace, (unsigned char *)current_states.data(), current_states.size());
                                }
                            }
                        }
                    } else if (ev.xclient.message_type == netatoms[NetActiveWindow]) {
                        auto it = std::find_if(clients.begin(), clients.end(), [&](const std::unique_ptr<Client>& c){ return c->window == ev.xclient.window; });
                        if (it != clients.end()) {
                            if ((*it)->is_minimized) (*it)->is_minimized = false;
                            GotoWorkspace(it->get()->workspace);
                            SetFocus(it->get());
                        }
                    }
                }
                else if (ev.type == EnterNotify) {
                    if (ev.xcrossing.mode != NotifyNormal || ev.xcrossing.detail == NotifyInferior) continue;
                    auto it = std::find_if(clients.begin(), clients.end(), [&](const std::unique_ptr<Client>& c){ return c->window == ev.xcrossing.window; });
                    if (it != clients.end() && it->get() != focused_client) SetFocus(it->get());
                }
                else if (ev.type == ButtonPress) {
                    if (ev.xbutton.window == bar_win) {
                        if (ev.xbutton.button == Button1) {
                            const int ws = (ev.xbutton.x - 5) / 25;
                            if (ws >= 0 && ws < (int)total_workspaces) GotoWorkspace(ws);
                        } else if (ev.xbutton.button == Button4 && ev.xbutton.x >= status_start_x) {
                            int cv = GetVolume();
                            if (cv < 100) {
                                int nv = std::min(100, cv + 5);
                                Spawn("pactl set-sink-volume @DEFAULT_SINK@ " + std::to_string(nv) + "%");
                            }
                        } else if (ev.xbutton.button == Button5 && ev.xbutton.x >= status_start_x) {
                            int cv = GetVolume();
                            int nv = std::max(0, cv - 5);
                            Spawn("pactl set-sink-volume @DEFAULT_SINK@ " + std::to_string(nv) + "%");
                        }
                        XFlush(display);
                    } else if (ev.xbutton.subwindow != None) {
                        auto it = std::find_if(clients.begin(), clients.end(), [&](const std::unique_ptr<Client>& c){ return c->window == ev.xbutton.subwindow; });
                        if (it != clients.end()) {
                            grab_client = it->get();
                            SetFocus(it->get());
                            start_mouse_x = ev.xbutton.x_root; start_mouse_y = ev.xbutton.y_root;
                            start_win_x = grab_client->x; start_win_y = grab_client->y;
                            start_win_w = grab_client->w; start_win_h = grab_client->h;
                        }
                    }
                }
                else if (ev.type == MotionNotify) {
                    if (ev.xmotion.window == bar_win) {
                        Client* new_hovered_client = nullptr;
                        for (const auto& area : bar_client_areas) {
                            if (ev.xmotion.x >= std::get<0>(area) && ev.xmotion.x < std::get<0>(area) + std::get<2>(area)) {
                                new_hovered_client = std::get<4>(area); break;
                            }
                        }
                        if (new_hovered_client != current_hovered_client) {
                            current_hovered_client = new_hovered_client;
                            ShowTooltip(current_hovered_client, ev.xmotion.x_root, ev.xmotion.y_root);
                        }
                        if (!new_hovered_client) {
                             ShowTooltip(nullptr, 0, 0);
                        }
                    } else if (grab_client) {
                        if (ev.xmotion.state & Button1Mask) {
                            int dx = ev.xmotion.x_root - start_mouse_x;
                            int dy = ev.xmotion.y_root - start_mouse_y;
                            if (!grab_client->is_floating) grab_client->is_floating = true;
                            grab_client->x = start_win_x + dx;
                            grab_client->y = start_win_y + dy;
                        } else if (ev.xmotion.state & Button3Mask) {
                            int dx = ev.xmotion.x_root - start_mouse_x;
                            int dy = ev.xmotion.y_root - start_mouse_y;
                            if (!grab_client->is_floating) grab_client->is_floating = true;
                            grab_client->w = std::max(10, start_win_w + dx);
                            grab_client->h = std::max(10, start_win_h + dy);
                        }
                    }
                }
                else if (ev.type == LeaveNotify) {
                    current_hovered_client = nullptr;
                    ShowTooltip(nullptr, 0, 0);
                }
                else if (ev.type == ButtonRelease) {
                    grab_client = nullptr;
                    Arrange();
                }
                else if (ev.type == Expose) {
                    if (ev.xexpose.window == bar_win) DrawBar();
                    else if (ev.xexpose.window == tooltip_window) RedrawTooltip();
                }
            }
            const auto now = std::chrono::steady_clock::now();
            const auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - last_draw).count();

            if (elapsed > status_update_interval_ms) {
                UpdateStatusText();
                DrawBar();
                XFlush(display);
                last_draw = now;
            }

            if (XPending(display) == 0) {
                std::this_thread::sleep_for(std::chrono::milliseconds(16));
            }
        }
    }

void SvarkaWM::Cleanup() {
        for (const auto& c : clients) {
            XUnmapWindow(display, c->window);
        }
        status_running = false;
        if (status_thread.joinable()) status_thread.join();
        clients.clear();
        if (bar_draw) { XftDrawDestroy(bar_draw); bar_draw = nullptr; }
        if (display && bar_win != None) XDestroyWindow(display, bar_win);
        if (wm_check_win != None) XDestroyWindow(display, wm_check_win);
        if (tooltip_window != None) {
            if (tooltip_draw) XftDrawDestroy(tooltip_draw);
            XftColorFree(display, visual, cmap, &xft_tooltip_color);
            XDestroyWindow(display, tooltip_window);
        }
        XUngrabKey(display, AnyKey, AnyModifier, root);
        XSync(display, False);
    }
