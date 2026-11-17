// Orbit Courier: rendering, input and the main loop. All game rules live in oc_core.

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <string>
#include <vector>

#include <raylib.h>

#include "oc/levels.hpp"
#include "oc/world.hpp"

#if defined(__EMSCRIPTEN__)
#include <emscripten/emscripten.h>
#endif

namespace {

constexpr int kWidth = 1280;
constexpr int kHeight = 720;

constexpr Color kSpace{7, 11, 20, 255};
constexpr Color kPlanet{56, 104, 160, 255};
constexpr Color kMoon{150, 150, 165, 255};
constexpr Color kShip{240, 240, 245, 255};
constexpr Color kAccent{255, 186, 73, 255};
constexpr Color kPath{120, 220, 255, 255};
constexpr Color kDanger{255, 92, 92, 255};
constexpr Color kText{220, 226, 240, 255};

Vector2 toScreen(oc::Vec2 v) { return {static_cast<float>(v.x), static_cast<float>(v.y)}; }

struct Star {
    float x, y, depth, size;
};

/// Small deterministic generator, so the star field is the same on every start.
std::uint32_t nextRandom(std::uint32_t& state) {
    state ^= state << 13;
    state ^= state >> 17;
    state ^= state << 5;
    return state;
}

std::vector<Star> makeStars(int count) {
    std::uint32_t seed = 0x9E3779B9u;
    auto unit = [&] { return static_cast<float>(nextRandom(seed) % 10'000) / 10'000.0f; };
    std::vector<Star> stars;
    for (int i = 0; i < count; ++i) {
        stars.push_back({unit() * kWidth, unit() * kHeight, 0.05f + unit() * 0.25f, 0.6f + unit() * 1.4f});
    }
    return stars;
}

struct Game {
    oc::World world = oc::makeSandbox();
    Camera2D camera{};
    double accumulator = 0.0;
    float zoom = 1.0f;
    float crashTimer = 0.0f;
    std::vector<Star> stars = makeStars(260);

    Game() {
        camera.offset = {kWidth / 2.0f, kHeight / 2.0f};
        camera.target = toScreen(world.ship().position);
        camera.zoom = zoom;
    }

    void restart() {
        world = oc::makeSandbox();
        crashTimer = 0.0f;
    }

    // Capture mode (OC_CAPTURE=file.png): a scripted flight, then a screenshot. Used for README images.
    const char* capturePath = std::getenv("OC_CAPTURE");
    int frameCount = 0;

    oc::Input readInput() const {
        if (capturePath != nullptr) {
            return {.turn = frameCount > 40 && frameCount < 62 ? -1 : 0,
                    .thrust = frameCount > 62 && frameCount < 130};
        }
        oc::Input input;
        if (IsKeyDown(KEY_LEFT) || IsKeyDown(KEY_A)) {
            input.turn -= 1;
        }
        if (IsKeyDown(KEY_RIGHT) || IsKeyDown(KEY_D)) {
            input.turn += 1;
        }
        input.thrust = IsKeyDown(KEY_UP) || IsKeyDown(KEY_W) || IsKeyDown(KEY_SPACE);
        return input;
    }

    void update(float frameTime) {
        if (IsKeyPressed(KEY_R)) {
            restart();
        }
        zoom = std::clamp(zoom * std::pow(1.15f, GetMouseWheelMove()), 0.25f, 3.0f);

        // Fixed time step: the simulation runs at 120 Hz whatever the frame rate is.
        accumulator += std::min(static_cast<double>(frameTime), 0.25);
        const oc::Input input = readInput();
        while (accumulator >= oc::World::kStep) {
            world.step(input);
            accumulator -= oc::World::kStep;
        }
        if (world.status() == oc::Status::Crashed) {
            crashTimer += frameTime;
        }

        const Vector2 target = toScreen(world.ship().position);
        const float follow = 1.0f - std::exp(-6.0f * frameTime);
        camera.target.x += (target.x - camera.target.x) * follow;
        camera.target.y += (target.y - camera.target.y) * follow;
        camera.zoom += (zoom - camera.zoom) * follow;
    }

    void drawStars() const {
        for (const Star& s : stars) {
            // Parallax: distant stars move slower than the camera.
            const float x = std::fmod(s.x - camera.target.x * s.depth, static_cast<float>(kWidth));
            const float y = std::fmod(s.y - camera.target.y * s.depth, static_cast<float>(kHeight));
            const auto brightness = static_cast<unsigned char>(90 + 600 * s.depth);
            DrawCircleV({x < 0 ? x + kWidth : x, y < 0 ? y + kHeight : y}, s.size,
                        Color{brightness, brightness, 255, 255});
        }
    }

    void drawBodies() const {
        const auto& bodies = world.bodies();
        for (std::size_t i = 0; i < bodies.size(); ++i) {
            const oc::Body& b = bodies[i];
            const Vector2 pos = toScreen(world.bodyPosition(i, world.time()));
            const auto radius = static_cast<float>(b.radius);
            if (b.parent) {
                const auto orbit = static_cast<float>(b.orbitRadius);
                const float half = 0.75f / camera.zoom;
                DrawRing(toScreen(world.bodyPosition(*b.parent, world.time())), orbit - half, orbit + half, 0.0f,
                         360.0f, 256, Fade(kText, 0.14f));
            }
            const Color base = b.parent ? kMoon : kPlanet;
            const Color shadow = ColorBrightness(base, -0.55f);
            const Color light = ColorBrightness(base, 0.35f);
            DrawCircleGradient(pos, radius * 1.35f, Fade(base, 0.22f), Fade(base, 0.0f)); // atmosphere
            DrawCircleV(pos, radius, shadow);
            // Lit hemisphere: the light comes from the upper left.
            DrawCircleGradient({pos.x - radius * 0.2f, pos.y - radius * 0.2f}, radius * 0.8f, light, Fade(base, 0.0f));
        }
    }

    void drawTrajectory() const {
        if (world.status() != oc::Status::Flying) {
            return;
        }
        const auto points = world.predictTrajectory(240, 6);
        const float thickness = 2.0f / camera.zoom;
        Vector2 previous = toScreen(world.ship().position);
        for (std::size_t i = 0; i < points.size(); ++i) {
            const Vector2 p = toScreen(points[i]);
            const float fade = 1.0f - static_cast<float>(i) / static_cast<float>(points.size());
            DrawLineEx(previous, p, thickness, Fade(kPath, 0.55f * fade));
            previous = p;
        }
        if (points.size() < 240 && !points.empty()) {
            DrawCircleV(toScreen(points.back()), 5.0f / camera.zoom, kDanger); // predicted impact
        }
    }

    void drawShip() const {
        const oc::Ship& s = world.ship();
        const Vector2 p = toScreen(s.position);
        if (world.status() == oc::Status::Crashed) {
            const float t = std::min(crashTimer, 1.2f);
            DrawCircleV(p, 6.0f + 60.0f * t, Fade(kAccent, std::max(0.0f, 0.6f - t * 0.5f)));
            DrawCircleLinesV(p, 10.0f + 90.0f * t, Fade(kDanger, std::max(0.0f, 1.0f - t)));
            return;
        }

        const oc::Vec2 fwd = oc::Vec2::fromAngle(s.angle);
        const oc::Vec2 side{-fwd.y, fwd.x};
        const Vector2 nose = toScreen(s.position + fwd * 16.0);
        const Vector2 left = toScreen(s.position - fwd * 10.0 + side * 10.0);
        const Vector2 right = toScreen(s.position - fwd * 10.0 - side * 10.0);

        if (world.thrusting()) {
            const double flicker = 10.0 + 6.0 * std::sin(GetTime() * 60.0);
            const Vector2 tail = toScreen(s.position - fwd * (10.0 + flicker * 1.4));
            const Vector2 l = toScreen(s.position - fwd * 8.5 + side * 5.0);
            const Vector2 r = toScreen(s.position - fwd * 8.5 - side * 5.0);
            DrawTriangle(l, tail, r, kAccent);
            DrawTriangle(l, r, tail, kAccent);
        }
        // Draw both windings: raylib culls one of them depending on orientation.
        DrawTriangle(nose, left, right, kShip);
        DrawTriangle(nose, right, left, kShip);
    }

    /// Altitude above the surface of the nearest body, and that body's name.
    std::pair<double, std::string> nearestBody() const {
        double best = 1e18;
        std::string name;
        const auto& bodies = world.bodies();
        for (std::size_t i = 0; i < bodies.size(); ++i) {
            const double alt = (world.bodyPosition(i, world.time()) - world.ship().position).length() - bodies[i].radius;
            if (alt < best) {
                best = alt;
                name = bodies[i].name;
            }
        }
        return {best, name};
    }

    void drawHud() const {
        const oc::Ship& s = world.ship();
        DrawText("ORBIT COURIER", 24, 20, 22, kText);
        DrawText("sandbox", 24, 46, 16, Fade(kText, 0.5f));

        // Fuel gauge
        DrawText("FUEL", 24, kHeight - 58, 14, Fade(kText, 0.7f));
        DrawRectangle(24, kHeight - 38, 200, 12, Fade(kText, 0.15f));
        DrawRectangle(24, kHeight - 38, static_cast<int>(200 * s.fuel), 12, s.fuel > 0.2 ? kAccent : kDanger);

        const auto [altitude, body] = nearestBody();
        const std::string speed = TextFormat("SPEED  %5.1f", s.velocity.length());
        const std::string alt = TextFormat("ALT    %5.0f  (%s)", altitude, body.c_str());
        DrawText(speed.c_str(), 250, kHeight - 58, 16, kText);
        DrawText(alt.c_str(), 250, kHeight - 36, 16, kText);

        const char* help = "A/D or arrows: turn   W/up/space: thrust   wheel: zoom   R: restart";
        DrawText(help, kWidth - MeasureText(help, 14) - 24, kHeight - 30, 14, Fade(kText, 0.5f));

        if (world.status() == oc::Status::Crashed) {
            const std::string msg = "Crashed into " + world.bodies()[*world.crashedInto()].name;
            DrawText(msg.c_str(), (kWidth - MeasureText(msg.c_str(), 32)) / 2, kHeight / 2 - 60, 32, kDanger);
            const char* hint = "Press R to try again";
            DrawText(hint, (kWidth - MeasureText(hint, 18)) / 2, kHeight / 2 - 20, 18, kText);
        }
    }

    /// Returns false when the game should quit.
    bool frame() {
        ++frameCount;
        update(capturePath != nullptr ? 1.0f / 60.0f : GetFrameTime());
        BeginDrawing();
        ClearBackground(kSpace);
        drawStars();
        BeginMode2D(camera);
        drawBodies();
        drawTrajectory();
        drawShip();
        EndMode2D();
        drawHud();
        EndDrawing();

        if (capturePath != nullptr && frameCount == 150) {
            TakeScreenshot(capturePath);
            return false;
        }
        return true;
    }
};

#if defined(__EMSCRIPTEN__)
Game* gGame = nullptr;

// The browser drives the loop: one call per animation frame.
void runFrame() { gGame->frame(); }
#endif

} // namespace

int main() {
    SetConfigFlags(FLAG_MSAA_4X_HINT | FLAG_VSYNC_HINT);
    InitWindow(kWidth, kHeight, "Orbit Courier");
    SetExitKey(KEY_NULL);

    Game game;
#if defined(__EMSCRIPTEN__)
    gGame = &game;
    emscripten_set_main_loop(runFrame, 0, 1);
#else
    SetTargetFPS(144);
    while (!WindowShouldClose() && game.frame()) {
    }
#endif
    CloseWindow();
    return 0;
}
