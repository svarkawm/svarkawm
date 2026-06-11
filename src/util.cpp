#include "svarkawm.h"


std::string SvarkaWM::trim(const std::string& s) {
        const char* whitespace = " \t\r\n";
        const auto start = s.find_first_not_of(whitespace);
        if (start == std::string::npos) return "";
        const auto end = s.find_last_not_of(whitespace);
        return s.substr(start, end - start + 1);
    }

int SvarkaWM::OnWMDetector(Display* disp, XErrorEvent* e) {
        (void)disp;
        if (e->error_code == BadAccess) {
            std::cerr << "svarkawm: Another window manager is already running." << std::endl;
            exit(1);
        }
        return 0;
    }

int SvarkaWM::OnXError(Display* disp, XErrorEvent* e) {
        (void)disp;
        if (e->error_code == BadWindow || (e->request_code == X_SetInputFocus && e->error_code == BadMatch))
            return 0;
        std::cerr << "svarkawm: X11 Error: code=" << (int)e->error_code << " req=" << (int)e->request_code << std::endl;
        return 0;
    }

void SvarkaWM::SigChildHandler(int sig) {
        while (waitpid(-1, NULL, WNOHANG) > 0);
    }

void SvarkaWM::Spawn(const std::string& command) {
        if (fork() == 0) {
            setsid();
            if (display) close(ConnectionNumber(display));
            execl("/bin/sh", "sh", "-c", command.c_str(), nullptr);
            _exit(0);
        }
    }
