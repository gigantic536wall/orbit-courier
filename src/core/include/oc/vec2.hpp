#pragma once

#include <cmath>

namespace oc {

/// 2D vector in world units. Double precision keeps long orbits stable.
struct Vec2 {
    double x = 0.0;
    double y = 0.0;

    constexpr Vec2 operator+(Vec2 o) const { return {x + o.x, y + o.y}; }
    constexpr Vec2 operator-(Vec2 o) const { return {x - o.x, y - o.y}; }
    constexpr Vec2 operator-() const { return {-x, -y}; }
    constexpr Vec2 operator*(double s) const { return {x * s, y * s}; }
    constexpr Vec2 operator/(double s) const { return {x / s, y / s}; }
    constexpr Vec2& operator+=(Vec2 o) {
        x += o.x;
        y += o.y;
        return *this;
    }
    constexpr Vec2& operator-=(Vec2 o) {
        x -= o.x;
        y -= o.y;
        return *this;
    }
    constexpr bool operator==(const Vec2&) const = default;

    [[nodiscard]] constexpr double dot(Vec2 o) const { return x * o.x + y * o.y; }
    [[nodiscard]] constexpr double cross(Vec2 o) const { return x * o.y - y * o.x; }
    [[nodiscard]] constexpr double lengthSquared() const { return dot(*this); }
    [[nodiscard]] double length() const { return std::sqrt(lengthSquared()); }
    [[nodiscard]] Vec2 normalized() const {
        const double len = length();
        return len > 0.0 ? *this / len : Vec2{};
    }

    /// Unit vector pointing at `angle` radians (0 = +x, counter-clockwise in a y-up system).
    [[nodiscard]] static Vec2 fromAngle(double angle) { return {std::cos(angle), std::sin(angle)}; }
};

constexpr Vec2 operator*(double s, Vec2 v) { return v * s; }

inline constexpr double kPi = 3.14159265358979323846;
inline constexpr double kTau = 2.0 * kPi;

} // namespace oc
