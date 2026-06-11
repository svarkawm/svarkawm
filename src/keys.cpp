#include "svarkawm.h"


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

void SvarkaWM::HandleInternalCommand(const std::string& cmd) {
        if (cmd == "internal:quit") {
            running = false;
        } else if (cmd == "internal:close") {
            if (focused_client) CloseWindow(focused_client->window);
        } else if (cmd == "internal:promote") {
            if (focused_client) {
                auto it = std::find_if(clients.begin(), clients.end(), [this](const std::unique_ptr<Client>& c){ return c.get() == focused_client; });
                if (it != clients.end()) {
                    auto c = std::move(*it); 
                    clients.erase(it);
                    clients.insert(clients.begin(), std::move(c));
                }
                if (focused_client->is_floating) {
                    int bh = (bar_win != None) ? bar_h : 0;
                    int ty = bh + top_padding;
                    int uh = sh - ty - bottom_padding;
                    int nw = sw * 9 / 10;
                    int nh = uh * 9 / 10;
                    focused_client->w = nw;
                    focused_client->h = nh;
                    focused_client->x = (sw - nw) >> 1;
                    focused_client->y = ty + ((uh - nh) >> 1);
                }
                XRaiseWindow(display, focused_client->window);
                Arrange();
            }
        } else if (cmd == "internal:minimize") {
            if (focused_client) {
                focused_client->is_minimized = true;
                Atom state = netatoms[NetWMHidden];
                XChangeProperty(display, focused_client->window, netatoms[NetWMState], XA_ATOM, 32, PropModeReplace, (unsigned char *)&state, 1);

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
                    long ws_prop = ws;
                    XChangeProperty(display, focused_client->window, XInternAtom(display, "_NET_WM_DESKTOP", False), XA_CARDINAL, 32, PropModeReplace, (unsigned char *)&ws_prop, 1);
                    last_focused[ws] = focused_client;
                    GotoWorkspace(ws);
                }
            } catch (...) {}
        } else {
            Spawn(cmd);
        }
    }
