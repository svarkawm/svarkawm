#include "svarkawm.h"

std::string SvarkaWM::trim(const std::string& s) {
        const char* whitespace = " \t\r\n";
        const auto start = s.find_first_not_of(whitespace);
        if (start == std::string::npos) return "";
        const auto end = s.find_last_not_of(whitespace);
        return s.substr(start, end - start + 1);
    }

int SvarkaWM::OnWMDetector(Display* disp, XErrorEvent* e) {
        if (e->error_code == BadAccess) {
            std::cerr << "svarkawm: Another window manager is already running." << std::endl;
            exit(1);
        }
        return 0;
    }

int SvarkaWM::OnXError(Display* disp, XErrorEvent* e) {
        if (e->error_code == BadWindow || (e->request_code == X_SetInputFocus && e->error_code == BadMatch))
            return 0;
        std::cerr << "svarkawm: X11 Error: code=" << (int)e->error_code << " req=" << (int)e->request_code << std::endl;
        return 0;
    }

int SvarkaWM::GetVolume() {
        int vol = 0;
        FILE* pipe = popen("pactl get-sink-volume @DEFAULT_SINK@", "r");
        if (pipe) {
            char buffer[256];
            if (fgets(buffer, sizeof(buffer), pipe)) {
                std::string s(buffer);
                size_t pos = s.find('%');
                if (pos != std::string::npos && pos > 3) {
                    size_t start = s.rfind(' ', pos);
                    if (start != std::string::npos)
                        try { vol = std::stoi(s.substr(start + 1, pos - start - 1)); } catch (...) {}
                }
            }
            pclose(pipe);
        }
        return vol;
    }

int SvarkaWM::GetBattery() {
        static std::ifstream file;
        if (!file.is_open()) file.open("/sys/class/power_supply/BAT0/capacity");
        file.clear();
        file.seekg(0);
        int cap = -1;
        if (file >> cap) return cap;
        return -1;
    }

void SvarkaWM::StatusUpdateLoop() {
        while (status_running) {
            std::string new_status;
            if (!custom_status_command.empty()) {
                FILE* pipe = popen(custom_status_command.c_str(), "r");
                if (pipe) {
                    char buffer[512];
                    if (fgets(buffer, sizeof(buffer), pipe)) {
                        new_status = trim(buffer);
                    }
                    pclose(pipe);
                }
            } else {
                std::string parts;
                char buf[128];

                auto add_separator = [&]() {
                    if (!parts.empty()) parts += " | ";
                };

                if (status_show_bat) {
                    const int bat = GetBattery();
                    if (bat != -1) {
                        snprintf(buf, sizeof(buf), "BAT %d%%", bat);
                        parts += buf;
                    }
                }

                if (status_show_cpu) {
                    double load[3];
                    getloadavg(load, 3);
                    add_separator();
                    snprintf(buf, sizeof(buf), "CPU %.2f", load[0]);
                    parts += buf;
                }

                if (status_show_ram) {
                    add_separator();
                    snprintf(buf, sizeof(buf), "RAM %d%%", GetRamUsage());
                    parts += buf;
                }

                if (status_show_vol) {
                    add_separator();
                    snprintf(buf, sizeof(buf), "VOL %d%%", std::min(100, GetVolume()));
                    parts += buf;
                }

                if (status_show_time) {
                    const auto now = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
                    char time_str[64] = {0};
                    std::strftime(time_str, sizeof(time_str), "%H:%M:%S | %d.%m.%Y", std::localtime(&now));
                    add_separator();
                    parts += time_str;
                }
                new_status = parts;
            }

            if (!new_status.empty())
            {
                std::lock_guard<std::mutex> lock(status_mutex);
                async_status_text = new_status;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(status_update_interval_ms));
        }
    }

void SvarkaWM::UpdateStatusText() {
        std::lock_guard<std::mutex> lock(status_mutex);
        status_text = async_status_text;
    }

void SvarkaWM::RedrawTooltip() {
        if (tooltip_window == None || !current_hovered_client || !xft_font) return;
        XClearWindow(display, tooltip_window);
        int tooltip_height = xft_font->height + 5;
        XftDrawStringUtf8(tooltip_draw, &xft_tooltip_color, xft_font, 5, 
                          xft_font->ascent + (tooltip_height - xft_font->height) / 2, 
                          (FcChar8*)current_hovered_client->title.c_str(), current_hovered_client->title.length());
    }

int SvarkaWM::GetRamUsage() {
        static std::ifstream meminfo("/proc/meminfo");
        meminfo.clear();
        meminfo.seekg(0);
        std::string line;
        long long total = 0, avail = 0;
        while (std::getline(meminfo, line) && (total == 0 || avail == 0)) {
            if (line.compare(0, 9, "MemTotal:") == 0) 
                total = std::atoll(line.c_str() + 10);
            else if (line.compare(0, 13, "MemAvailable:") == 0) 
                avail = std::atoll(line.c_str() + 14);
            if (total > 0 && avail > 0) break;
        }
        return total > 0 ? static_cast<int>(100 * (total - avail) / total) : 0;
    }

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

        XSetWindowAttributes wa;
        wa.override_redirect = True;
        wm_check_win = XCreateWindow(display, root, -1, -1, 1, 1, 0, 
                                    DefaultDepth(display, screen), InputOutput, 
                                    DefaultVisual(display, screen), CWOverrideRedirect, &wa);

        XChangeProperty(display, wm_check_win, netatoms[NetWMCheck], XA_WINDOW, 32, PropModeReplace, (unsigned char *)&wm_check_win, 1);
        XChangeProperty(display, wm_check_win, netatoms[NetWMName], netatoms[UTF8String], 8, PropModeReplace, (unsigned char *)"svarkawm", 8);
        XChangeProperty(display, root, netatoms[NetWMCheck], XA_WINDOW, 32, PropModeReplace, (unsigned char *)&wm_check_win, 1);

        XMapWindow(display, wm_check_win);
        XChangeProperty(display, root, netatoms[NetSupported], XA_ATOM, 32, PropModeReplace, (unsigned char *)netatoms, NetLast);
        XChangeProperty(display, root, netatoms[NetNumberOfDesktops], XA_CARDINAL, 32, PropModeReplace, (unsigned char *)&total_workspaces, 1);
        XDeleteProperty(display, root, netatoms[NetClientList]);
        UpdateCurrentDesktopProperty();
    }

void SvarkaWM::SetupBarResources() {
        if (bar_draw) XftDrawDestroy(bar_draw);
        if (xft_font) XftFontClose(display, xft_font);
        
        xft_font = XftFontOpenName(display, screen, bar_font_name.c_str());
        if (!xft_font) {
            xft_font = XftFontOpenName(display, screen, "monospace:size=10");
        }

        XftColorAllocName(display, visual, cmap, bar_text_color_hex.c_str(), &xft_focus_color);
        XftColorAllocName(display, visual, cmap, bar_text_inactive_color_hex.c_str(), &xft_unfocus_color);
        
        if (bar_win) bar_draw = XftDrawCreate(display, bar_win, visual, cmap);
    }

void SvarkaWM::CreateTooltipWindow() {
        XSetWindowAttributes wa;
        wa.override_redirect = True;
        wa.background_pixel = tooltip_bg_color;
        wa.border_pixel = xft_focus_color.pixel;
        wa.event_mask = ExposureMask;
        
        tooltip_window = XCreateWindow(display, root, 0, 0, 1, 1, 1,
                                DefaultDepth(display, screen), CopyFromParent,
                                DefaultVisual(display, screen),
                                CWOverrideRedirect | CWBackPixel | CWBorderPixel | CWEventMask, &wa);
        
        tooltip_draw = XftDrawCreate(display, tooltip_window, DefaultVisual(display, screen), DefaultColormap(display, screen));
        XftColorAllocName(display, DefaultVisual(display, screen), DefaultColormap(display, screen), "#ffffff", &xft_tooltip_color);
    }

void SvarkaWM::ShowTooltip(Client* c, int x_root, int y_root) {
        if (!c || !xft_font) {
            if (tooltip_window != None) {
                XUnmapWindow(display, tooltip_window);
            }
            return;
        }

        if (tooltip_window == None) {
            CreateTooltipWindow();
        }

        XGlyphInfo extents;
        XftTextExtentsUtf8(display, xft_font, (FcChar8*)c->title.c_str(), c->title.length(), &extents);

        int tooltip_width = extents.width + 10;
        int tooltip_height = xft_font->height + 5;

        int final_x = x_root + 10;
        int final_y = y_root + 10;

        if (final_x + tooltip_width > sw) final_x = sw - tooltip_width - 5;
        if (final_y + tooltip_height > sh) final_y = sh - tooltip_height - 5;
        if (final_x < 0) final_x = 0;
        if (final_y < 0) final_y = 0;

        XMoveResizeWindow(display, tooltip_window, final_x, final_y, tooltip_width, tooltip_height);
        XMapRaised(display, tooltip_window);
        RedrawTooltip();
        XFlush(display);
    }

void SvarkaWM::SetupBar() {
        XSetWindowAttributes wa;
        wa.override_redirect = True;
        wa.background_pixel = bar_bg_color;
        wa.event_mask = ExposureMask | ButtonPressMask | PointerMotionMask | LeaveWindowMask;
        
        if (bar_h <= 0) bar_h = 24;

        bar_win = XCreateWindow(display, root, 0, 0, sw, bar_h, 0, 
                                DefaultDepth(display, screen), CopyFromParent,
                                DefaultVisual(display, screen),
                                CWOverrideRedirect | CWBackPixel | CWEventMask, &wa);
        
        Cursor cursor = XCreateFontCursor(display, XC_left_ptr);
        XDefineCursor(display, bar_win, cursor);
        XMapRaised(display, bar_win);
        SetupBarResources();
    }

bool SvarkaWM::DrawIcon(Client* c, int x, int y, int size) {
        Atom actual_type;
        int actual_format;
        unsigned long nitems, bytes_after;
        unsigned char *prop = nullptr;
        bool success = false;

        if (XGetWindowProperty(display, c->window, netatoms[NetWMIcon], 0, 100000L, False, XA_CARDINAL,
                               &actual_type, &actual_format, &nitems, &bytes_after, &prop) == Success && prop) {
            unsigned long *data = (unsigned long *)prop;
            if (nitems >= 2) {
                int w = (int)data[0];
                int h = (int)data[1];
                
                if (w > 0 && h > 0 && w <= 2048 && h <= 2048 && (2UL + (unsigned long)w * h) <= nitems) {
                    icon_scratchpad.assign(size * size, 0);
                    uint32_t* dest = icon_scratchpad.data();

                    for (int dy = 0; dy < size; dy++) {
                        const int sy = dy * h / size;
                        for (int dx = 0; dx < size; dx++) {
                            const int sx = dx * w / size;
                            dest[dy * size + dx] = static_cast<uint32_t>(data[2 + sy * w + sx]);
                        }
                    }

                    void* img_data = malloc(size * size * 4);
                    std::memcpy(img_data, dest, size * size * 4);

                    XImage *img = XCreateImage(display, visual, DefaultDepth(display, screen),
                                               ZPixmap, 0, (char*)img_data, size, size, 32, 0);
                    if (img) {
                        XPutImage(display, bar_win, DefaultGC(display, screen), img, 0, 0, x, y, size, size);
                        XDestroyImage(img);
                        success = true;
                    } else {
                        std::free(img_data);
                    }
                }
            }
            XFree(prop);
        }

        if (!success) {
            const unsigned long color = (c == focused_client) ? xft_focus_color.pixel : xft_unfocus_color.pixel;
            XSetForeground(display, DefaultGC(display, screen), color);
            XDrawRectangle(display, bar_win, DefaultGC(display, screen), x + 2, y + 2, size - 5, size - 5);
            XDrawLine(display, bar_win, DefaultGC(display, screen), x + 2, y + 5, x + size - 3, y + 5);
        }
        return true;
    }

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
            for (unsigned long i = 0; i < nitems; i++) {
                if (atoms[i] == netatoms[NetWMHidden]) hidden = true;
                if (atoms[i] == netatoms[NetWMSkipTaskbar]) c->skip_taskbar = true;
                if (atoms[i] == netatoms[NetWMFullscreen]) c->is_fullscreen = true;
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

void SvarkaWM::DrawBar() {
        if (!bar_win || !xft_font) return;
        
        XClearWindow(display, bar_win);
        bar_client_areas.clear();

        XGlyphInfo status_ext;
        XftTextExtentsUtf8(display, xft_font, (FcChar8*)status_text.c_str(), status_text.length(), &status_ext);
        status_start_x = sw - status_ext.width - 10;

        int x_off = 10;
        const int text_y = (bar_h + xft_font->ascent - xft_font->descent) / 2;

            for (unsigned int i = 0; i < total_workspaces; i++) {
                bool occupied = false;
                for (const auto& c : clients) {
                    if (c->workspace == i && !c->is_minimized) { occupied = true; break; }
                }

                std::string ws_name = std::to_string(i + 1);
                if (i == current_workspace) {
                    XSetForeground(display, DefaultGC(display, screen), focused_border_color);
                    XFillRectangle(display, bar_win, DefaultGC(display, screen), x_off - 5, 2, 20, bar_h - 4);
                }

                XftColor* col = (i == current_workspace) ? &xft_focus_color : &xft_unfocus_color;
                XftDrawStringUtf8(bar_draw, col, xft_font, x_off, text_y, (FcChar8*)ws_name.c_str(), ws_name.length());
                
                if (occupied && i != current_workspace) {
                    XSetForeground(display, DefaultGC(display, screen), xft_unfocus_color.pixel);
                    XFillRectangle(display, bar_win, DefaultGC(display, screen), x_off, 2, 4, 2);
                }
                x_off += 25;
            }

            XftDrawStringUtf8(bar_draw, &xft_unfocus_color, xft_font, x_off, text_y, (FcChar8*)"|", 1);
            x_off += 15;

            for (const auto& c_ptr : clients) {
                Client* c = c_ptr.get();
                if (c->workspace == current_workspace && !c->is_minimized && !c->skip_taskbar) {
                    if (c->title.empty() || c->title == "Untitled Window") continue;

                    std::string short_title = "";
                    size_t char_count = 0;
                    for (size_t i = 0; i < c->title.length() && char_count < 8; ) {
                        unsigned char ch = (unsigned char)c->title[i];
                        size_t len = 1;
                        if (ch >= 0xf0) len = 4;
                        else if (ch >= 0xe0) len = 3;
                        else if (ch >= 0xc0) len = 2;
                        
                        if (i + len <= c->title.length()) short_title += c->title.substr(i, len);
                        i += len;
                        char_count++;
                    }
                    if (short_title.length() < c->title.length()) short_title += "...";

                    XGlyphInfo te;
                    XftTextExtentsUtf8(display, xft_font, (FcChar8*)short_title.c_str(), short_title.length(), &te);

                    int item_width = 20 + te.width + 15;
                    if (x_off + item_width > status_start_x - 5) {
                        if (x_off + 20 < status_start_x) 
                            XftDrawStringUtf8(bar_draw, &xft_unfocus_color, xft_font, x_off, text_y, (FcChar8*)"...", 3);
                        break; 
                    }

                    int start_item_x = x_off;
                    if (DrawIcon(c, x_off, (bar_h - 16) / 2, 16)) x_off += 20;
                    
                    XftColor* col = (c == focused_client) ? &xft_focus_color : &xft_unfocus_color;
                    XftDrawStringUtf8(bar_draw, col, xft_font, x_off, text_y, (FcChar8*)short_title.c_str(), short_title.length());
                    
                    bar_client_areas.emplace_back(start_item_x, 0, (x_off - start_item_x) + te.width, bar_h, c);
                    x_off += te.width + 15;
                }
            }

            if (x_off < status_start_x - 10) XftDrawStringUtf8(bar_draw, &xft_unfocus_color, xft_font, x_off, text_y, (FcChar8*)"|", 1);

            XftDrawStringUtf8(bar_draw, &xft_focus_color, xft_font, status_start_x, text_y, (FcChar8*)status_text.c_str(), status_text.length());
    }

void SvarkaWM::UpdateCurrentDesktopProperty() {
        XChangeProperty(display, root, netatoms[NetCurrentDesktop], XA_CARDINAL, 32, PropModeReplace, (unsigned char *)&current_workspace, 1);
    }

void SvarkaWM::UpdateClientList() {
        XDeleteProperty(display, root, netatoms[NetClientList]);
        for (const auto& c : clients) {
            XChangeProperty(display, root, netatoms[NetClientList], XA_WINDOW, 32, PropModeAppend, (unsigned char *)&(c->window), 1);
        }
        DrawBar();
    }

void SvarkaWM::SetFocus(Client* c) {
        if (!c) {
        if (focused_client == nullptr) return;
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
            }
        }

        // Поддержка WM_TAKE_FOCUS для корректной работы Electron/Java
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

void SvarkaWM::SigChildHandler(int sig) {
        while (waitpid(-1, NULL, WNOHANG) > 0);
    }

SvarkaWM::SvarkaWM() {
        display = XOpenDisplay(nullptr);
        signal(SIGCHLD, SigChildHandler);
        std::cerr << "svarkawm: Attempting to open X display." << std::endl;
        if (!display) {
            std::cerr << "Failed to open X display" << std::endl;
            exit(1);
        }
        xft_font = nullptr;
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
        status_update_interval_ms = 500;
        status_show_bat = true;
        status_show_cpu = true;
        status_show_ram = true;
        status_show_vol = true;
        status_show_time = true;

        running = false;
        screen = DefaultScreen(display);
        visual = DefaultVisual(display, screen);
        cmap = DefaultColormap(display, screen);
        sw = DisplayWidth(display, screen); 
        sh = DisplayHeight(display, screen);

        for (unsigned int i = 0; i < total_workspaces; i++) {
            ws_layout[i] = TILE;
            ws_mfact[i] = 0.55f;
        }
    }

SvarkaWM::~SvarkaWM() {
        status_running = false;
        if (status_thread.joinable()) status_thread.join();
        
        if (display) {
            XftColorFree(display, visual, cmap, &xft_focus_color);
            XftColorFree(display, visual, cmap, &xft_unfocus_color);
            XftColorFree(display, visual, cmap, &xft_tooltip_color);
        }

        if (xft_font) XftFontClose(display, xft_font);
        if (bar_draw) XftDrawDestroy(bar_draw);
        if (tooltip_draw) XftDrawDestroy(tooltip_draw);
        if (tooltip_window != None) XDestroyWindow(display, tooltip_window);
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

void SvarkaWM::LoadConfig() {
        const char* home = getenv("HOME");
        if (!home) return;

        fs::path config_dir = fs::path(home) / ".config" / "svarkawm";
        fs::path config_file = config_dir / "config.conf";

        if (!fs::exists(config_file)) {
            std::cout << "Generating default config at " << config_file << std::endl;
            try {
                fs::create_directories(config_dir);
                std::ofstream def(config_file);
                if (!def.is_open()) {
                    std::cerr << "Failed to open config file for writing: " << config_file << std::endl;
                    return;
                }
                def << "# svarkawm configuration file\n\n"
                    << "################################################################################\n"
                    << "# Keybindings\n"
                    << "# Syntax: bind [Modifiers] [Key] [Command]\n"
                    << "# Modifiers: Alt, Super, Shift, Control (can be combined like AltShift)\n"
                    << "# Commands: Any system command or internal WM commands (start with internal:)\n"
                    << "################################################################################\n\n"
                    << "bind Alt Return xterm             # Start terminal\n"
                    << "bind Alt q internal:close         # Close focused window\n"
                    << "bind AltShift q internal:quit     # Exit svarkawm\n"
                    << "bind AltShift r internal:reload   # Reload configuration\n"
                    << "bind Super r rofi -show drun      # Start application launcher\n\n"
                    << "# Window Focus\n"
                    << "bind Alt j internal:focus_next    # Focus next window\n"
                    << "bind Alt k internal:focus_prev    # Focus previous window\n\n"
                    << "# Layout Control\n"
                    << "bind Alt t internal:layout_tile      # Tiling layout (Master-Stack)\n"
                    << "bind Alt m internal:layout_monocle   # Monocle layout (Fullscreen-like)\n"
                    << "bind Alt g internal:layout_grid      # Grid layout\n"
                    << "bind Alt s internal:layout_floating  # Floating layout mode\n\n"
                    << "# Master Factor and Gaps\n"
                    << "bind Alt l internal:inc_mfact         # Increase master area size\n"
                    << "bind Alt h internal:dec_mfact         # Decrease master area size\n"
                    << "bind Alt bracketright internal:inc_gaps # Increase gap size\n"
                    << "bind Alt bracketleft internal:dec_gaps  # Decrease gap size\n\n"
                    << "# Window State\n"
                    << "bind Alt f internal:toggle_floating   # Toggle floating for window\n"
                    << "bind Super f internal:toggle_fullscreen # Toggle fullscreen\n"
                    << "bind Alt n internal:minimize          # Minimize (hide) window\n"
                    << "bind AltShift f internal:float_all    # Set all windows to floating\n"
                    << "bind AltShift t internal:unfloat_all  # Set all windows to tiling\n\n"
                    << "# Workspace Switching (1-9)\n"
                    << "bind Alt 1 internal:ws_1\n"
                    << "bind Alt 2 internal:ws_2\n"
                    << "bind Alt 3 internal:ws_3\n"
                    << "bind Alt 4 internal:ws_4\n"
                    << "bind Alt 5 internal:ws_5\n"
                    << "bind Alt 6 internal:ws_6\n"
                    << "bind Alt 7 internal:ws_7\n"
                    << "bind Alt 8 internal:ws_8\n"
                    << "bind Alt 9 internal:ws_9\n\n"
                    << "# Moving Windows to Workspaces\n"
                    << "bind AltShift 1 internal:move_ws_1\n"
                    << "bind AltShift 2 internal:move_ws_2\n"
                    << "bind AltShift 3 internal:move_ws_3\n"
                    << "bind AltShift 4 internal:move_ws_4\n"
                    << "bind AltShift 5 internal:move_ws_5\n"
                    << "bind AltShift 6 internal:move_ws_6\n"
                    << "bind AltShift 7 internal:move_ws_7\n"
                    << "bind AltShift 8 internal:move_ws_8\n"
                    << "bind AltShift 9 internal:move_ws_9\n\n"
                    << "################################################################################\n"
                    << "# Appearance and Behavior\n"
                    << "################################################################################\n\n"
                    << "border_width 2       # Width of the window border in pixels\n"
                    << "gap_size 5           # Gap between windows and screen edges\n"
                    << "master_factor 0.55   # Default size of the master area (0.05 to 0.95)\n\n"
                    << "# Colors (Hex format: RRGGBB, no # prefix needed for borders)\n"
                    << "color_focus 4444ff   # Border color for focused window\n"
                    << "color_unfocus 222222 # Border color for unfocused windows\n\n"
                    << "################################################################################\n"
                    << "# Status Bar Settings\n"
                    << "################################################################################\n\n"
                    << "bar_height 24                        # Height of the top bar\n"
                    << "color_bar 111111                     # Background color of the bar\n"
                    << "color_bar_text ffffff                # Color for focused/active text\n"
                    << "color_bar_text_inactive aaaaaa       # Color for inactive text\n"
                    << "bar_font JetBrains Mono:size=10:antialias=true # Xft font string\n\n"
                    << "# Status Bar Modules (1 = Enabled, 0 = Disabled)\n"
                    << "status_show_bat 1\n"
                    << "status_show_cpu 1\n"
                    << "status_show_ram 1\n"
                    << "status_show_vol 1\n"
                    << "status_show_time 1\n"
                    << "status_interval 500                  # Refresh rate in milliseconds\n\n"
                    << "# custom_status_command: If set, use output of this command instead of default modules\n"
                    << "# status_command my_status_script.sh\n\n"
                    << "################################################################################\n"
                    << "# Autostart\n"
                    << "# Commands to run when the WM starts\n"
                    << "################################################################################\n\n"
                    << "autostart xsetroot -solid '#222222'  # Set wallpaper color\n"
                    << "autostart xterm                      # Launch terminal\n";
                def.close();
            } catch (const std::exception& e) {
                std::cerr << "Failed to create default config: " << e.what() << std::endl;
            }
        }
        std::ifstream file(config_file);
        if (!file.is_open()) {
            std::cerr << "Could not open config file." << std::endl;
            return;
        }

        std::string line;
        while (std::getline(file, line)) {
            size_t comment_pos = line.find('#');
            if (comment_pos != std::string::npos) line = line.substr(0, comment_pos);
            if (trim(line).empty()) continue;

            std::stringstream ss(line);
            std::string type;
            ss >> type;

            if (type == "bind") {
                std::string mod_str, key_str, cmd_part;
                ss >> mod_str >> key_str;
                std::string cmd;
                std::getline(ss, cmd);
                size_t first = cmd.find_first_not_of(" \t");
                if (first != std::string::npos) cmd = cmd.substr(first);

                unsigned int mod = 0;
                if (mod_str.find("Alt") != std::string::npos) mod |= Mod1Mask;
                if (mod_str.find("Shift") != std::string::npos) mod |= ShiftMask;
                if (mod_str.find("Control") != std::string::npos) mod |= ControlMask;
                if (mod_str.find("Super") != std::string::npos) mod |= Mod4Mask;

                KeySym sym = XStringToKeysym(key_str.c_str());
                cmd = trim(cmd);
                if (sym != NoSymbol && !cmd.empty()) {
                    keybinds[{mod, sym}] = cmd;
                }
            } else if (type == "color_focus") {
                std::string color; ss >> color;
                while (!color.empty() && (color[0] == '#' || color[0] == ' ')) color.erase(0, 1);
                try { focused_border_color = std::stoul(color, nullptr, 16); } 
                catch (...) { focused_border_color = 0x4444ff; }
            } else if (type == "color_unfocus") {
                std::string color; ss >> color;
                while (!color.empty() && (color[0] == '#' || color[0] == ' ')) color.erase(0, 1);
                try { unfocused_border_color = std::stoul(color, nullptr, 16); }
                catch (...) { unfocused_border_color = 0x222222; }
            } else if (type == "color_bar") {
                std::string color; ss >> color;
                while (!color.empty() && (color[0] == '#' || color[0] == ' ')) color.erase(0, 1);
                try { bar_bg_color = std::stoul(color, nullptr, 16); }
                catch (...) { bar_bg_color = 0x111111; }
            } else if (type == "color_bar_text") {
                ss >> bar_text_color_hex;
                if (bar_text_color_hex[0] != '#') bar_text_color_hex = "#" + bar_text_color_hex;
            } else if (type == "color_bar_text_inactive") {
                ss >> bar_text_inactive_color_hex;
                if (bar_text_inactive_color_hex[0] != '#') bar_text_inactive_color_hex = "#" + bar_text_inactive_color_hex;
            } else if (type == "status_show_bat") {
                ss >> status_show_bat;
            } else if (type == "status_show_cpu") {
                ss >> status_show_cpu;
            } else if (type == "status_show_ram") {
                ss >> status_show_ram;
            } else if (type == "status_show_vol") {
                ss >> status_show_vol;
            } else if (type == "status_show_time") {
                ss >> status_show_time;
            } else if (type == "status_command") {
                std::string cmd;
                std::getline(ss, cmd);
                custom_status_command = trim(cmd);
            } else if (type == "bar_height") {
                ss >> bar_h;
            } else if (type == "bar_font") {
                std::string font;
                std::getline(ss, font);
                bar_font_name = trim(font);
            } else if (type == "status_interval") {
                ss >> status_update_interval_ms;
            } else if (type == "border_width") {
                ss >> border_width;
            } else if (type == "gap_size") {
                ss >> gappx;
            } else if (type == "master_factor") {
                float new_mfact_default;
                ss >> new_mfact_default;
                for (unsigned int i = 0; i < total_workspaces; ++i) {
                    ws_mfact[i] = new_mfact_default;
                }
            } else if (type == "exec" || type == "autostart") {
                std::string cmd;
                std::getline(ss, cmd);
                cmd = trim(cmd);
                if (!cmd.empty()) autostart.push_back(cmd);
            } 
        }
    }

void SvarkaWM::ReloadConfig() {
        if (display) {
            XftColorFree(display, visual, cmap, &xft_focus_color);
            XftColorFree(display, visual, cmap, &xft_unfocus_color);
        }

        keybinds.clear();
        autostart.clear();
        LoadConfig();
        GrabKeys();
        SetupBarResources();
        Arrange();
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

void SvarkaWM::HandleInternalCommand(const std::string& cmd) {
        if (cmd == "internal:quit") {
            running = false;
        } else if (cmd == "internal:close") {
            if (focused_client) CloseWindow(focused_client->window);
        } else if (cmd == "internal:promote") {
            if (focused_client) {
                auto it = std::find_if(clients.begin(), clients.end(), [this](const std::unique_ptr<Client>& c){ return c.get() == focused_client; });
                if (it != clients.end()) {
                    std::unique_ptr<Client> c = std::move(*it); // Create a temporary unique_ptr
                    clients.erase(it);
                    clients.insert(clients.begin(), std::move(c)); // Insert at the beginning
                }
                if (focused_client->is_floating) {
                    focused_client->w = sw * 9 / 10;
                    focused_client->h = (sh - bar_h) * 9 / 10;
                    focused_client->x = (sw - focused_client->w) >> 1;
                    focused_client->y = bar_h + ((sh - bar_h - focused_client->h) >> 1);
                    XMoveResizeWindow(display, focused_client->window, focused_client->x, focused_client->y, 
                                     focused_client->w - 2*border_width, focused_client->h - 2*border_width);
                }
                XRaiseWindow(display, focused_client->window);
                Arrange();
            }
        } else if (cmd == "internal:minimize") {
            if (focused_client) {
                focused_client->is_minimized = true;
                Client* next = nullptr;
                for(const auto& cl : clients) if(cl->workspace == current_workspace && !cl->is_minimized) { next = cl.get(); break; }
                SetFocus(next);
                Arrange();
            }
        } else if (cmd == "internal:layout_monocle") {
            ws_layout[current_workspace] = MONOCLE;
            Arrange();
        } else if (cmd == "internal:layout_tile") {
            ws_layout[current_workspace] = TILE;
            Arrange();
        } else if (cmd == "internal:layout_grid") {
            ws_layout[current_workspace] = GRID;
            Arrange();
        } else if (cmd == "internal:layout_floating") {
            ws_layout[current_workspace] = FLOATING;
            Arrange();
        } else if (cmd == "internal:inc_mfact") {
            ws_mfact[current_workspace] = std::min(0.95f, ws_mfact[current_workspace] + 0.05f); Arrange();
        } else if (cmd == "internal:float_all") {
            for (const auto& c : clients) if (c->workspace == current_workspace) c->is_floating = true;
            Arrange();
        } else if (cmd == "internal:unfloat_all") {
            for (const auto& c : clients) if (c->workspace == current_workspace) c->is_floating = false;
            Arrange();
        } else if (cmd == "internal:toggle_floating") {
            if (focused_client) {
                focused_client->is_floating = !focused_client->is_floating;
                Arrange();
            }
        } else if (cmd == "internal:toggle_fullscreen") {
            if (focused_client) {
                focused_client->is_fullscreen = !focused_client->is_fullscreen;
                if (focused_client->is_fullscreen) {
                    Atom fs = netatoms[NetWMFullscreen];
                    XChangeProperty(display, focused_client->window, netatoms[NetWMState], XA_ATOM, 32, PropModeReplace, (unsigned char *)&fs, 1);
                } else {
                    XDeleteProperty(display, focused_client->window, netatoms[NetWMState]);
                }
                Arrange();
            }
        } else if (cmd == "internal:dec_mfact") {
            ws_mfact[current_workspace] = std::max(0.05f, ws_mfact[current_workspace] - 0.05f); Arrange();
        } else if (cmd == "internal:inc_gaps") {
            gappx += 2; Arrange();
        } else if (cmd == "internal:dec_gaps") {
            if (gappx > 0) {
                gappx -= 2;
            }
            Arrange();
        } else if (cmd == "internal:focus_next") {
            std::vector<Client*> ws_clients;
            for(const auto& c : clients) if(c->workspace == current_workspace) ws_clients.push_back(c.get());
            if (ws_clients.empty()) return;
            auto it = std::find(ws_clients.begin(), ws_clients.end(), focused_client);
            if (it != ws_clients.end()) {
                auto next = (std::next(it) == ws_clients.end()) ? ws_clients.begin() : std::next(it);
                SetFocus(*next);
            }
        } else if (cmd == "internal:focus_prev") {
            std::vector<Client*> ws_clients;
            for(const auto& c : clients) if(c->workspace == current_workspace) ws_clients.push_back(c.get());
            if (ws_clients.empty()) return;
            auto it = std::find(ws_clients.begin(), ws_clients.end(), focused_client);
            if (it != ws_clients.end()) {
                auto prev = (it == ws_clients.begin()) ? std::prev(ws_clients.end()) : std::prev(it);
                SetFocus(*prev);
            }
        } else if (cmd == "internal:reload") {
            ReloadConfig();
        } else if (cmd.find("internal:ws_") == 0) {
            std::string ws_num = cmd.substr(12);
            if (ws_num.empty()) return;
            try {
                unsigned int ws = std::stoi(ws_num) - 1;
                GotoWorkspace(ws);
            } catch (...) {}
        } else if (cmd.find("internal:move_ws_") == 0) {
            std::string ws_num = cmd.substr(17);
            if (ws_num.empty() || !focused_client) return;
            try {
                unsigned int ws = std::stoi(ws_num) - 1;
                if (ws < total_workspaces && ws != focused_client->workspace) {
                    focused_client->workspace = ws;
                    last_focused[ws] = focused_client;
                    GotoWorkspace(ws);
                }
            } catch (...) {}
        } else {
            Spawn(cmd);
        }
    }

void SvarkaWM::Spawn(const std::string& command) {
        if (fork() == 0) {
            setsid();
            if (display) close(ConnectionNumber(display));
            execl("/bin/sh", "sh", "-c", command.c_str(), nullptr);
            _exit(0);
        }
    }

void SvarkaWM::GotoWorkspace(unsigned int ws) {
        if (ws >= total_workspaces || ws == current_workspace) return;
        current_workspace = ws;
        UpdateCurrentDesktopProperty();
        SetFocus(last_focused[current_workspace]);
        Arrange();
    }

void SvarkaWM::Arrange() {
        std::vector<Client*> visible_clients; 
        std::vector<Client*> tiling_clients;
        visible_clients.reserve(clients.size());
        tiling_clients.reserve(clients.size());
        
        bool has_fullscreen = false;

        for (const auto& c_ptr : clients) {
            Client* c = c_ptr.get();
            if (c->workspace == current_workspace && !c->is_minimized) {
                visible_clients.push_back(c);
                XMapWindow(display, c->window);
                if (c->is_fullscreen && !c->is_minimized) has_fullscreen = true;
                XSetWindowBorderWidth(display, c->window, c->is_fullscreen ? 0 : border_width);
                if (!c->is_floating && !c->is_fullscreen) tiling_clients.push_back(c);
            } else {
                XMoveWindow(display, c->window, -sw * 2, -sh * 2);
            }
        }

        if (has_fullscreen) {
            XUnmapWindow(display, bar_win);
            for (auto c : visible_clients) {
                if (c->is_fullscreen) {
                    XMoveResizeWindow(display, c->window, 0, 0, sw, sh);
                    XRaiseWindow(display, c->window);
                }
            }
            return;
        } else {
            XMapWindow(display, bar_win);
        }

        if (ws_layout[current_workspace] == MONOCLE) {
            for (auto c : visible_clients) {
                XMoveResizeWindow(display, c->window, 0, bar_h, sw - 2 * border_width, sh - bar_h - 2 * border_width);
            }
        } else if (ws_layout[current_workspace] == FLOATING) {
            for (auto c : visible_clients) {
                XMoveResizeWindow(display, c->window, c->x, c->y, c->w - 2 * border_width, c->h - 2 * border_width);
            }
        } else if (ws_layout[current_workspace] == TILE && !tiling_clients.empty()) {
            int n = tiling_clients.size();
            if (n == 1) {
                int w = sw - 2 * gappx - 2 * border_width;
                int h = sh - bar_h - 2 * gappx - 2 * border_width;
                XMoveResizeWindow(display, tiling_clients[0]->window, gappx, bar_h + gappx, 
                                  std::max(1, w), std::max(1, h));
            } else {
                int mw = static_cast<int>(sw * ws_mfact[current_workspace]);
                int mw_safe = std::max(1, mw - gappx - (gappx/2) - 2*border_width);
                int mh_safe = std::max(1, sh - bar_h - 2*gappx - 2*border_width);
                XMoveResizeWindow(display, tiling_clients[0]->window, gappx, bar_h + gappx,
                                  mw_safe, mh_safe);
                
                int th = (sh - bar_h - 2*gappx) / (n - 1);
                for (int i = 1; i < n; i++) {
                    int tw = sw - mw - gappx - (gappx/2) - 2*border_width;
                    int th_win = th - gappx - 2*border_width;
                    XMoveResizeWindow(display, tiling_clients[i]->window, mw + (gappx/2), bar_h + gappx + (i-1)*th,
                                      std::max(1, tw), std::max(1, th_win));
                }
            }
        } else if (ws_layout[current_workspace] == GRID && !tiling_clients.empty()) {
            int n = tiling_clients.size();
            int cols = std::ceil(std::sqrt(n));
            int rows = (n + cols - 1) / cols;
            int cw = sw / std::max(1, cols);
            int rh = (sh - bar_h) / rows;
            
            for (int i = 0; i < n; i++) {
                int r = i / cols;
                int c = i % cols;
                int w = cw - 2*gappx - 2*border_width;
                int h = rh - 2*gappx - 2*border_width;
                XMoveResizeWindow(display, tiling_clients[i]->window, 
                                  c * cw + gappx, bar_h + r * rh + gappx,
                                  std::max(1, w), std::max(1, h));
            }
        }
        for (auto c : visible_clients) {
            if (c->is_floating) {
                XMoveResizeWindow(display, c->window, c->x, c->y, c->w - 2 * border_width, c->h - 2 * border_width);
                XRaiseWindow(display, c->window);
            }
        }
        DrawBar();
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

void SvarkaWM::GrabKeys() {
        XUngrabKey(display, AnyKey, AnyModifier, root);
        static const unsigned int modifiers[] = { 0, LockMask, Mod2Mask, LockMask | Mod2Mask };
        for (auto const& [key, cmd] : keybinds) {
            const KeyCode code = XKeysymToKeycode(display, key.second);
            for (unsigned int m : modifiers) {
                XGrabKey(display, code, key.first | m, root, True, GrabModeAsync, GrabModeAsync);
            }
        }
        XGrabButton(display, AnyButton, Mod1Mask, root, True, ButtonPressMask | PointerMotionMask | ButtonReleaseMask, GrabModeAsync, GrabModeAsync, None, None);
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
        bool is_normal_type = false;
        if (XGetWindowProperty(display, w, netatoms[NetWMWindowType], 0, 8L, False, XA_ATOM, &actual_type, &actual_format, &nitems, &bytes_after, &prop) == Success && prop) {
            Atom *types = (Atom*)prop;
            for (unsigned long i = 0; i < nitems; i++) {
                if (types[i] == netatoms[NetWMWindowTypeNormal]) is_normal_type = true;
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
        
        if (!is_normal_type && !c->is_floating) {
            if (wa->width < 160 || wa->height < 160) c->is_floating = true;
        }
        
        UpdateWindowTitle(c);
        UpdateClientState(c);
        
        if (c->skip_taskbar) c->is_floating = true;

        clients.push_back(std::move(c_unique));
        UpdateClientList();

        XSelectInput(display, w, EnterWindowMask | FocusChangeMask | PropertyChangeMask | StructureNotifyMask);
        XSetWindowBorder(display, w, unfocused_border_color);
        XSetWindowBorderWidth(display, w, border_width);
        
        if (c->is_minimized || c->workspace != current_workspace) {
            XMoveWindow(display, w, -sw * 2, -sh * 2);
        } else {
            XMapWindow(display, w);
            SetFocus(c);
        }
        Arrange();
    }

void SvarkaWM::Unmanage(Window w) {
        auto it = std::find_if(clients.begin(), clients.end(), [w](const std::unique_ptr<Client>& c){ return c->window == w; });
        if (it != clients.end()) {
            Client* c = it->get();
            
            for (int i = 0; i < 9; i++) {
                if (last_focused[i] == c) last_focused[i] = nullptr;
            }

            if (current_hovered_client == c) {
                current_hovered_client = nullptr;
                ShowTooltip(nullptr, 0, 0);
            }
            if (grab_client == c) grab_client = nullptr;
            if (focused_client == c) {
                focused_client = nullptr;
                Client* next_focus = nullptr;
                // Ищем последнего кандидата на фокус в текущем воркспейсе
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
        }
    }

void SvarkaWM::Run() {
        running = true;
        LoadConfig();
        status_running = true;
        status_thread = std::thread(&SvarkaWM::StatusUpdateLoop, this);

        XSetErrorHandler(OnWMDetector);
        SetupBar();
        XSelectInput(display, root, SubstructureRedirectMask | SubstructureNotifyMask | KeyPressMask | ButtonPressMask | ButtonReleaseMask | StructureNotifyMask);
        
        static const Cursor cursor = XCreateFontCursor(display, XC_left_ptr);
        XDefineCursor(display, root, cursor);
        
        SetupAtoms();
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
                        int dx = ev.xmotion.x_root - start_mouse_x;
                        int dy = ev.xmotion.y_root - start_mouse_y;
                        if (ev.xmotion.state & Button1Mask) {
                            if (!grab_client->is_floating) grab_client->is_floating = true;
                            grab_client->x = start_win_x + dx;
                            grab_client->y = start_win_y + dy;
                            XMoveWindow(display, grab_client->window, grab_client->x, grab_client->y);
                        } else if (ev.xmotion.state & Button3Mask) {
                            if (!grab_client->is_floating) grab_client->is_floating = true;
                            grab_client->w = std::max(10, start_win_w + dx);
                            grab_client->h = std::max(10, start_win_h + dy);
                            XResizeWindow(display, grab_client->window, grab_client->w, grab_client->h);
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

            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
    }

void SvarkaWM::Cleanup() {
        for (const auto& c : clients) {
            XUnmapWindow(display, c->window);
        }
        clients.clear();
        if (bar_win != None) XDestroyWindow(display, bar_win);
        if (wm_check_win != None) XDestroyWindow(display, wm_check_win);
        if (tooltip_window != None) {
            if (tooltip_draw) XftDrawDestroy(tooltip_draw);
            XDestroyWindow(display, tooltip_window);
        }
        XUngrabKey(display, AnyKey, AnyModifier, root);
        XSync(display, False);
    }
