#include "oc/world.hpp"

#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace oc {

World::World(std::vector<Body> bodies, Ship ship, ShipSpec spec)
    : bodies_(std::move(bodies)), ship_(ship), spec_(spec) {
    for (std::size_t i = 0; i < bodies_.size(); ++i) {
        if (bodies_[i].parent && *bodies_[i].parent >= i) {
            throw std::invalid_argument("A body's parent must be listed before it: " + bodies_[i].name);
        }
    }
}

Vec2 World::bodyPosition(std::size_t index, double t) const {
    const Body& b = bodies_.at(index);
    if (!b.parent) {
        return b.center;
    }
    return bodyPosition(*b.parent, t) + Vec2::fromAngle(b.phase + b.angularSpeed * t) * b.orbitRadius;
}

Vec2 World::gravityAt(Vec2 p, double t) const {
    Vec2 a;
    for (std::size_t i = 0; i < bodies_.size(); ++i) {
        const Vec2 d = bodyPosition(i, t) - p;
        // Inside a body gravity would explode; the ship crashes long before, so clamp for safety.
        const double r2 = std::max(d.lengthSquared(), bodies_[i].radius * bodies_[i].radius);
        a += d * (bodies_[i].mu / (r2 * std::sqrt(r2)));
    }
    return a;
}

std::optional<std::size_t> World::collidingBody(Vec2 p, double t) const {
    for (std::size_t i = 0; i < bodies_.size(); ++i) {
        const double reach = bodies_[i].radius + spec_.radius;
        if ((bodyPosition(i, t) - p).lengthSquared() < reach * reach) {
            return i;
        }
    }
    return std::nullopt;
}

Vec2 World::acceleration(Vec2 p, double t, bool engineOn) const {
    Vec2 a = gravityAt(p, t);
    if (engineOn) {
        a += Vec2::fromAngle(ship_.angle) * spec_.thrust;
    }
    return a;
}

void World::step(const Input& input) {
    if (status_ != Status::Flying) {
        return;
    }

    constexpr double dt = kStep;
    ship_.angle += std::clamp(input.turn, -1, 1) * spec_.turnRate * dt;

    thrusting_ = input.thrust && ship_.fuel > 0.0;
    if (thrusting_) {
        ship_.fuel = std::max(0.0, ship_.fuel - spec_.fuelPerSecond * dt);
    }

    // Velocity Verlet (kick-drift-kick): symplectic, so orbits keep their energy over thousands of laps.
    ship_.velocity += acceleration(ship_.position, time_, thrusting_) * (dt / 2.0);
    ship_.position += ship_.velocity * dt;
    time_ += dt;
    ship_.velocity += acceleration(ship_.position, time_, thrusting_) * (dt / 2.0);

    if (auto hit = collidingBody(ship_.position, time_)) {
        status_ = Status::Crashed;
        crashedInto_ = hit;
        thrusting_ = false;
    }
}

std::vector<Vec2> World::predictTrajectory(int steps, int stride) const {
    World ghost = *this;
    std::vector<Vec2> points;
    points.reserve(static_cast<std::size_t>(std::max(steps, 0)));
    for (int i = 0; i < steps && ghost.status_ == Status::Flying; ++i) {
        for (int s = 0; s < stride && ghost.status_ == Status::Flying; ++s) {
            ghost.step(Input{});
        }
        points.push_back(ghost.ship_.position);
    }
    return points;
}

double circularSpeed(double mu, double r) { return std::sqrt(mu / r); }

} // namespace oc
