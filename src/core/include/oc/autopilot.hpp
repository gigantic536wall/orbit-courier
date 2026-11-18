#pragma once

#include "oc/mission.hpp"

namespace oc {

/// Flies the first job of a level as one textbook Hohmann transfer between two circular station orbits
/// around the same body: wait for the right phase angle, burn once (prograde to go up, retrograde to go
/// down), then coast. No corrections at arrival, so a successful docking proves that the level is
/// solvable and that physics and docking rules agree. Also drives the recorded gameplay demo.
class HohmannPilot {
public:
    explicit HohmannPilot(const Level& level);

    /// Controls for the next simulation step.
    [[nodiscard]] Input next(const Mission& mission);

    [[nodiscard]] double waitTime() const { return wait_; }
    [[nodiscard]] double transferTime() const { return transfer_; }

private:
    double wait_ = 0.0;
    double transfer_ = 0.0;
    double targetSpeed_ = 0.0;
    bool raise_ = true;
    bool burned_ = false;
};

/// Runs a whole mission with the pilot until it ends or `maxSeconds` have passed.
[[nodiscard]] Mission flyHohmann(const Level& level, double maxSeconds = 600.0);

} // namespace oc
