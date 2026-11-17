#pragma once

#include <optional>
#include <string>
#include <vector>

#include "oc/vec2.hpp"

namespace oc {

/// A planet or moon. Bodies move "on rails" (analytic circular orbits), so the level is fully
/// deterministic and only the ship needs numerical integration.
struct Body {
    std::string name;
    double radius = 0.0;
    double mu = 0.0; ///< Gravitational parameter G*M (world units^3 / s^2).

    /// Fixed position when `parent` is empty, otherwise the orbit around the parent body.
    Vec2 center;
    std::optional<std::size_t> parent;
    double orbitRadius = 0.0;
    double angularSpeed = 0.0; ///< rad/s, positive = counter-clockwise.
    double phase = 0.0;        ///< Angle at t = 0.
};

/// A delivery station on a circular orbit around a body. Stations have no gravity and cannot be hit.
struct Station {
    std::string name;
    std::size_t parent = 0;
    double orbitRadius = 0.0;
    double angularSpeed = 0.0;
    double phase = 0.0;
};

struct Ship {
    Vec2 position;
    Vec2 velocity;
    double angle = 0.0; ///< Heading in radians.
    double fuel = 1.0;  ///< 0..1
};

struct Input {
    int turn = 0;        ///< -1 = clockwise, 0 = none, +1 = counter-clockwise
    bool thrust = false; ///< Main engine on
    bool fine = false;   ///< Low-power thrust for precise docking
};

enum class Status { Flying, Crashed };

/// Tunable ship characteristics.
struct ShipSpec {
    double thrust = 25.0;        ///< Engine acceleration (world units / s^2).
    double fineFactor = 0.3;     ///< Thrust and fuel use in fine mode.
    double turnRate = 3.2;       ///< rad/s
    double fuelPerSecond = 0.04; ///< Fraction of the tank used per second of full thrust.
    double radius = 6.0;         ///< Collision radius.

    /// Total velocity change a full tank can give.
    [[nodiscard]] double deltaVPerTank() const { return thrust / fuelPerSecond; }
};

/// Closest point between the ship's coasting path and a station.
struct Approach {
    double distance = 0.0;
    double relativeSpeed = 0.0;
    double time = 0.0; ///< Seconds from now.
    Vec2 shipPosition;
    Vec2 stationPosition;
};

class World {
public:
    static constexpr double kStep = 1.0 / 120.0; ///< Fixed simulation step in seconds.

    World(std::vector<Body> bodies, Ship ship, ShipSpec spec = {});
    World(std::vector<Body> bodies, std::vector<Station> stations, Ship ship, ShipSpec spec = {});

    /// Advances the simulation by exactly one fixed step.
    void step(const Input& input);

    /// Position of body `index` at time `t` (follows parents recursively).
    [[nodiscard]] Vec2 bodyPosition(std::size_t index, double t) const;
    [[nodiscard]] Vec2 bodyVelocity(std::size_t index, double t) const;
    [[nodiscard]] Vec2 stationPosition(std::size_t index, double t) const;
    [[nodiscard]] Vec2 stationVelocity(std::size_t index, double t) const;

    /// Gravitational acceleration at point `p` at time `t`, summed over all bodies.
    [[nodiscard]] Vec2 gravityAt(Vec2 p, double t) const;
    /// Index of the body the ship has hit, if any.
    [[nodiscard]] std::optional<std::size_t> collidingBody(Vec2 p, double t) const;

    /// Path the ship will follow if the engine stays off: `steps` points, `stride` simulation
    /// steps apart. Stops early at a collision.
    [[nodiscard]] std::vector<Vec2> predictTrajectory(int steps, int stride = 4) const;
    /// Closest approach to station `index` within `seconds` of coasting.
    [[nodiscard]] Approach closestApproach(std::size_t index, double seconds) const;

    /// Docking: the ship takes over the station's velocity.
    void matchVelocity(Vec2 velocity) { ship_.velocity = velocity; }

    [[nodiscard]] const std::vector<Body>& bodies() const { return bodies_; }
    [[nodiscard]] const std::vector<Station>& stations() const { return stations_; }
    [[nodiscard]] const Ship& ship() const { return ship_; }
    [[nodiscard]] const ShipSpec& spec() const { return spec_; }
    [[nodiscard]] double time() const { return time_; }
    [[nodiscard]] Status status() const { return status_; }
    [[nodiscard]] bool thrusting() const { return thrusting_; }
    [[nodiscard]] std::optional<std::size_t> crashedInto() const { return crashedInto_; }

private:
    [[nodiscard]] Vec2 acceleration(Vec2 p, double t, double engine) const;

    std::vector<Body> bodies_;
    std::vector<Station> stations_;
    Ship ship_;
    ShipSpec spec_;
    double time_ = 0.0;
    Status status_ = Status::Flying;
    bool thrusting_ = false;
    std::optional<std::size_t> crashedInto_;
};

/// Speed for a circular orbit of radius `r` around a body with gravitational parameter `mu`.
[[nodiscard]] double circularSpeed(double mu, double r);

} // namespace oc
