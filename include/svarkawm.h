#ifndef SVARKAWM_H
#define SVARKAWM_H

#include <cstring>
#include <X11/Xlib.h>
#include <X11/keysym.h>
#include <X11/Xutil.h>
#include <X11/cursorfont.h>
#include <X11/Xatom.h>
#include <X11/Xproto.h>
#include <X11/Xft/Xft.h>
#include <iostream>
#include <vector>
#include <algorithm>
#include <list>
#include <fstream>
#include <sstream>
#include <map>
#include <unistd.h>
#include <tuple>
#include <utility>
#include <memory>
#include <sys/wait.h>
#include <signal.h>
#include <wordexp.h>
#include <filesystem>
#include <chrono>
#include <iomanip>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <thread>
#include <mutex>
#include <atomic>
#include <clocale>

namespace fs = std::filesystem;

#define _NET_WM_STATE_REMOVE        0
#define _NET_WM_STATE_ADD           1
#define _NET_WM_STATE_TOGGLE        2

enum { NetSupported, NetWMName, NetWMState, NetWMCheck, NetActiveWindow, NetWMWindowType,
       NetWMWindowTypeDialog, NetWMWindowTypeNormal, NetClientList, NetNumberOfDesktops, 
       NetCurrentDesktop, NetWMIcon, NetWMFullscreen, NetWMSkipTaskbar, NetWMHidden, NetWMWindowTypeSplash,
       NetWMWindowTypeToolbar, NetWMWindowTypeMenu, NetWMWindowTypeDropdownMenu, NetWMWindowTypePopupMenu,
       NetWMWindowTypeTooltip, NetWMWindowTypeCombo, NetWMWindowTypeDND,
       WMProtocols, WMDelete, WMTakeFocus, WMState, UTF8String, NetWMWindowTypeUtility, NetWMWindowTypeNotification, NetWMOpacity, NetLast };

struct Client {
    Window window;
    int x, y, w, h;
    int old_w, old_h;
    int round_w = -1, round_h = -1;
    bool is_floating;
    bool is_urgent;
    bool is_minimized;
    bool is_fullscreen;
    bool skip_taskbar;
    unsigned int workspace;
    std::string title;
};

enum LayoutMode { TILE, MONOCLE, GRID, FLOATING };

class SvarkaWM {
private:
    Display* display;
    Window root = None;
    Window bar_win = None;
    int screen;
    Visual* visual;
    Colormap cmap;
    int sw, sh;
    int bar_h = 24, top_padding = 0, bottom_padding = 0;
    
    std::vector<std::unique_ptr<Client>> clients;
    Client* focused_client = nullptr;
    Client* grab_client = nullptr;
    Client* last_focused[9] = {nullptr};
    Window wm_check_win = None;
    
    int start_mouse_x, start_mouse_y, start_win_x, start_win_y, start_win_w, start_win_h;
    std::map<std::pair<unsigned int, KeySym>, std::string> keybinds;
    std::vector<std::string> autostart;
    std::string status_text;
    std::string custom_status_command;
    std::string async_status_text;
    std::string bar_font_name;
    bool status_show_bat, status_show_cpu, status_show_ram, status_show_vol, status_show_time;
    std::string bar_text_color_hex, bar_text_inactive_color_hex;
    std::mutex status_mutex;
    std::thread status_thread;
    std::atomic<bool> status_running{false};
    int status_start_x = 0;
    int status_update_interval_ms = 500;
    
    unsigned long focused_border_color, unfocused_border_color, bar_bg_color;
    int border_width, gappx;
    unsigned long opacity_focus, opacity_unfocus, opacity_bar;
    float ws_mfact[9];
    unsigned int current_workspace = 0;
    const unsigned int total_workspaces = 9;
    LayoutMode ws_layout[9];
    bool running, show_bar = true;
    Atom netatoms[NetLast];
    XftDraw* bar_draw = nullptr;
    XftFont* xft_font = nullptr;
    std::vector<uint32_t> icon_scratchpad;
    XftColor xft_focus_color, xft_unfocus_color;
    Window tooltip_window = None;
    XftDraw* tooltip_draw = nullptr;
    XftColor xft_tooltip_color;
    unsigned long tooltip_bg_color = 0x333333;
    std::vector<std::tuple<int, int, int, int, Client*>> bar_client_areas;
    Client* current_hovered_client = nullptr;

    std::string trim(const std::string& s);
    static int OnWMDetector(Display* disp, XErrorEvent* e);
    static int OnXError(Display* disp, XErrorEvent* e);
    int GetVolume();
    int GetBattery();
    void StatusUpdateLoop();
    void UpdateStatusText();
    void RedrawTooltip();
    int GetRamUsage();
    void SetupAtoms();
    void SetupBarResources();
    void CreateTooltipWindow();
    void ShowTooltip(Client* c, int x_root, int y_root);
    void SetupBar();
    bool DrawIcon(Drawable drw, Client* c, int x, int y, int size);
    void UpdateRounding(Client* c, int w, int h);
    void SetWindowOpacity(Window w, unsigned long opacity);
    void UpdateClientOpacity(Client* c);
    void UpdateAllOpacities();
    void UpdateClientState(Client* c);
    void DrawBar();
    void UpdateCurrentDesktopProperty();
    void UpdateClientList();
    void SetFocus(Client* c);
    static void SigChildHandler(int sig);
    void CloseWindow(Window w);
    void HandleInternalCommand(const std::string& cmd);
    void Spawn(const std::string& command);
    void GotoWorkspace(unsigned int ws);
    void Arrange();
    void UpdateWindowTitle(Client* c);
    void GrabKeys();
    void Manage(Window w, XWindowAttributes* wa);
    void Unmanage(Window w);

public:
    SvarkaWM();
    ~SvarkaWM();
    void Scan();
    void LoadConfig();
    void ReloadConfig();
    void Run();
    void Cleanup();
};

#endif
