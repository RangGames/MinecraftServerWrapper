#include "Application.h"

int WINAPI WinMain(HINSTANCE instance, HINSTANCE, LPSTR, int show) {
    return wrapper::runApplication(instance, show);
}
