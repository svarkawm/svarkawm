#include "svarkawm.h"
#include <cmath>
#include <algorithm>


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
        int bh = (bar_win != None) ? bar_h : 0;
        int off_y = bh + top_padding;
        int usable_h = sh - off_y - bottom_padding;

        for (const auto& c_ptr : clients) {
            Client* c = c_ptr.get();
            if (c->workspace == current_workspace && !c->is_minimized) {
                visible_clients.push_back(c);
                XMapWindow(display, c->window);
                if (c->is_fullscreen && !c->is_minimized) has_fullscreen = true;
                XSetWindowBorderWidth(display, c->window, c->is_fullscreen ? 0 : border_width);
                if (!c->is_floating && !c->is_fullscreen) tiling_clients.push_back(c);
            } else {
                XUnmapWindow(display, c->window);
            }
        }

        if (has_fullscreen) {
            if (bar_win != None) XUnmapWindow(display, bar_win);
            for (auto c : visible_clients) {
                if (c->is_fullscreen) {
                    c->x = 0; c->y = 0; c->w = sw; c->h = sh;
                    XMoveResizeWindow(display, c->window, 0, 0, sw, sh);
                    UpdateRounding(c, sw, sh);
                    XRaiseWindow(display, c->window);
                }
            }
            return;
        } else {
            if (bar_win != None) XMapWindow(display, bar_win);
        }

        if (ws_layout[current_workspace] == MONOCLE) {
            for (auto c : visible_clients) {
                c->x = 0; c->y = off_y;
                c->w = sw - 2 * border_width;
                c->h = usable_h - 2 * border_width;
            }
        } else if (ws_layout[current_workspace] == FLOATING) {
            
        } else if (ws_layout[current_workspace] == TILE && !tiling_clients.empty()) {
            int n = tiling_clients.size();
            if (n == 1) {
                tiling_clients[0]->x = gappx;
                tiling_clients[0]->y = off_y + gappx;
                tiling_clients[0]->w = sw - 2 * gappx - 2 * border_width;
                tiling_clients[0]->h = usable_h - 2 * gappx - 2 * border_width;
            } else {
                int mw = static_cast<int>(sw * ws_mfact[current_workspace]);
                int mw_safe = std::max(1, mw - gappx - (gappx/2) - 2*border_width);
                int mh_safe = std::max(1, usable_h - 2*gappx - 2*border_width);
                tiling_clients[0]->x = gappx;
                tiling_clients[0]->y = off_y + gappx;
                tiling_clients[0]->w = mw_safe;
                tiling_clients[0]->h = mh_safe;
                
                int th = (usable_h - 2*gappx) / (n - 1);
                for (int i = 1; i < n; i++) {
                    int tw = sw - mw - gappx - (gappx/2) - 2*border_width;
                    int th_win = th - gappx - 2*border_width;
                    tiling_clients[i]->x = mw + (gappx/2);
                    tiling_clients[i]->y = off_y + gappx + (i-1)*th;
                    tiling_clients[i]->w = tw;
                    tiling_clients[i]->h = th_win;
                }
            }
        } else if (ws_layout[current_workspace] == GRID && !tiling_clients.empty()) {
            int n = tiling_clients.size();
            int cols = std::ceil(std::sqrt(n));
            int rows = (n + cols - 1) / cols;
            int cw = sw / std::max(1, cols);
            int rh = usable_h / rows;
            
            for (int i = 0; i < n; i++) {
                int r = i / cols;
                int c = i % cols;
                tiling_clients[i]->x = c * cw + gappx;
                tiling_clients[i]->y = off_y + r * rh + gappx;
                tiling_clients[i]->w = cw - 2 * gappx - 2 * border_width;
                tiling_clients[i]->h = rh - 2 * gappx - 2 * border_width;
            }
        }
        for (auto c : visible_clients) {
            if (!c->is_fullscreen) {
                XSetWindowBorder(display, c->window,
                    c == focused_client ? focused_border_color : unfocused_border_color);
                XSetWindowBorderWidth(display, c->window, border_width);
            }
            if (c->is_floating) {
                XRaiseWindow(display, c->window);
            }
        }

        for (auto c : visible_clients) {
            XMoveResizeWindow(display, c->window, c->x, c->y,
                              std::max(1, c->w), std::max(1, c->h));
            UpdateRounding(c, c->w, c->h);
        }

        DrawBar();
    }
