#include "oc/levels.hpp"

#include <cmath>

namespace oc {

double orbitalAngularSpeed(double mu, double r) { return std::sqrt(mu / (r * r * r)); }

World makeSandbox() {
    constexpr double planetRadius = 120.0;
    const double planetMu = muForSurfaceGravity(40.0, planetRadius);

    constexpr double moonOrbit = 650.0;
    std::vector<Body> bodies{
        Body{.name = "Halcyon", .radius = planetRadius, .mu = planetMu, .center = {0.0, 0.0}},
        Body{.name = "Pip",
             .radius = 35.0,
             .mu = muForSurfaceGravity(25.0, 35.0),
             .parent = 0,
             .orbitRadius = moonOrbit,
             .angularSpeed = orbitalAngularSpeed(planetMu, moonOrbit),
             .phase = 2.4},
    };

    constexpr double startOrbit = 260.0;
    Ship ship{
        .position = {startOrbit, 0.0},
        .velocity = {0.0, circularSpeed(planetMu, startOrbit)},
        .angle = kPi / 2.0, // prograde
        .fuel = 1.0,
    };
    return World(std::move(bodies), ship);
}

} // namespace oc
