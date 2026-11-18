// Orbit Courier: window setup and the main loop. Game rules live in oc_core, the screens in App.

#include <cstdlib>
#include <memory>
#include <string>

#include <raylib.h>

#include "app.hpp"

#if defined(__EMSCRIPTEN__)
#include <emscripten/emscripten.h>
#endif

namespace {

game::Capture captureFromEnvironment() {
    game::Capture c;
    c.path = std::getenv("OC_CAPTURE");
    if (const char* level = std::getenv("OC_LEVEL")) {
        c.level = std::atoi(level);
    }
    if (const char* frames = std::getenv("OC_FRAMES")) {
        c.frames = std::atoi(frames);
    }
    if (const char* screen = std::getenv("OC_SCREEN")) {
        c.screen = screen;
    }
    return c;
}

#if defined(__EMSCRIPTEN__)
game::App* gApp = nullptr;

// The browser drives the loop: one call per animation frame.
void runFrame() { gApp->frame(); }
#endif

} // namespace

int main() {
    const game::Capture capture = captureFromEnvironment();

    SetConfigFlags(FLAG_MSAA_4X_HINT | FLAG_VSYNC_HINT);
    SetTraceLogLevel(LOG_WARNING);
    InitWindow(1280, 720, "Orbit Courier");
    SetExitKey(KEY_NULL); // Esc pauses instead of quitting
    if (capture.path == nullptr) {
        InitAudioDevice(); // silent screenshot runs
    }

    auto app = std::make_unique<game::App>(capture);
#if defined(__EMSCRIPTEN__)
    gApp = app.get();
    emscripten_set_main_loop(runFrame, 0, 1);
#else
    SetTargetFPS(144);
    while (!WindowShouldClose() && app->frame()) {
    }
#endif
    app.reset(); // unload sounds while the audio device is still open
    if (IsAudioDeviceReady()) {
        CloseAudioDevice();
    }
    CloseWindow();
    return 0;
}
