#pragma once

#include <optional>
#include <vector>

#include <raylib.h>

#include "oc/levels.hpp"
#include "oc/mission.hpp"

namespace game {

enum class Screen { Briefing, Playing, Paused, Result };

/// Scripted run for screenshots (OC_CAPTURE=file.png [OC_LEVEL=n] [OC_FRAMES=n]).
struct Capture {
    const char* path = nullptr;
    int level = 0;
    int frames = 150;
    bool briefing = false; // OC_BRIEFING=1: capture the briefing screen
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

    void startLevel(std::size_t index);
    void handleInput();
    [[nodiscard]] oc::Input flightInput() const;
    void simulate(float frameTime);
    void updateCamera(float frameTime);

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
    void drawBriefing() const;
    void drawResult() const;
    void drawPaused() const;

    [[nodiscard]] float overviewZoom() const;
    [[nodiscard]] std::optional<std::size_t> target() const;

    Capture capture_;
    std::vector<oc::Level> levels_ = oc::campaign();
    std::vector<std::optional<oc::Result>> best_;
    std::size_t levelIndex_ = 0;
    oc::Mission mission_;
    Screen screen_ = Screen::Briefing;

    Camera2D camera_{};
    float zoom_ = 1.0f;
    bool overview_ = true;
    double accumulator_ = 0.0;
    float endTimer_ = 0.0f;
    int frameCount_ = 0;
    std::optional<oc::Approach> approach_;
    std::vector<Star> stars_;
};

} // namespace game
