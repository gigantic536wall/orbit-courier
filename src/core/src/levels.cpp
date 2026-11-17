#include "oc/levels.hpp"

#include <cmath>
#include <utility>

namespace oc {

double orbitalAngularSpeed(double mu, double r) { return std::sqrt(mu / (r * r * r)); }

double hohmannDeltaV(double mu, double r1, double r2) {
    const double a = (r1 + r2) / 2.0;
    const double depart = std::abs(circularSpeed(mu, r1) * (std::sqrt(r2 / a) - 1.0));
    const double arrive = std::abs(circularSpeed(mu, r2) * (1.0 - std::sqrt(r1 / a)));
    return depart + arrive;
}

namespace {

enum class Direction { Prograde, Retrograde };

Body planet(std::string name, Vec2 center, double radius, double surfaceGravity) {
    return Body{.name = std::move(name),
                .radius = radius,
                .mu = muForSurfaceGravity(surfaceGravity, radius),
                .center = center};
}

/// A moon on a circular orbit around `bodies[parent]`.
Body moon(const std::vector<Body>& bodies, std::size_t parent, std::string name, double orbit, double phase,
          double radius, double surfaceGravity) {
    return Body{.name = std::move(name),
                .radius = radius,
                .mu = muForSurfaceGravity(surfaceGravity, radius),
                .parent = parent,
                .orbitRadius = orbit,
                .angularSpeed = orbitalAngularSpeed(bodies[parent].mu, orbit),
                .phase = phase};
}

Station station(const std::vector<Body>& bodies, std::size_t parent, std::string name, double orbit, double phase,
                Direction direction = Direction::Prograde) {
    const double speed = orbitalAngularSpeed(bodies[parent].mu, orbit);
    return Station{.name = std::move(name),
                   .parent = parent,
                   .orbitRadius = orbit,
                   .angularSpeed = direction == Direction::Prograde ? speed : -speed,
                   .phase = phase};
}

std::vector<Body> halcyon() { return {planet("Halcyon", {0.0, 0.0}, 120.0, 40.0)}; }

std::vector<Body> halcyonAndPip(double pipPhase) {
    auto bodies = halcyon();
    bodies.push_back(moon(bodies, 0, "Pip", 650.0, pipPhase, 35.0, 25.0));
    return bodies;
}

} // namespace

World makeSandbox() {
    auto bodies = halcyonAndPip(2.4);
    constexpr double startOrbit = 260.0;
    const double mu = bodies[0].mu;
    Ship ship{
        .position = {startOrbit, 0.0},
        .velocity = {0.0, circularSpeed(mu, startOrbit)},
        .angle = kPi / 2.0, // prograde
        .fuel = 1.0,
    };
    return World(std::move(bodies), ship);
}

std::vector<Level> campaign() {
    std::vector<Level> levels;

    {
        auto b = halcyon();
        auto s = std::vector{station(b, 0, "Halcyon Low", 260.0, 0.0), station(b, 0, "Ring Depot", 340.0, 0.9)};
        levels.push_back({.name = "First Delivery",
                          .briefing = "Ring Depot circles just above you. Burn prograde to raise your orbit, "
                                      "then match its speed to dock.",
                          .bodies = std::move(b),
                          .stations = std::move(s),
                          .fuel = 0.5,
                          .jobs = {{0, 1}},
                          .par = {.time = 60.0, .fuelLeft = 0.40}});
    }
    {
        auto b = halcyon();
        auto s = std::vector{station(b, 0, "Highport", 460.0, 0.0), station(b, 0, "Lowdock", 220.0, 2.5)};
        levels.push_back({.name = "Down the Well",
                          .briefing = "Lowdock is deep in the gravity well. Burn retrograde to fall, and brake "
                                      "before you arrive: low orbits are fast.",
                          .bodies = std::move(b),
                          .stations = std::move(s),
                          .fuel = 0.5,
                          .jobs = {{0, 1}},
                          .par = {.time = 70.0, .fuelLeft = 0.38}});
    }
    {
        auto b = halcyon();
        auto s = std::vector{station(b, 0, "Halcyon Low", 260.0, 0.0), station(b, 0, "Outer Ring", 420.0, 3.0)};
        levels.push_back({.name = "Round Trip",
                          .briefing = "Take a parcel to Outer Ring and bring the reply back home.",
                          .bodies = std::move(b),
                          .stations = std::move(s),
                          .fuel = 0.6,
                          .jobs = {{0, 1}, {1, 0}},
                          .par = {.time = 130.0, .fuelLeft = 0.40}});
    }
    {
        auto b = halcyonAndPip(1.2);
        auto s = std::vector{station(b, 0, "Halcyon Low", 260.0, 0.0), station(b, 1, "Pip Station", 80.0, 0.0)};
        levels.push_back({.name = "Moon Mail",
                          .briefing = "Pip has its own gravity. Reach the moon, let it capture you, then find the "
                                      "station in its orbit.",
                          .bodies = std::move(b),
                          .stations = std::move(s),
                          .fuel = 0.7,
                          .jobs = {{0, 1}},
                          .par = {.time = 130.0, .fuelLeft = 0.45}});
    }
    {
        auto b = halcyon();
        auto s = std::vector{station(b, 0, "Halcyon Low", 260.0, 0.0),
                             station(b, 0, "Contraflow", 320.0, kPi, Direction::Retrograde)};
        levels.push_back({.name = "Wrong Way",
                          .briefing = "Contraflow orbits backwards. You will have to kill your speed and build it "
                                      "up again in the other direction.",
                          .bodies = std::move(b),
                          .stations = std::move(s),
                          .fuel = 0.8,
                          .jobs = {{0, 1}},
                          .par = {.time = 90.0, .fuelLeft = 0.40}});
    }
    {
        auto b = halcyonAndPip(0.6);
        b.push_back(moon(b, 0, "Bramble", 1000.0, 3.6, 45.0, 22.0));
        auto s = std::vector{station(b, 1, "Pip Station", 80.0, 0.0), station(b, 2, "Bramble Yard", 100.0, 0.0)};
        levels.push_back({.name = "Two Moons",
                          .briefing = "Escape Pip, cross to the outer moon Bramble and dock at its yard.",
                          .bodies = std::move(b),
                          .stations = std::move(s),
                          .fuel = 0.8,
                          .jobs = {{0, 1}},
                          .par = {.time = 180.0, .fuelLeft = 0.40}});
    }
    {
        auto b = halcyon();
        auto s = std::vector{station(b, 0, "Halcyon Low", 260.0, 0.0), station(b, 0, "Skyhook", 520.0, 1.1)};
        levels.push_back({.name = "Express",
                          .briefing = "Urgent parcel for Skyhook. A slow transfer will not make par: "
                                      "burn hard and brake hard.",
                          .bodies = std::move(b),
                          .stations = std::move(s),
                          .fuel = 0.6,
                          .jobs = {{0, 1}},
                          .par = {.time = 40.0, .fuelLeft = 0.20}});
    }
    {
        std::vector<Body> b{planet("Castor", {-450.0, 0.0}, 90.0, 35.0), planet("Pollux", {450.0, 0.0}, 90.0, 35.0)};
        auto s = std::vector{station(b, 0, "Castor Dock", 190.0, kPi), station(b, 1, "Pollux Dock", 190.0, 0.0)};
        levels.push_back({.name = "Twin Anchors",
                          .briefing = "Two worlds pull at you at once. Cross between them without falling into "
                                      "either.",
                          .bodies = std::move(b),
                          .stations = std::move(s),
                          .fuel = 0.7,
                          .jobs = {{0, 1}},
                          .par = {.time = 150.0, .fuelLeft = 0.35}});
    }
    {
        auto b = halcyonAndPip(2.0);
        auto s = std::vector{station(b, 0, "Halcyon Low", 260.0, 0.0), station(b, 0, "Outer Ring", 420.0, 2.0),
                             station(b, 1, "Pip Station", 80.0, 0.0), station(b, 0, "Deep Relay", 900.0, 4.5)};
        levels.push_back({.name = "Courier Chain",
                          .briefing = "Three parcels, four stations. Plan each leg before you burn.",
                          .bodies = std::move(b),
                          .stations = std::move(s),
                          .fuel = 1.0,
                          .jobs = {{0, 1}, {1, 2}, {2, 3}},
                          .par = {.time = 330.0, .fuelLeft = 0.35},
                          .boundary = 2200.0});
    }
    {
        auto b = halcyonAndPip(0.4);
        auto s = std::vector{station(b, 0, "Halcyon Low", 260.0, 0.0), station(b, 0, "Far Beacon", 1300.0, kPi)};
        levels.push_back({.name = "Slingshot",
                          .briefing = "Far Beacon is at the edge of the system and your tank is small. "
                                      "A pass behind Pip can lend you speed.",
                          .bodies = std::move(b),
                          .stations = std::move(s),
                          .fuel = 0.25,
                          .jobs = {{0, 1}},
                          .par = {.time = 200.0, .fuelLeft = 0.12},
                          .boundary = 2200.0});
    }
    return levels;
}

} // namespace oc
