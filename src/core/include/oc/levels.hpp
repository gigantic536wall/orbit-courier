#pragma once

#include <vector>

#include "oc/mission.hpp"
#include "oc/world.hpp"

namespace oc {

/// Free-flight sandbox: one planet with a moon, the ship starts in a low circular orbit.
[[nodiscard]] World makeSandbox();

/// The ten campaign levels, from a first orbit raise to a slingshot across the system.
[[nodiscard]] std::vector<Level> campaign();

/// Gravitational parameter that gives surface gravity `g` on a body of radius `r`.
[[nodiscard]] constexpr double muForSurfaceGravity(double g, double r) { return g * r * r; }

/// Angular speed of a circular orbit with radius `r` around a body with parameter `mu`.
[[nodiscard]] double orbitalAngularSpeed(double mu, double r);

/// Velocity change of a Hohmann transfer between two circular orbits around the same body.
[[nodiscard]] double hohmannDeltaV(double mu, double r1, double r2);

} // namespace oc
