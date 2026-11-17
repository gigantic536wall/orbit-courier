#pragma once

#include "oc/world.hpp"

namespace oc {

/// Free-flight sandbox: one planet with a moon, the ship starts in a low circular orbit.
[[nodiscard]] World makeSandbox();

/// Gravitational parameter that gives surface gravity `g` on a body of radius `r`.
[[nodiscard]] constexpr double muForSurfaceGravity(double g, double r) { return g * r * r; }

/// Angular speed of a circular orbit with radius `r` around a body with parameter `mu`.
[[nodiscard]] double orbitalAngularSpeed(double mu, double r);

} // namespace oc
