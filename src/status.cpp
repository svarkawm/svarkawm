#include "svarkawm.h"


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
        if (!file.is_open()) {
            file.open("/sys/class/power_supply/BAT0/capacity");
            if (!file) return -1;
        }
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
                    struct tm time_info;
                    localtime_r(&now, &time_info);
                    std::strftime(time_str, sizeof(time_str), "%H:%M:%S | %d.%m.%Y", &time_info);
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
