#include "oc/world.hpp"

#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace oc {

namespace {

/// Velocity of a point on a circular orbit: angular speed times the perpendicular radius vector.
Vec2 orbitVelocity(double radius, double angularSpeed, double angle) {
    return Vec2{-std::sin(angle), std::cos(angle)} * (radius * angularSpeed);
}

} // namespace

World::World(std::vector<Body> bodies, Ship ship, ShipSpec spec) : World(std::move(bodies), {}, ship, spec) {}

World::World(std::vector<Body> bodies, std::vector<Station> stations, Ship ship, ShipSpec spec)
    : bodies_(std::move(bodies)), stations_(std::move(stations)), ship_(ship), spec_(spec) {
    for (std::size_t i = 0; i < bodies_.size(); ++i) {
        if (bodies_[i].parent && *bodies_[i].parent >= i) {
            throw std::invalid_argument("A body's parent must be listed before it: " + bodies_[i].name);
        }
    }
    for (const Station& s : stations_) {
        if (s.parent >= bodies_.size()) {
            throw std::invalid_argument("Station orbits a body that does not exist: " + s.name);
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

Vec2 World::bodyVelocity(std::size_t index, double t) const {
    const Body& b = bodies_.at(index);
    if (!b.parent) {
        return {};
    }
    return bodyVelocity(*b.parent, t) + orbitVelocity(b.orbitRadius, b.angularSpeed, b.phase + b.angularSpeed * t);
}

Vec2 World::stationPosition(std::size_t index, double t) const {
    const Station& s = stations_.at(index);
    return bodyPosition(s.parent, t) + Vec2::fromAngle(s.phase + s.angularSpeed * t) * s.orbitRadius;
}

Vec2 World::stationVelocity(std::size_t index, double t) const {
    const Station& s = stations_.at(index);
    return bodyVelocity(s.parent, t) + orbitVelocity(s.orbitRadius, s.angularSpeed, s.phase + s.angularSpeed * t);
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

Vec2 World::acceleration(Vec2 p, double t, double engine) const {
    return gravityAt(p, t) + Vec2::fromAngle(ship_.angle) * (spec_.thrust * engine);
}

void World::step(const Input& input) {
    if (status_ != Status::Flying) {
        return;
    }

    constexpr double dt = kStep;
    ship_.angle += std::clamp(input.turn, -1, 1) * spec_.turnRate * dt;

    thrusting_ = input.thrust && ship_.fuel > 0.0;
    const double engine = thrusting_ ? (input.fine ? spec_.fineFactor : 1.0) : 0.0;
    ship_.fuel = std::max(0.0, ship_.fuel - spec_.fuelPerSecond * engine * dt);

    // Velocity Verlet (kick-drift-kick): symplectic, so orbits keep their energy over thousands of laps.
    ship_.velocity += acceleration(ship_.position, time_, engine) * (dt / 2.0);
    ship_.position += ship_.velocity * dt;
    time_ += dt;
    ship_.velocity += acceleration(ship_.position, time_, engine) * (dt / 2.0);

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

Approach World::closestApproach(std::size_t index, double seconds) const {
    World ghost = *this;
    auto measure = [&] {
        const Vec2 station = ghost.stationPosition(index, ghost.time_);
        return Approach{
            .distance = (ghost.ship_.position - station).length(),
            .relativeSpeed = (ghost.ship_.velocity - ghost.stationVelocity(index, ghost.time_)).length(),
            .time = ghost.time_ - time_,
            .shipPosition = ghost.ship_.position,
            .stationPosition = station,
        };
    };

    Approach best = measure();
    const auto steps = static_cast<int>(seconds / kStep);
    for (int i = 0; i < steps && ghost.status_ == Status::Flying; ++i) {
        ghost.step(Input{});
        if (const Approach now = measure(); now.distance < best.distance) {
            best = now;
        }
    }
    return best;
}

double circularSpeed(double mu, double r) { return std::sqrt(mu / r); }

} // namespace oc
