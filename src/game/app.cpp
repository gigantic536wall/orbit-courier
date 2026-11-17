#include "app.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <string>

namespace game {

namespace {

constexpr int kWidth = 1280;
constexpr int kHeight = 720;

constexpr Color kSpace{7, 11, 20, 255};
constexpr Color kPlanet{56, 104, 160, 255};
constexpr Color kMoon{150, 150, 165, 255};
constexpr Color kShip{240, 240, 245, 255};
constexpr Color kFlame{255, 186, 73, 255};
constexpr Color kPickup{255, 186, 73, 255};
constexpr Color kDropoff{110, 231, 140, 255};
constexpr Color kPath{120, 220, 255, 255};
constexpr Color kDanger{255, 92, 92, 255};
constexpr Color kText{220, 226, 240, 255};
constexpr Color kMuted{140, 150, 170, 255};

Vector2 v2(oc::Vec2 v) { return {static_cast<float>(v.x), static_cast<float>(v.y)}; }
float f(double d) { return static_cast<float>(d); }

void textCentered(const char* text, int y, int size, Color color) {
    DrawText(text, (kWidth - MeasureText(text, size)) / 2, y, size, color);
}

/// Splits `text` into lines no wider than `width` pixels.
std::vector<std::string> wrap(const std::string& text, int size, int width) {
    std::vector<std::string> lines;
    std::string line;
    std::size_t start = 0;
    while (start < text.size()) {
        std::size_t end = text.find(' ', start);
        if (end == std::string::npos) {
            end = text.size();
        }
        const std::string word = text.substr(start, end - start);
        const std::string candidate = line.empty() ? word : line + " " + word;
        if (!line.empty() && MeasureText(candidate.c_str(), size) > width) {
            lines.push_back(line);
            line = word;
        } else {
            line = candidate;
        }
        start = end + 1;
    }
    if (!line.empty()) {
        lines.push_back(line);
    }
    return lines;
}

void drawStar(Vector2 c, float r, Color color) {
    DrawPoly(c, 5, r * 0.45f, -90.0f + 36.0f, color);
    for (int i = 0; i < 5; ++i) {
        const float a = (-90.0f + 72.0f * static_cast<float>(i)) * DEG2RAD;
        const float l = a - 36.0f * DEG2RAD;
        const float rr = a + 36.0f * DEG2RAD;
        const Vector2 tip{c.x + std::cos(a) * r, c.y + std::sin(a) * r};
        const Vector2 left{c.x + std::cos(l) * r * 0.42f, c.y + std::sin(l) * r * 0.42f};
        const Vector2 right{c.x + std::cos(rr) * r * 0.42f, c.y + std::sin(rr) * r * 0.42f};
        DrawTriangle(tip, left, right, color);
        DrawTriangle(tip, right, left, color);
    }
}

void drawStars(Vector2 c, int earned, float r, float gap) {
    for (int i = 0; i < 3; ++i) {
        const Vector2 p{c.x + (static_cast<float>(i) - 1.0f) * gap, c.y};
        drawStar(p, r, i < earned ? kFlame : Fade(kMuted, 0.35f));
    }
}

std::uint32_t nextRandom(std::uint32_t& state) {
    state ^= state << 13;
    state ^= state >> 17;
    state ^= state << 5;
    return state;
}

std::string seconds(double s) { return TextFormat("%.1f s", s); }

} // namespace

App::App(Capture capture)
    : capture_(capture), best_(levels_.size()),
      levelIndex_(std::min<std::size_t>(static_cast<std::size_t>(std::max(capture.level, 0)), levels_.size() - 1)),
      mission_(levels_[levelIndex_]) {
    std::uint32_t seed = 0x9E3779B9u;
    auto unit = [&] { return static_cast<float>(nextRandom(seed) % 10'000) / 10'000.0f; };
    for (int i = 0; i < 260; ++i) {
        stars_.push_back({unit() * kWidth, unit() * kHeight, 0.05f + unit() * 0.25f, 0.6f + unit() * 1.4f});
    }
    camera_.offset = {kWidth / 2.0f, kHeight / 2.0f};
    startLevel(levelIndex_);
    if (capture_.path != nullptr && !capture_.briefing) {
        screen_ = Screen::Playing;
        overview_ = false;
    }
}

void App::startLevel(std::size_t index) {
    levelIndex_ = index;
    mission_ = oc::Mission(levels_[index]);
    screen_ = Screen::Briefing;
    overview_ = true;
    accumulator_ = 0.0;
    endTimer_ = 0.0f;
    approach_.reset();
    camera_.target = {0.0f, 0.0f};
    camera_.zoom = overviewZoom();
}

float App::overviewZoom() const {
    const oc::World& w = mission_.world();
    double extent = 200.0;
    for (std::size_t i = 0; i < w.bodies().size(); ++i) {
        extent = std::max(extent, w.bodyPosition(i, 0.0).length() + w.bodies()[i].radius);
    }
    for (std::size_t i = 0; i < w.stations().size(); ++i) {
        extent = std::max(extent, w.bodyPosition(w.stations()[i].parent, 0.0).length() + w.stations()[i].orbitRadius);
    }
    return std::clamp(f(320.0 / (extent * 1.08)), 0.15f, 1.2f);
}

std::optional<std::size_t> App::target() const {
    if (const auto stop = mission_.currentStop()) {
        return stop->station;
    }
    return std::nullopt;
}

oc::Input App::flightInput() const {
    if (capture_.path != nullptr) {
        return {.turn = frameCount_ > 40 && frameCount_ < 62 ? -1 : 0, .thrust = frameCount_ > 62 && frameCount_ < 100};
    }
    oc::Input input;
    if (IsKeyDown(KEY_LEFT) || IsKeyDown(KEY_A)) {
        input.turn -= 1;
    }
    if (IsKeyDown(KEY_RIGHT) || IsKeyDown(KEY_D)) {
        input.turn += 1;
    }
    input.thrust = IsKeyDown(KEY_UP) || IsKeyDown(KEY_W) || IsKeyDown(KEY_SPACE);
    input.fine = IsKeyDown(KEY_LEFT_SHIFT) || IsKeyDown(KEY_RIGHT_SHIFT);
    return input;
}

void App::handleInput() {
    const bool confirm = IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_KP_ENTER) || IsKeyPressed(KEY_SPACE);
    auto restart = [&] {
        startLevel(levelIndex_);
        screen_ = Screen::Playing;
        overview_ = false;
    };

    if (IsKeyPressed(KEY_RIGHT_BRACKET) && levelIndex_ + 1 < levels_.size()) {
        startLevel(levelIndex_ + 1);
        return;
    }
    if (IsKeyPressed(KEY_LEFT_BRACKET) && levelIndex_ > 0) {
        startLevel(levelIndex_ - 1);
        return;
    }

    switch (screen_) {
    case Screen::Briefing:
        if (confirm) {
            screen_ = Screen::Playing;
            overview_ = false;
        }
        break;
    case Screen::Playing:
        if (IsKeyPressed(KEY_P) || IsKeyPressed(KEY_ESCAPE)) {
            screen_ = Screen::Paused;
        } else if (IsKeyPressed(KEY_R)) {
            restart();
        } else if (IsKeyPressed(KEY_TAB) || IsKeyPressed(KEY_M)) {
            overview_ = !overview_;
        }
        break;
    case Screen::Paused:
        if (IsKeyPressed(KEY_P) || IsKeyPressed(KEY_ESCAPE) || confirm) {
            screen_ = Screen::Playing;
        } else if (IsKeyPressed(KEY_R)) {
            restart();
        }
        break;
    case Screen::Result:
        if (mission_.state() == oc::MissionState::Complete && confirm) {
            startLevel((levelIndex_ + 1) % levels_.size());
        } else if (IsKeyPressed(KEY_R) || confirm) {
            restart();
        }
        break;
    }

    if (!overview_) {
        zoom_ = std::clamp(zoom_ * std::pow(1.15f, GetMouseWheelMove()), 0.2f, 3.0f);
    }
}

void App::simulate(float frameTime) {
    if (screen_ != Screen::Playing) {
        return;
    }
    // Fixed time step: the simulation runs at 120 Hz whatever the frame rate is.
    accumulator_ += std::min(static_cast<double>(frameTime), 0.25);
    const oc::Input input = flightInput();
    while (accumulator_ >= oc::World::kStep) {
        mission_.step(input);
        accumulator_ -= oc::World::kStep;
    }

    if (const auto t = target(); t && mission_.state() == oc::MissionState::Flying) {
        approach_ = mission_.world().closestApproach(*t, 90.0);
    } else {
        approach_.reset();
    }

    if (mission_.state() != oc::MissionState::Flying) {
        endTimer_ += frameTime;
        if (endTimer_ > 1.2f) {
            screen_ = Screen::Result;
            if (mission_.state() == oc::MissionState::Complete) {
                const oc::Result r = mission_.result();
                auto& best = best_[levelIndex_];
                if (!best || r.score > best->score) {
                    best = r;
                }
            }
        }
    }
}

void App::updateCamera(float frameTime) {
    Vector2 target{0.0f, 0.0f};
    float zoom = overviewZoom();
    if (!overview_) {
        target = v2(mission_.world().ship().position);
        zoom = zoom_;
    }
    const float follow = 1.0f - std::exp(-5.0f * frameTime);
    camera_.target.x += (target.x - camera_.target.x) * follow;
    camera_.target.y += (target.y - camera_.target.y) * follow;
    camera_.zoom += (zoom - camera_.zoom) * follow;
}

bool App::frame() {
    ++frameCount_;
    const float dt = capture_.path != nullptr ? 1.0f / 60.0f : GetFrameTime();
    handleInput();
    simulate(dt);
    updateCamera(dt);

    BeginDrawing();
    ClearBackground(kSpace);
    drawBackground();
    BeginMode2D(camera_);
    drawWorld();
    EndMode2D();
    drawLabels();
    drawTargetArrow();
    drawHud();
    drawToast();
    if (screen_ == Screen::Briefing) {
        drawBriefing();
    } else if (screen_ == Screen::Result) {
        drawResult();
    } else if (screen_ == Screen::Paused) {
        drawPaused();
    }
    EndDrawing();

    if (capture_.path != nullptr && frameCount_ >= capture_.frames) {
        TakeScreenshot(capture_.path);
        return false;
    }
    return true;
}

// ---------------------------------------------------------------------------------------------
// World rendering

void App::drawBackground() const {
    for (const Star& s : stars_) {
        // Parallax: distant stars move slower than the camera.
        const float x = std::fmod(s.x - camera_.target.x * s.depth, static_cast<float>(kWidth));
        const float y = std::fmod(s.y - camera_.target.y * s.depth, static_cast<float>(kHeight));
        const auto brightness = static_cast<unsigned char>(90.0f + 600.0f * s.depth);
        DrawCircleV({x < 0 ? x + kWidth : x, y < 0 ? y + kHeight : y}, s.size, Color{brightness, brightness, 255, 255});
    }
}

void App::drawWorld() const {
    drawBodies();
    drawStations();
    drawTrajectory();
    drawApproach();
    drawShip();
}

void App::drawBodies() const {
    const oc::World& w = mission_.world();
    const auto& bodies = w.bodies();
    for (std::size_t i = 0; i < bodies.size(); ++i) {
        const oc::Body& b = bodies[i];
        const Vector2 pos = v2(w.bodyPosition(i, w.time()));
        const float radius = f(b.radius);
        if (b.parent) {
            const float orbit = f(b.orbitRadius);
            const float half = 0.75f / camera_.zoom;
            DrawRing(v2(w.bodyPosition(*b.parent, w.time())), orbit - half, orbit + half, 0.0f, 360.0f, 256,
                     Fade(kText, 0.10f));
        }
        const Color base = b.parent ? kMoon : kPlanet;
        DrawCircleGradient(pos, radius * 1.35f, Fade(base, 0.22f), Fade(base, 0.0f)); // atmosphere
        DrawCircleV(pos, radius, ColorBrightness(base, -0.45f));
        // Lit hemisphere: the light comes from the upper left.
        DrawCircleGradient({pos.x - radius * 0.2f, pos.y - radius * 0.2f}, radius * 0.8f, ColorBrightness(base, 0.35f),
                           Fade(base, 0.0f));
    }
}

void App::drawStations() const {
    const oc::World& w = mission_.world();
    const auto tgt = target();
    const bool pickup = mission_.currentStop() && mission_.currentStop()->kind == oc::StopKind::Pickup;
    const float pulse = 0.5f + 0.5f * f(std::sin(GetTime() * 4.0));
    for (std::size_t i = 0; i < w.stations().size(); ++i) {
        const oc::Station& s = w.stations()[i];
        const Vector2 pos = v2(w.stationPosition(i, w.time()));
        const bool isTarget = tgt == i;
        const Color color = isTarget ? (pickup ? kPickup : kDropoff) : kMuted;

        const float orbit = f(s.orbitRadius);
        const float half = 0.6f / camera_.zoom;
        DrawRing(v2(w.bodyPosition(s.parent, w.time())), orbit - half, orbit + half, 0.0f, 360.0f, 192,
                 Fade(color, isTarget ? 0.22f : 0.07f));

        const float spin = f(w.time() * 40.0);
        const float size = std::max(8.0f, 5.0f / camera_.zoom);
        DrawPoly(pos, 4, size, spin, color);
        DrawPolyLinesEx(pos, 4, size * 1.7f, -spin, 1.5f / camera_.zoom, Fade(color, 0.8f));
        if (isTarget) {
            const auto dock = f(oc::Mission::kDockRadius);
            DrawRing(pos, dock - 1.0f / camera_.zoom, dock + 1.0f / camera_.zoom, 0.0f, 360.0f, 64,
                     Fade(color, 0.25f + 0.45f * pulse));
        }
    }
}

void App::drawTrajectory() const {
    const oc::World& w = mission_.world();
    if (mission_.state() != oc::MissionState::Flying) {
        return;
    }
    constexpr int kPoints = 240;
    const auto points = w.predictTrajectory(kPoints, 6);
    const float thickness = 2.0f / camera_.zoom;
    Vector2 previous = v2(w.ship().position);
    for (std::size_t i = 0; i < points.size(); ++i) {
        const Vector2 p = v2(points[i]);
        const float fade = 1.0f - static_cast<float>(i) / static_cast<float>(points.size());
        DrawLineEx(previous, p, thickness, Fade(kPath, 0.55f * fade));
        previous = p;
    }
    if (points.size() < kPoints && !points.empty()) {
        DrawCircleV(v2(points.back()), 6.0f / camera_.zoom, kDanger); // predicted impact
    }
}

void App::drawApproach() const {
    if (!approach_ || approach_->time < 0.3 || approach_->distance > 400.0) {
        return;
    }
    const bool dockable =
        approach_->distance <= oc::Mission::kDockRadius && approach_->relativeSpeed <= oc::Mission::kMaxDockSpeed;
    const Color color = dockable ? kDropoff : kPath;
    const float line = 1.5f / camera_.zoom;
    const Vector2 ship = v2(approach_->shipPosition);
    const Vector2 station = v2(approach_->stationPosition);
    DrawLineEx(ship, station, line, Fade(color, 0.7f));
    DrawCircleLinesV(ship, 7.0f / camera_.zoom, color);
    DrawPolyLinesEx(station, 4, 9.0f / camera_.zoom, 45.0f, line, color);
}

void App::drawShip() const {
    const oc::World& w = mission_.world();
    const oc::Ship& s = w.ship();
    const Vector2 p = v2(s.position);
    const float scale = std::max(1.0f, 0.8f / camera_.zoom);

    if (mission_.state() == oc::MissionState::Crashed) {
        const float t = std::min(endTimer_, 1.2f);
        DrawCircleV(p, 6.0f + 60.0f * t, Fade(kFlame, std::max(0.0f, 0.6f - t * 0.5f)));
        DrawCircleLinesV(p, 10.0f + 90.0f * t, Fade(kDanger, std::max(0.0f, 1.0f - t)));
        return;
    }

    const oc::Vec2 fwd = oc::Vec2::fromAngle(s.angle) * scale;
    const oc::Vec2 side{-fwd.y, fwd.x};
    const Vector2 nose = v2(s.position + fwd * 16.0);
    const Vector2 left = v2(s.position - fwd * 10.0 + side * 10.0);
    const Vector2 right = v2(s.position - fwd * 10.0 - side * 10.0);

    if (w.thrusting()) {
        const bool fine = IsKeyDown(KEY_LEFT_SHIFT) || IsKeyDown(KEY_RIGHT_SHIFT);
        const double flicker = (fine ? 5.0 : 12.0) + 5.0 * std::sin(GetTime() * 60.0);
        const Vector2 tail = v2(s.position - fwd * (10.0 + flicker));
        const Vector2 l = v2(s.position - fwd * 8.5 + side * 5.0);
        const Vector2 r = v2(s.position - fwd * 8.5 - side * 5.0);
        DrawTriangle(l, tail, r, kFlame);
        DrawTriangle(l, r, tail, kFlame);
    }
    // Draw both windings: raylib culls one of them depending on orientation.
    DrawTriangle(nose, left, right, kShip);
    DrawTriangle(nose, right, left, kShip);
}

// ---------------------------------------------------------------------------------------------
// Screen-space overlays

void App::drawLabels() const {
    const oc::World& w = mission_.world();
    for (std::size_t i = 0; i < w.bodies().size(); ++i) {
        const oc::Body& b = w.bodies()[i];
        const Vector2 p = GetWorldToScreen2D(v2(w.bodyPosition(i, w.time()) + oc::Vec2{0.0, b.radius}), camera_);
        const char* name = b.name.c_str();
        DrawText(name, static_cast<int>(p.x) - MeasureText(name, 14) / 2, static_cast<int>(p.y) + 8, 14,
                 Fade(kText, 0.45f));
    }

    const auto tgt = target();
    for (std::size_t i = 0; i < w.stations().size(); ++i) {
        const Vector2 p = GetWorldToScreen2D(v2(w.stationPosition(i, w.time())), camera_);
        const bool isTarget = tgt == i;
        DrawText(w.stations()[i].name.c_str(), static_cast<int>(p.x) + 16, static_cast<int>(p.y) - 7, 14,
                 isTarget ? kText : Fade(kMuted, 0.8f));
        if (isTarget) {
            const bool pickup = mission_.currentStop()->kind == oc::StopKind::Pickup;
            DrawText(pickup ? "PICK UP" : "DELIVER", static_cast<int>(p.x) + 16, static_cast<int>(p.y) + 9, 12,
                     pickup ? kPickup : kDropoff);
        }
    }

    if (approach_ && approach_->time >= 0.3 && approach_->distance <= 400.0) {
        const Vector2 p = GetWorldToScreen2D(v2(approach_->stationPosition), camera_);
        const std::string info = TextFormat("closest %.0f  rel %.0f  in %.0fs", approach_->distance,
                                            approach_->relativeSpeed, approach_->time);
        DrawText(info.c_str(), static_cast<int>(p.x) + 16, static_cast<int>(p.y) + 24, 12, kPath);
    }
}

void App::drawTargetArrow() const {
    const auto tgt = target();
    if (!tgt || mission_.state() != oc::MissionState::Flying) {
        return;
    }
    const oc::World& w = mission_.world();
    const Vector2 p = GetWorldToScreen2D(v2(w.stationPosition(*tgt, w.time())), camera_);
    // Keep the arrow inside a frame that leaves room for the HUD at the top and bottom.
    constexpr float side = 60.0f;
    constexpr float top = 130.0f;
    constexpr float bottom = 100.0f;
    if (p.x > side && p.x < kWidth - side && p.y > top && p.y < kHeight - bottom) {
        return;
    }
    const Vector2 c{kWidth / 2.0f, (top + kHeight - bottom) / 2.0f};
    const Vector2 d{p.x - c.x, p.y - c.y};
    const float sx = (kWidth / 2.0f - side) / std::max(std::abs(d.x), 1e-3f);
    const float sy = ((kHeight - top - bottom) / 2.0f) / std::max(std::abs(d.y), 1e-3f);
    const float s = std::min(sx, sy);
    const Vector2 edge{c.x + d.x * s, c.y + d.y * s};
    const float a = std::atan2(d.y, d.x);
    const Vector2 tip{edge.x + std::cos(a) * 14.0f, edge.y + std::sin(a) * 14.0f};
    const Vector2 l{edge.x + std::cos(a + 2.5f) * 10.0f, edge.y + std::sin(a + 2.5f) * 10.0f};
    const Vector2 r{edge.x + std::cos(a - 2.5f) * 10.0f, edge.y + std::sin(a - 2.5f) * 10.0f};
    const bool pickup = mission_.currentStop()->kind == oc::StopKind::Pickup;
    const Color color = pickup ? kPickup : kDropoff;
    DrawTriangle(tip, l, r, color);
    DrawTriangle(tip, r, l, color);
    const double distance = (w.stationPosition(*tgt, w.time()) - w.ship().position).length();
    const std::string label = TextFormat("%.0f", distance);
    DrawText(label.c_str(), static_cast<int>(edge.x - std::cos(a) * 30.0f) - MeasureText(label.c_str(), 14) / 2,
             static_cast<int>(edge.y - std::sin(a) * 26.0f) - 7, 14, color);
}

void App::drawHud() const {
    const oc::World& w = mission_.world();
    const oc::Level& level = mission_.level();
    const oc::Ship& s = w.ship();

    // Soft backdrops keep the HUD readable over planets and labels.
    DrawRectangleGradientH(0, 0, 460, 110, Fade(kSpace, 0.85f), Fade(kSpace, 0.0f));
    DrawRectangleGradientV(0, kHeight - 90, kWidth, 90, Fade(kSpace, 0.0f), Fade(kSpace, 0.85f));

    DrawText(TextFormat("LEVEL %d/%d", static_cast<int>(levelIndex_) + 1, static_cast<int>(levels_.size())), 24, 18, 14,
             kMuted);
    DrawText(level.name.c_str(), 24, 36, 24, kText);
    if (const auto stop = mission_.currentStop()) {
        const bool pickup = stop->kind == oc::StopKind::Pickup;
        const std::string& name = w.stations()[stop->station].name;
        const std::string goal = pickup ? "Pick up parcel at " + name : "Deliver parcel to " + name;
        DrawText(goal.c_str(), 24, 66, 18, pickup ? kPickup : kDropoff);
        DrawText(TextFormat("stop %d of %d", static_cast<int>(mission_.stopsDone()) + 1,
                            static_cast<int>(mission_.route().size())),
                 24, 88, 14, kMuted);
    }

    const double t = mission_.state() == oc::MissionState::Complete ? mission_.result().time : w.time();
    const std::string time = "TIME " + seconds(t);
    const std::string par = "par " + seconds(level.par.time);
    DrawText(time.c_str(), kWidth - 24 - MeasureText(time.c_str(), 22), 20, 22, t <= level.par.time ? kText : kDanger);
    DrawText(par.c_str(), kWidth - 24 - MeasureText(par.c_str(), 14), 46, 14, kMuted);
    if (const auto& best = best_[levelIndex_]) {
        drawStars({kWidth - 60.0f, 78.0f}, best->stars, 8.0f, 20.0f);
    }

    // Fuel gauge with the third-star threshold.
    constexpr int gx = 24;
    constexpr int gy = kHeight - 40;
    constexpr int gw = 220;
    DrawText("FUEL", gx, gy - 20, 14, kMuted);
    DrawRectangle(gx, gy, gw, 12, Fade(kText, 0.12f));
    DrawRectangle(gx, gy, static_cast<int>(gw * s.fuel), 12, s.fuel >= level.par.fuelLeft ? kFlame : kDanger);
    const int parX = gx + static_cast<int>(gw * level.par.fuelLeft);
    DrawRectangle(parX - 1, gy - 4, 2, 20, kText);

    DrawText(TextFormat("SPEED %5.1f", s.velocity.length()), 270, kHeight - 60, 16, kText);
    if (const auto tgt = target()) {
        const oc::Vec2 rel = s.velocity - w.stationVelocity(*tgt, w.time());
        const double distance = (w.stationPosition(*tgt, w.time()) - s.position).length();
        const bool slow = rel.length() <= oc::Mission::kMaxDockSpeed;
        DrawText(TextFormat("TARGET %5.0f away", distance), 270, kHeight - 38, 16, kText);
        DrawText(TextFormat("REL SPEED %5.1f", rel.length()), 470, kHeight - 60, 16, slow ? kDropoff : kDanger);
        DrawText(TextFormat("dock below %.0f", oc::Mission::kMaxDockSpeed), 470, kHeight - 38, 14, kMuted);
    }

    const char* help = "A/D turn   W thrust   Shift fine   Tab map   wheel zoom   P pause   R restart";
    DrawText(help, kWidth - MeasureText(help, 14) - 24, kHeight - 28, 14, Fade(kText, 0.45f));
}

void App::drawToast() const {
    const auto& e = mission_.lastEvent();
    const oc::World& w = mission_.world();
    if (!e || w.time() - e->time > 2.5) {
        return;
    }
    const std::string& station = w.stations()[e->station].name;
    std::string text;
    Color color = kText;
    switch (e->type) {
    case oc::MissionEvent::Type::PickedUp:
        text = "Parcel picked up at " + station;
        color = kPickup;
        break;
    case oc::MissionEvent::Type::Delivered:
        text = "Delivered to " + station;
        color = kDropoff;
        break;
    case oc::MissionEvent::Type::TooFast:
        text = TextFormat("Too fast to dock: %.0f (max %.0f)", e->speed, oc::Mission::kMaxDockSpeed);
        color = kDanger;
        break;
    }
    const float alpha = std::min(1.0f, f(2.5 - (w.time() - e->time)) * 2.0f);
    textCentered(text.c_str(), 120, 22, Fade(color, alpha));
}

void App::drawBriefing() const {
    const oc::Level& level = mission_.level();
    DrawRectangle(0, 0, kWidth, kHeight, Fade(kSpace, 0.55f));
    constexpr int boxW = 640;
    constexpr int boxX = (kWidth - boxW) / 2;
    DrawRectangle(boxX, 170, boxW, 360, Fade(BLACK, 0.88f));
    DrawRectangleLines(boxX, 170, boxW, 360, Fade(kText, 0.25f));

    textCentered(TextFormat("LEVEL %d OF %d", static_cast<int>(levelIndex_) + 1, static_cast<int>(levels_.size())), 196,
                 16, kMuted);
    textCentered(level.name.c_str(), 222, 36, kText);
    int y = 280;
    for (const std::string& line : wrap(level.briefing, 18, boxW - 80)) {
        textCentered(line.c_str(), y, 18, kText);
        y += 26;
    }
    y += 16;
    textCentered(TextFormat("2nd star: finish within %.0f s      3rd star: keep %.0f%% fuel", level.par.time,
                            level.par.fuelLeft * 100.0),
                 y, 16, kFlame);
    if (const auto& best = best_[levelIndex_]) {
        drawStars({kWidth / 2.0f, static_cast<float>(y) + 40.0f}, best->stars, 10.0f, 26.0f);
    }
    textCentered("Press Enter to launch", 470, 20, kDropoff);
    textCentered("[ and ] switch levels", 498, 14, kMuted);
}

void App::drawResult() const {
    DrawRectangle(0, 0, kWidth, kHeight, Fade(kSpace, 0.55f));
    DrawRectangle((kWidth - 640) / 2, 170, 640, 330, Fade(BLACK, 0.88f));
    DrawRectangleLines((kWidth - 640) / 2, 170, 640, 330, Fade(kText, 0.25f));
    const oc::World& w = mission_.world();
    switch (mission_.state()) {
    case oc::MissionState::Complete: {
        const oc::Result r = mission_.result();
        const oc::Level& level = mission_.level();
        const bool last = levelIndex_ + 1 == levels_.size();
        textCentered(last ? "Campaign complete!" : "All parcels delivered!", 200, 36, kDropoff);
        drawStars({kWidth / 2.0f, 285.0f}, r.stars, 26.0f, 70.0f);
        textCentered(TextFormat("SCORE %d", r.score), 340, 28, kText);
        textCentered(TextFormat("time %.1f s (par %.0f s)      fuel left %.0f%% (par %.0f%%)", r.time, level.par.time,
                                r.fuelLeft * 100.0, level.par.fuelLeft * 100.0),
                     382, 16, kMuted);
        textCentered(last ? "Enter: play again from level 1     R: retry" : "Enter: next level     R: retry", 440, 20,
                     kText);
        break;
    }
    case oc::MissionState::Crashed:
        textCentered(("Crashed into " + w.bodies()[*w.crashedInto()].name).c_str(), 270, 36, kDanger);
        textCentered("The trajectory line turns red where it hits a surface.", 320, 18, kMuted);
        textCentered("Enter or R: try again", 380, 20, kText);
        break;
    case oc::MissionState::Lost:
        textCentered("Lost in deep space", 270, 36, kDanger);
        textCentered("You flew beyond the edge of the system.", 320, 18, kMuted);
        textCentered("Enter or R: try again", 380, 20, kText);
        break;
    case oc::MissionState::Flying:
        break;
    }
}

void App::drawPaused() const {
    DrawRectangle(0, 0, kWidth, kHeight, Fade(kSpace, 0.6f));
    textCentered("Paused", 290, 40, kText);
    textCentered("P or Enter: continue     R: restart     [ ]: switch level", 350, 18, kMuted);
}

} // namespace game
