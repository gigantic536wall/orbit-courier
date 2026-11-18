#pragma once

#include <optional>
#include <string>
#include <vector>

#include <raylib.h>

#include "audio.hpp"
#include "oc/autopilot.hpp"
#include "oc/levels.hpp"
#include "oc/mission.hpp"
#include "oc/progress.hpp"
#include "particles.hpp"

namespace game {

enum class Screen { Menu, Briefing, Playing, Paused, Result };

/// What a click, tap or key does. Buttons and keys map to the same actions.
enum class Action { None, Launch, Retry, Next, Levels, Resume, Pause, Map, Sound };

struct Button {
    Rectangle rect;
    std::string label;
    Action action = Action::None;
    bool primary = false;
};

/// On-screen flight controls, shown once the player touches the screen.
struct TouchControls {
    Rectangle left, right, fine, thrust;
};

/// Scripted run for screenshots (OC_CAPTURE=file.png [OC_LEVEL=n] [OC_FRAMES=n] [OC_SCREEN=menu|briefing]).
struct Capture {
    const char* path = nullptr;
    int level = 0;
    int frames = 150;
    std::string screen; ///< empty = playing
    // Demo recording (OC_RECORD=1): the autopilot flies the level, every `every`-th frame is saved as
    // frame_NNNN.png in the working directory, at `speed` times real time.
    bool record = false;
    int every = 3;
    float speed = 3.0f;

    [[nodiscard]] bool active() const { return path != nullptr || record; }
};

class App {
public:
    explicit App(Capture capture);

    /// One frame: input, simulation, drawing. Returns false when the app should quit.
    bool frame();

private:
    struct Star {
        float x, y, depth, size;
    };

    // Flow and input (app.cpp)
    void openMenu();
    void startLevel(std::size_t index);
    void launch();
    void perform(Action action);
    void handleInput();
    void handlePointer();
    [[nodiscard]] oc::Input flightInput() const;
    void simulate(float frameTime);
    void react(); ///< sounds and particles for mission events
    void updateCamera(float frameTime);
    void saveProgress();

    // Layout shared by input and drawing
    [[nodiscard]] std::vector<Button> buttons() const;
    [[nodiscard]] static Rectangle menuCard(std::size_t index);
    [[nodiscard]] static TouchControls touchControls();
    [[nodiscard]] float overviewZoom() const;
    [[nodiscard]] std::optional<std::size_t> target() const;

    // Drawing (render.cpp)
    void drawBackground() const;
    void drawWorld() const;
    void drawBodies() const;
    void drawStations() const;
    void drawTrajectory() const;
    void drawApproach() const;
    void drawShip() const;
    void drawLabels() const;
    void drawTargetArrow() const;
    void drawHud() const;
    void drawToast() const;
    void drawMenu() const;
    void drawBriefing() const;
    void drawResult() const;
    void drawPaused() const;
    void drawButtons() const;
    void drawTouchControls() const;

    Capture capture_;
    std::vector<oc::Level> levels_ = oc::campaign();
    oc::Progress progress_;
    std::size_t levelIndex_ = 0;
    std::size_t menuSelection_ = 0;
    oc::Mission mission_;
    Screen screen_ = Screen::Menu;

    Audio audio_;
    Particles particles_;

    Camera2D camera_{};
    float zoom_ = 1.0f;
    bool overview_ = true;
    double accumulator_ = 0.0;
    float endTimer_ = 0.0f;
    int frameCount_ = 0;
    std::optional<oc::Approach> approach_;
    std::vector<Star> stars_;
    oc::Input lastInput_;
    std::optional<oc::HohmannPilot> pilot_; ///< drives demo recordings
    int recorded_ = 0;
    int resultFrames_ = 0;

    bool touchUi_ = false;
    int previousTouches_ = 0;
    double lastTouchTime_ = -10.0;

    double seenEventTime_ = -1.0;
    oc::MissionState seenState_ = oc::MissionState::Flying;
};

} // namespace game
