#include "svarkawm.h"
#include <X11/extensions/shape.h>


void SvarkaWM::SetWindowOpacity(Window w, unsigned long opacity) {
        if (w == None || netatoms[NetWMOpacity] == None) return;
        if (opacity >= 0xffffffffUL) {
            XDeleteProperty(display, w, netatoms[NetWMOpacity]);
        } else {
            XChangeProperty(display, w, netatoms[NetWMOpacity], XA_CARDINAL, 32,
                            PropModeReplace, (unsigned char *)&opacity, 1);
        }
    }

void SvarkaWM::UpdateClientOpacity(Client* c) {
        if (!c || c->window == bar_win) return;
        const unsigned long op = (c == focused_client) ? opacity_focus : opacity_unfocus;
        SetWindowOpacity(c->window, op);
    }

void SvarkaWM::UpdateAllOpacities() {
        for (const auto& c_ptr : clients) {
            Client* c = c_ptr.get();
            if (c->workspace == current_workspace && !c->is_minimized)
                UpdateClientOpacity(c);
        }
        if (bar_win != None) SetWindowOpacity(bar_win, opacity_bar);
    }

void SvarkaWM::UpdateRounding(Client* c, int w, int h) {
    if (c->round_w == w && c->round_h == h) return;
    c->round_w = w;
    c->round_h = h;
    XShapeCombineMask(display, c->window, ShapeBounding, 0, 0, None, ShapeSet);
    XShapeCombineMask(display, c->window, ShapeClip, 0, 0, None, ShapeSet);
}
