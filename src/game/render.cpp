// Drawing: the world in camera space, then HUD and screens in screen space.

#include <algorithm>
#include <cmath>
#include <string>

#include "app.hpp"
#include "theme.hpp"

namespace game {

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
    particles_.draw(camera_.zoom);
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
        const bool fine = lastInput_.fine;
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
        // Keep the label clear of the HUD panels at the top and bottom of the screen.
        const int x = std::clamp(static_cast<int>(p.x) + 16, 24, kWidth - 24 - MeasureText(info.c_str(), 12));
        const int y = std::clamp(static_cast<int>(p.y) + 24, 120, kHeight - 110);
        DrawText(info.c_str(), x, y, 12, kPath);
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
    if (const auto& best = progress_.best(levelIndex_)) {
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

    if (!touchUi_) {
        const char* help = "A/D turn   W thrust   Shift fine   Tab map   wheel zoom   P pause   R restart   M sound";
        DrawText(help, kWidth - MeasureText(help, 14) - 24, kHeight - 28, 14, Fade(kText, 0.45f));
    }
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
    if (const auto& best = progress_.best(levelIndex_)) {
        drawStars({kWidth / 2.0f, static_cast<float>(y) + 40.0f}, best->stars, 10.0f, 26.0f);
    }
    if (!touchUi_) {
        textCentered("Enter: launch     Esc: levels     [ ]: previous / next level", 508, 14, kMuted);
    }
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
        break;
    }
    case oc::MissionState::Crashed:
        textCentered(("Crashed into " + w.bodies()[*w.crashedInto()].name).c_str(), 270, 36, kDanger);
        textCentered("The trajectory line turns red where it hits a surface.", 320, 18, kMuted);
        break;
    case oc::MissionState::Lost:
        textCentered("Lost in deep space", 270, 36, kDanger);
        textCentered("You flew beyond the edge of the system.", 320, 18, kMuted);
        break;
    case oc::MissionState::Flying:
        break;
    }
}

void App::drawPaused() const {
    DrawRectangle(0, 0, kWidth, kHeight, Fade(kSpace, 0.6f));
    textCentered("Paused", 290, 40, kText);
    if (!touchUi_) {
        textCentered("P: continue     R: restart     L: levels", 340, 16, kMuted);
    }
}

// ---------------------------------------------------------------------------------------------
// Menu, buttons and touch controls

void App::drawMenu() const {
    DrawRectangle(0, 0, kWidth, kHeight, Fade(kSpace, 0.55f));
    textCentered("ORBIT COURIER", 92, 56, kText);
    textCentered("Deliver parcels across gravity wells", 156, 20, kMuted);
    const std::string total = TextFormat("%d / %d", progress_.totalStars(), static_cast<int>(levels_.size()) * 3);
    const int totalWidth = MeasureText(total.c_str(), 20) + 26;
    drawStar({kWidth / 2.0f - static_cast<float>(totalWidth) / 2.0f + 8.0f, 205.0f}, 9.0f, kFlame);
    DrawText(total.c_str(), kWidth / 2 - totalWidth / 2 + 24, 196, 20, kText);

    for (std::size_t i = 0; i < levels_.size(); ++i) {
        const Rectangle r = menuCard(i);
        const bool open = progress_.unlocked(i);
        const bool selected = i == menuSelection_;
        DrawRectangleRec(r, Fade(BLACK, selected ? 0.85f : 0.65f));
        DrawRectangleLinesEx(r, selected ? 2.0f : 1.0f, selected ? kDropoff : Fade(kText, open ? 0.3f : 0.12f));

        const int x = static_cast<int>(r.x) + 14;
        const int y = static_cast<int>(r.y) + 12;
        DrawText(TextFormat("%02d", static_cast<int>(i) + 1), x, y, 18, open ? kMuted : Fade(kMuted, 0.4f));
        if (!open) {
            // Padlock
            const Vector2 c{r.x + r.width / 2.0f, r.y + r.height / 2.0f + 6.0f};
            DrawRing({c.x, c.y - 10.0f}, 7.0f, 10.0f, 180.0f, 360.0f, 16, Fade(kMuted, 0.6f));
            DrawRectangleRounded({c.x - 13.0f, c.y - 10.0f, 26.0f, 20.0f}, 0.25f, 4, Fade(kMuted, 0.6f));
            continue;
        }
        const std::string& name = levels_[i].name;
        DrawText(name.c_str(), x, y + 30, 20, kText);
        if (const auto& best = progress_.best(i)) {
            drawStars({r.x + 44.0f, r.y + r.height - 26.0f}, best->stars, 9.0f, 24.0f);
            const std::string score = TextFormat("%d", best->score);
            DrawText(score.c_str(), static_cast<int>(r.x + r.width) - 14 - MeasureText(score.c_str(), 16),
                     static_cast<int>(r.y + r.height) - 34, 16, kMuted);
        } else {
            DrawText("new", x, static_cast<int>(r.y + r.height) - 34, 16, kPickup);
        }
    }

    const char* hint = touchUi_ ? "Tap a level to play" : "Click a level, or use the arrow keys and Enter";
    textCentered(hint, 560, 18, kMuted);
    textCentered("C++20  /  raylib  /  WebAssembly", kHeight - 40, 14, Fade(kMuted, 0.6f));
}

void App::drawButtons() const {
    const Vector2 mouse = GetMousePosition();
    for (const Button& b : buttons()) {
        const bool hover = !touchUi_ && CheckCollisionPointRec(mouse, b.rect);
        const Color accent = b.primary ? kDropoff : kText;
        DrawRectangleRounded(b.rect, 0.2f, 6, Fade(b.primary ? kDropoff : BLACK, b.primary ? (hover ? 0.35f : 0.22f) : (hover ? 0.85f : 0.6f)));
        DrawRectangleRoundedLinesEx(b.rect, 0.2f, 6, 1.5f, Fade(accent, hover ? 1.0f : 0.6f));
        const int size = b.rect.height < 40.0f ? 16 : 20;
        const int w = MeasureText(b.label.c_str(), size);
        DrawText(b.label.c_str(), static_cast<int>(b.rect.x + (b.rect.width - static_cast<float>(w)) / 2.0f),
                 static_cast<int>(b.rect.y + (b.rect.height - static_cast<float>(size)) / 2.0f), size, accent);
    }
}

void App::drawTouchControls() const {
    const TouchControls tc = touchControls();
    auto held = [](Rectangle r) {
        for (int i = 0; i < GetTouchPointCount(); ++i) {
            if (CheckCollisionPointRec(GetTouchPosition(i), r)) {
                return true;
            }
        }
        return false;
    };
    auto pad = [&](Rectangle r, const char* label, Color color) {
        const bool on = held(r);
        DrawRectangleRounded(r, 0.3f, 8, Fade(color, on ? 0.35f : 0.12f));
        DrawRectangleRoundedLinesEx(r, 0.3f, 8, 2.0f, Fade(color, on ? 0.9f : 0.45f));
        const int w = MeasureText(label, 22);
        DrawText(label, static_cast<int>(r.x + (r.width - static_cast<float>(w)) / 2.0f),
                 static_cast<int>(r.y + r.height / 2.0f - 11.0f), 22, Fade(color, 0.9f));
    };
    pad(tc.left, "<", kText);
    pad(tc.right, ">", kText);
    pad(tc.fine, "FINE", kPath);
    pad(tc.thrust, "THRUST", kFlame);
}

} // namespace game
