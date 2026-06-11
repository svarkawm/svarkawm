#include "svarkawm.h"
#include <fstream>
#include <sstream>
#include <algorithm>


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
                    << "# Window opacity (0.0 transparent — 1.0 opaque)\n"
                    << "# For transparency effects, a compositor like picom pijulius is required.\n"
                    << "opacity_focus 1.0\n"
                    << "opacity_unfocus 1.0\n"
                    << "opacity_bar 1.0\n\n"
                    << "# Window animations are handled by external compositor (e.g. picom pijulius).\n\n"
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
                    << "show_bar 1                            # 1 to show status bar, 0 to hide\n"
                    << "top_padding 0                         # Extra space at top (for external bars)\n"
                    << "bottom_padding 0                      # Extra space at bottom\n"
                    << "status_interval 500                  # Refresh rate in milliseconds\n\n"
                    << "# custom_status_command: If set, use output of this command instead of default modules\n"
                    << "# status_command my_status_script.sh\n\n"
                    << "################################################################################\n"
                    << "# Autostart\n"
                    << "# Commands to run when the WM starts\n"
                    << "################################################################################\n\n"
                    << "# autostart feh --bg-scale /path/to/wallpaper.jpg\n"
                    << "autostart xsetroot -solid '#1a1a2e'  # Fallback background color\n"
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
            } else if (type == "show_bar") {
                ss >> show_bar;
            } else if (type == "top_padding") {
                ss >> top_padding;
            } else if (type == "bottom_padding") {
                ss >> bottom_padding;
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
            } else if (type == "opacity_focus" || type == "opacity_unfocus" || type == "opacity_bar") {
                float val = 1.0f;
                ss >> val;
                val = std::clamp(val, 0.0f, 1.0f);
                const unsigned long op = static_cast<unsigned long>(val * 4294967295.0f);
                if (type == "opacity_focus") opacity_focus = op;
                else if (type == "opacity_unfocus") opacity_unfocus = op;
                else opacity_bar = op;
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
            if (xft_focus_color.pixel)
            XftColorFree(display, visual, cmap, &xft_focus_color);
            if (xft_unfocus_color.pixel)
            XftColorFree(display, visual, cmap, &xft_unfocus_color);
            std::memset(&xft_focus_color, 0, sizeof(XftColor));
            std::memset(&xft_unfocus_color, 0, sizeof(XftColor));

            if (bar_draw) { XftDrawDestroy(bar_draw); bar_draw = nullptr; }
            if (bar_win != None) {
                XDestroyWindow(display, bar_win);
                bar_win = None;
            }
        }

        keybinds.clear();
        autostart.clear();
        LoadConfig();
        GrabKeys();
        SetupBar();
        Arrange();
        UpdateAllOpacities();
    }
