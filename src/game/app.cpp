// Game flow and input: menu, briefing, flight, pause and results; keyboard, mouse and touch.

#include "app.hpp"

#include <algorithm>
#include <cmath>
#include <initializer_list>

#include "storage.hpp"
#include "theme.hpp"

namespace game {

namespace {

constexpr const char* kSaveKey = "orbit-courier-progress";

/// A centred row of equally sized buttons.
void addRow(std::vector<Button>& out, float y, std::initializer_list<std::pair<const char*, Action>> items,
            Action primary) {
    constexpr float w = 180.0f;
    constexpr float h = 46.0f;
    constexpr float gap = 18.0f;
    const auto n = static_cast<float>(items.size());
    float x = (static_cast<float>(kWidth) - (n * w + (n - 1.0f) * gap)) / 2.0f;
    for (const auto& [label, action] : items) {
        out.push_back({{x, y, w, h}, label, action, action == primary});
        x += w + gap;
    }
}

} // namespace

App::App(Capture capture)
    : capture_(std::move(capture)),
      progress_(capture_.path != nullptr ? oc::Progress(levels_.size())
                                         : oc::Progress::parse(loadText(kSaveKey), levels_.size())),
      mission_(levels_.front()) {
    std::uint32_t seed = 0x9E3779B9u;
    auto unit = [&] { return static_cast<float>(nextRandom(seed) % 10'000) / 10'000.0f; };
    for (int i = 0; i < 260; ++i) {
        stars_.push_back({unit() * kWidth, unit() * kHeight, 0.05f + unit() * 0.25f, 0.6f + unit() * 1.4f});
    }
    camera_.offset = {kWidth / 2.0f, kHeight / 2.0f};
    audio_.setEnabled(progress_.soundOn);
    menuSelection_ = progress_.suggestedLevel();

    if (capture_.path == nullptr || capture_.screen == "menu") {
        openMenu();
        return;
    }
    startLevel(std::min<std::size_t>(static_cast<std::size_t>(std::max(capture_.level, 0)), levels_.size() - 1));
    if (capture_.screen != "briefing") {
        launch();
    }
}

// ---------------------------------------------------------------------------------------------
// Flow

void App::openMenu() {
    startLevel(menuSelection_); // the selected level's system is the menu background
    screen_ = Screen::Menu;
}

void App::startLevel(std::size_t index) {
    levelIndex_ = index;
    mission_ = oc::Mission(levels_[index]);
    screen_ = Screen::Briefing;
    overview_ = true;
    accumulator_ = 0.0;
    endTimer_ = 0.0f;
    approach_.reset();
    particles_.clear();
    seenEventTime_ = -1.0;
    seenState_ = oc::MissionState::Flying;
    camera_.target = {0.0f, 0.0f};
    camera_.zoom = overviewZoom();
}

void App::launch() {
    screen_ = Screen::Playing;
    overview_ = false;
}

void App::perform(Action action) {
    if (action != Action::None && action != Action::Pause && action != Action::Map) {
        audio_.play(Cue::Click);
    }
    switch (action) {
    case Action::None:
        break;
    case Action::Launch:
        launch();
        break;
    case Action::Retry:
        startLevel(levelIndex_);
        launch();
        break;
    case Action::Next:
        if (levelIndex_ + 1 < levels_.size() && progress_.unlocked(levelIndex_ + 1)) {
            startLevel(levelIndex_ + 1);
        } else {
            openMenu();
        }
        break;
    case Action::Levels:
        menuSelection_ = levelIndex_;
        openMenu();
        break;
    case Action::Resume:
        screen_ = Screen::Playing;
        break;
    case Action::Pause:
        screen_ = Screen::Paused;
        break;
    case Action::Map:
        overview_ = !overview_;
        break;
    case Action::Sound:
        progress_.soundOn = !progress_.soundOn;
        audio_.setEnabled(progress_.soundOn);
        audio_.play(Cue::Click);
        saveProgress();
        break;
    }
}

void App::saveProgress() {
    if (capture_.path == nullptr) {
        saveText(kSaveKey, progress_.serialize());
    }
}

// ---------------------------------------------------------------------------------------------
// Input

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

    if (touchUi_) {
        const TouchControls tc = touchControls();
        for (int i = 0; i < GetTouchPointCount(); ++i) {
            const Vector2 p = GetTouchPosition(i);
            if (CheckCollisionPointRec(p, tc.left)) {
                input.turn -= 1;
            } else if (CheckCollisionPointRec(p, tc.right)) {
                input.turn += 1;
            } else if (CheckCollisionPointRec(p, tc.thrust)) {
                input.thrust = true;
            } else if (CheckCollisionPointRec(p, tc.fine)) {
                input.thrust = true;
                input.fine = true;
            }
        }
        input.turn = std::clamp(input.turn, -1, 1);
    }
    return input;
}

void App::handleInput() {
    // Read the key queue instead of IsKeyPressed: a quick tap can start and end between two frames
    // (especially in the browser), and the queue still remembers it.
    std::vector<int> keys;
    for (int key = GetKeyPressed(); key != 0; key = GetKeyPressed()) {
        keys.push_back(key);
    }
    auto pressed = [&](std::initializer_list<int> wanted) {
        return std::any_of(keys.begin(), keys.end(),
                           [&](int k) { return std::find(wanted.begin(), wanted.end(), k) != wanted.end(); });
    };
    const bool confirm = pressed({KEY_ENTER, KEY_KP_ENTER, KEY_SPACE});

    if (pressed({KEY_M})) {
        perform(Action::Sound);
    }

    switch (screen_) {
    case Screen::Menu: {
        const auto count = static_cast<int>(levels_.size());
        int sel = static_cast<int>(menuSelection_);
        if (pressed({KEY_RIGHT, KEY_D})) {
            sel = std::min(sel + 1, count - 1);
        } else if (pressed({KEY_LEFT, KEY_A})) {
            sel = std::max(sel - 1, 0);
        } else if (pressed({KEY_DOWN, KEY_S})) {
            sel = std::min(sel + 5, count - 1);
        } else if (pressed({KEY_UP, KEY_W})) {
            sel = std::max(sel - 5, 0);
        }
        if (static_cast<std::size_t>(sel) != menuSelection_) {
            menuSelection_ = static_cast<std::size_t>(sel);
            audio_.play(Cue::Click);
            openMenu();
        } else if (confirm && progress_.unlocked(menuSelection_)) {
            audio_.play(Cue::Click);
            startLevel(menuSelection_);
        }
        break;
    }
    case Screen::Briefing:
        if (confirm) {
            perform(Action::Launch);
        } else if (pressed({KEY_ESCAPE, KEY_BACKSPACE, KEY_L})) {
            perform(Action::Levels);
        } else if (pressed({KEY_RIGHT_BRACKET}) && progress_.unlocked(levelIndex_ + 1)) {
            startLevel(levelIndex_ + 1);
        } else if (pressed({KEY_LEFT_BRACKET}) && levelIndex_ > 0) {
            startLevel(levelIndex_ - 1);
        }
        break;
    case Screen::Playing:
        if (pressed({KEY_P, KEY_ESCAPE})) {
            perform(Action::Pause);
        } else if (pressed({KEY_R})) {
            perform(Action::Retry);
        } else if (pressed({KEY_TAB})) {
            perform(Action::Map);
        }
        break;
    case Screen::Paused:
        if (pressed({KEY_P, KEY_ESCAPE}) || confirm) {
            perform(Action::Resume);
        } else if (pressed({KEY_R})) {
            perform(Action::Retry);
        } else if (pressed({KEY_L})) {
            perform(Action::Levels);
        }
        break;
    case Screen::Result:
        if (confirm) {
            perform(mission_.state() == oc::MissionState::Complete ? Action::Next : Action::Retry);
        } else if (pressed({KEY_R})) {
            perform(Action::Retry);
        } else if (pressed({KEY_L, KEY_ESCAPE})) {
            perform(Action::Levels);
        }
        break;
    }

    if (screen_ == Screen::Playing && !overview_) {
        zoom_ = std::clamp(zoom_ * std::pow(1.15f, GetMouseWheelMove()), 0.2f, 3.0f);
    }
}

void App::handlePointer() {
    bool pressed = false;
    Vector2 at{};
#if defined(__EMSCRIPTEN__)
    // Phones and tablets: the first touch switches on the on-screen flight controls.
    const int touches = GetTouchPointCount();
    if (touches > 0) {
        touchUi_ = true;
        lastTouchTime_ = GetTime();
        if (previousTouches_ == 0) {
            pressed = true;
            at = GetTouchPosition(0);
        }
    }
    previousTouches_ = touches;
#endif
    // Browsers also send a mouse click after a tap; ignore it so a button never fires twice.
    if (!pressed && IsMouseButtonPressed(MOUSE_BUTTON_LEFT) && GetTime() - lastTouchTime_ > 0.6) {
        pressed = true;
        at = GetMousePosition();
    }
    if (!pressed) {
        return;
    }

    if (screen_ == Screen::Menu) {
        for (std::size_t i = 0; i < levels_.size(); ++i) {
            if (CheckCollisionPointRec(at, menuCard(i))) {
                if (progress_.unlocked(i)) {
                    menuSelection_ = i;
                    audio_.play(Cue::Click);
                    startLevel(i);
                }
                return;
            }
        }
    }
    for (const Button& b : buttons()) {
        if (CheckCollisionPointRec(at, b.rect)) {
            perform(b.action);
            return;
        }
    }
}

// ---------------------------------------------------------------------------------------------
// Layout

Rectangle App::menuCard(std::size_t index) {
    constexpr float w = 210.0f;
    constexpr float h = 118.0f;
    constexpr float gap = 20.0f;
    constexpr float x0 = (kWidth - (5.0f * w + 4.0f * gap)) / 2.0f;
    const auto col = static_cast<float>(index % 5);
    const auto row = static_cast<float>(index / 5);
    return {x0 + col * (w + gap), 250.0f + row * (h + gap), w, h};
}

TouchControls App::touchControls() {
    constexpr float s = 110.0f;
    constexpr float y = kHeight - s - 24.0f;
    return {.left = {24.0f, y, s, s},
            .right = {24.0f + s + 16.0f, y, s, s},
            .fine = {kWidth - 2.0f * s - 40.0f, y + 20.0f, s, s - 20.0f},
            .thrust = {kWidth - s - 24.0f, y - 20.0f, s, s + 20.0f}};
}

std::vector<Button> App::buttons() const {
    std::vector<Button> out;
    switch (screen_) {
    case Screen::Menu:
        out.push_back({{kWidth - 200.0f, 24.0f, 176.0f, 40.0f}, progress_.soundOn ? "Sound: on" : "Sound: off",
                       Action::Sound});
        break;
    case Screen::Briefing:
        addRow(out, 452.0f, {{"Launch", Action::Launch}, {"Levels", Action::Levels}}, Action::Launch);
        break;
    case Screen::Playing:
        out.push_back({{kWidth - 24.0f - 44.0f, 104.0f, 44.0f, 36.0f}, "II", Action::Pause});
        out.push_back({{kWidth - 24.0f - 44.0f - 12.0f - 64.0f, 104.0f, 64.0f, 36.0f}, "MAP", Action::Map});
        break;
    case Screen::Paused:
        addRow(out, 380.0f, {{"Resume", Action::Resume}, {"Restart", Action::Retry}, {"Levels", Action::Levels}},
               Action::Resume);
        break;
    case Screen::Result:
        if (mission_.state() == oc::MissionState::Complete) {
            const bool last = levelIndex_ + 1 == levels_.size();
            addRow(out, 430.0f,
                   {{last ? "Levels" : "Next level", last ? Action::Levels : Action::Next},
                    {"Retry", Action::Retry},
                    {"Levels", Action::Levels}},
                   last ? Action::Levels : Action::Next);
            if (last) {
                out.erase(out.begin() + 2); // "Levels" is already the main button
                const float shift = (180.0f + 18.0f) / 2.0f;
                for (Button& b : out) {
                    b.rect.x += shift;
                }
            }
        } else {
            addRow(out, 400.0f, {{"Try again", Action::Retry}, {"Levels", Action::Levels}}, Action::Retry);
        }
        break;
    }
    return out;
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

// ---------------------------------------------------------------------------------------------
// Simulation, effects and the frame

void App::simulate(float frameTime) {
    const bool flying = screen_ == Screen::Playing;
    audio_.setThrust(flying && mission_.world().thrusting() ? 1.0f : 0.0f, lastInput_.fine);
    if (!flying) {
        return;
    }

    // Fixed time step: the simulation runs at 120 Hz whatever the frame rate is.
    accumulator_ += std::min(static_cast<double>(frameTime), 0.25);
    lastInput_ = flightInput();
    while (accumulator_ >= oc::World::kStep) {
        mission_.step(lastInput_);
        accumulator_ -= oc::World::kStep;
    }
    react();

    if (const auto t = target(); t && mission_.state() == oc::MissionState::Flying) {
        approach_ = mission_.world().closestApproach(*t, 90.0);
    } else {
        approach_.reset();
    }

    if (mission_.state() != oc::MissionState::Flying) {
        endTimer_ += frameTime;
        if (endTimer_ > 1.2f) {
            screen_ = Screen::Result;
            if (mission_.state() == oc::MissionState::Complete && progress_.record(levelIndex_, mission_.result())) {
                saveProgress();
            }
        }
    }
}

void App::react() {
    const oc::World& w = mission_.world();
    const oc::Ship& ship = w.ship();

    if (w.thrusting()) {
        const oc::Vec2 fwd = oc::Vec2::fromAngle(ship.angle);
        const oc::Vec2 side{-fwd.y, fwd.x};
        const int count = lastInput_.fine ? 1 : 3;
        for (int i = 0; i < count; ++i) {
            const oc::Vec2 velocity = ship.velocity - fwd * (60.0 + 50.0 * particles_.random()) +
                                      side * ((particles_.random() - 0.5) * 30.0);
            particles_.emit(ship.position - fwd * 10.0, velocity, 0.45f, 2.6f, kFlame);
        }
    }

    if (const auto& e = mission_.lastEvent(); e && e->time != seenEventTime_) {
        seenEventTime_ = e->time;
        const oc::Vec2 at = w.stationPosition(e->station, w.time());
        const oc::Vec2 v = w.stationVelocity(e->station, w.time());
        switch (e->type) {
        case oc::MissionEvent::Type::PickedUp:
            audio_.play(Cue::Pickup);
            particles_.burst(at, v, 40, 60.0, 0.8f, kPickup);
            break;
        case oc::MissionEvent::Type::Delivered:
            audio_.play(mission_.state() == oc::MissionState::Complete ? Cue::Complete : Cue::Deliver);
            particles_.burst(at, v, 60, 80.0, 1.0f, kDropoff);
            break;
        case oc::MissionEvent::Type::TooFast:
            audio_.play(Cue::TooFast);
            break;
        }
    }

    if (mission_.state() != seenState_) {
        seenState_ = mission_.state();
        if (seenState_ == oc::MissionState::Crashed) {
            audio_.play(Cue::Crash);
            const oc::Vec2 ground = w.bodyVelocity(*w.crashedInto(), w.time());
            particles_.burst(ship.position, ground, 90, 140.0, 1.4f, kFlame);
            particles_.burst(ship.position, ground, 40, 90.0, 1.8f, kDanger);
            particles_.burst(ship.position, ground, 25, 60.0, 2.2f, kShip);
        } else if (seenState_ == oc::MissionState::Lost) {
            audio_.play(Cue::Lost);
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
    handlePointer();
    simulate(dt);
    if (screen_ == Screen::Playing || screen_ == Screen::Result) {
        particles_.update(dt);
    }
    updateCamera(dt);

    BeginDrawing();
    ClearBackground(kSpace);
    drawBackground();
    BeginMode2D(camera_);
    drawWorld();
    EndMode2D();
    if (screen_ == Screen::Menu) {
        drawMenu();
    } else {
        drawLabels();
        drawTargetArrow();
        drawHud();
        drawToast();
        if (touchUi_ && screen_ == Screen::Playing) {
            drawTouchControls();
        }
        if (screen_ == Screen::Briefing) {
            drawBriefing();
        } else if (screen_ == Screen::Result) {
            drawResult();
        } else if (screen_ == Screen::Paused) {
            drawPaused();
        }
    }
    drawButtons();
    EndDrawing();

    if (capture_.path != nullptr && frameCount_ >= capture_.frames) {
        TakeScreenshot(capture_.path);
        return false;
    }
    return true;
}

} // namespace game
