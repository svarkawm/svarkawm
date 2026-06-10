#include "svarkawm.h"

int main() {
    std::setlocale(LC_ALL, "");
    SvarkaWM wm;
    wm.Run();
    wm.Cleanup();
    return 0;
}
