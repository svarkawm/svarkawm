#include "svarkawm.h"


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
        
        if (!show_bar) {
            bar_win = None;
            return;
        }

        if (bar_h <= 0) bar_h = 24;

        bar_win = XCreateWindow(display, root, 0, 0, sw, bar_h, 0, 
                                DefaultDepth(display, screen), CopyFromParent,
                                DefaultVisual(display, screen),
                                CWOverrideRedirect | CWBackPixel | CWEventMask, &wa);
        
        Cursor cursor = XCreateFontCursor(display, XC_left_ptr);
        XDefineCursor(display, bar_win, cursor);
        XMapRaised(display, bar_win);
        SetupBarResources();
        SetWindowOpacity(bar_win, opacity_bar);
    }

bool SvarkaWM::DrawIcon(Drawable drw, Client* c, int x, int y, int size) {
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
                        XPutImage(display, drw, DefaultGC(display, screen), img, 0, 0, x, y, size, size);
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
            XDrawRectangle(display, drw, DefaultGC(display, screen), x + 2, y + 2, size - 5, size - 5);
            XDrawLine(display, drw, DefaultGC(display, screen), x + 2, y + 5, x + size - 3, y + 5);
        }
        return true;
    }

void SvarkaWM::DrawBar() {
        if (!bar_win || !xft_font) return;

        Pixmap pm = XCreatePixmap(display, bar_win, sw, bar_h, DefaultDepth(display, screen));
        XftDraw* d = XftDrawCreate(display, pm, visual, cmap);
        
        XSetForeground(display, DefaultGC(display, screen), bar_bg_color);
        XFillRectangle(display, pm, DefaultGC(display, screen), 0, 0, sw, bar_h);

        GC gc = DefaultGC(display, screen);
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
                    XFillRectangle(display, pm, gc, x_off - 5, 2, 20, bar_h - 4);
                }
                XftColor* col = (i == current_workspace) ? &xft_focus_color : &xft_unfocus_color;
                XftDrawStringUtf8(d, col, xft_font, x_off, text_y, (FcChar8*)ws_name.c_str(), ws_name.length());
                
                if (occupied && i != current_workspace) {
                    XSetForeground(display, DefaultGC(display, screen), xft_unfocus_color.pixel);
                    XFillRectangle(display, pm, DefaultGC(display, screen), x_off, 2, 4, 2);
                }
                x_off += 25;
            }
            
            XftDrawStringUtf8(d, &xft_unfocus_color, xft_font, x_off, text_y, (FcChar8*)"|", 1);
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
                            XftDrawStringUtf8(d, &xft_unfocus_color, xft_font, x_off, text_y, (FcChar8*)"...", 3);
                        break; 
                    }

                    int start_item_x = x_off;
                    DrawIcon(pm, c, x_off, (bar_h - 16) / 2, 16); 
                    x_off += 20;
                    
                    XftColor* col = (c == focused_client) ? &xft_focus_color : &xft_unfocus_color;
                    XftDrawStringUtf8(d, col, xft_font, x_off, text_y, (FcChar8*)short_title.c_str(), short_title.length());
                    
                    bar_client_areas.emplace_back(start_item_x, 0, (x_off - start_item_x) + te.width, bar_h, c);
                    x_off += te.width + 15;
                }
            }

            if (x_off < status_start_x - 10) XftDrawStringUtf8(d, &xft_unfocus_color, xft_font, x_off, text_y, (FcChar8*)"|", 1);
            XftDrawStringUtf8(d, &xft_focus_color, xft_font, status_start_x, text_y, (FcChar8*)status_text.c_str(), status_text.length());

        XCopyArea(display, pm, bar_win, DefaultGC(display, screen), 0, 0, sw, bar_h, 0, 0);
        XftDrawDestroy(d);
        XFreePixmap(display, pm);
    }

void SvarkaWM::RedrawTooltip() {
        if (tooltip_window == None || !current_hovered_client || !xft_font) return;
        XClearWindow(display, tooltip_window);
        int tooltip_height = xft_font->height + 5;
        XftDrawStringUtf8(tooltip_draw, &xft_tooltip_color, xft_font, 5, 
                          xft_font->ascent + (tooltip_height - xft_font->height) / 2, 
                          (FcChar8*)current_hovered_client->title.c_str(), current_hovered_client->title.length());
    }
