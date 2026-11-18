#include "oc/autopilot.hpp"

#include <cmath>

namespace oc {

HohmannPilot::HohmannPilot(const Level& level) {
    const Job job = level.jobs.at(0);
    const Station& from = level.stations.at(job.from);
    const Station& to = level.stations.at(job.to);
    const double mu = level.bodies.at(from.parent).mu;
    const double r1 = from.orbitRadius;
    const double r2 = to.orbitRadius;
    const double a = (r1 + r2) / 2.0;
    transfer_ = kPi * std::sqrt(a * a * a / mu);

    // The target must be half a turn ahead of the departure point when the ship arrives.
    const double w1 = from.angularSpeed;
    const double w2 = to.angularSpeed;
    const double synodic = kTau / std::abs(w1 - w2);
    const double wait = (to.phase + w2 * transfer_ - kPi - from.phase) / (w1 - w2);
    wait_ = std::fmod(std::fmod(wait, synodic) + synodic, synodic);

    targetSpeed_ = circularSpeed(mu, r1) * std::sqrt(r2 / a);
    raise_ = r2 > r1;
}

Input HohmannPilot::next(const Mission& mission) {
    const Ship& s = mission.world().ship();
    const double prograde = std::atan2(s.velocity.y, s.velocity.x);
    const double desired = raise_ ? prograde : prograde + kPi;
    const double error = std::remainder(desired - s.angle, kTau);
    Input input{.turn = std::abs(error) > 0.01 ? (error > 0 ? 1 : -1) : 0};
    if (!burned_ && mission.world().time() >= wait_) {
        const double speed = s.velocity.length();
        input.thrust = raise_ ? speed < targetSpeed_ : speed > targetSpeed_;
        burned_ = !input.thrust;
    }
    return input;
}

Mission flyHohmann(const Level& level, double maxSeconds) {
    Mission mission(level);
    HohmannPilot pilot(level);
    const auto steps = static_cast<int>(maxSeconds / World::kStep);
    for (int i = 0; i < steps && mission.state() == MissionState::Flying; ++i) {
        mission.step(pilot.next(mission));
    }
    return mission;
}

} // namespace oc
