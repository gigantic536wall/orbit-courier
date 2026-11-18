#pragma once

// Shared look of the game: screen size, colours and small drawing helpers.

#include <cmath>
#include <cstdint>
#include <string>
#include <vector>

#include <raylib.h>

#include "oc/vec2.hpp"

namespace game {

inline constexpr int kWidth = 1280;
inline constexpr int kHeight = 720;

inline constexpr Color kSpace{7, 11, 20, 255};
inline constexpr Color kPlanet{56, 104, 160, 255};
inline constexpr Color kMoon{150, 150, 165, 255};
inline constexpr Color kShip{240, 240, 245, 255};
inline constexpr Color kFlame{255, 186, 73, 255};
inline constexpr Color kPickup{255, 186, 73, 255};
inline constexpr Color kDropoff{110, 231, 140, 255};
inline constexpr Color kPath{120, 220, 255, 255};
inline constexpr Color kDanger{255, 92, 92, 255};
inline constexpr Color kText{220, 226, 240, 255};
inline constexpr Color kMuted{140, 150, 170, 255};

inline Vector2 v2(oc::Vec2 v) { return {static_cast<float>(v.x), static_cast<float>(v.y)}; }
inline float f(double d) { return static_cast<float>(d); }

inline void textCentered(const char* text, int y, int size, Color color) {
    DrawText(text, (kWidth - MeasureText(text, size)) / 2, y, size, color);
}

/// Splits `text` into lines no wider than `width` pixels.
inline std::vector<std::string> wrap(const std::string& text, int size, int width) {
    std::vector<std::string> lines;
    std::string line;
    std::size_t start = 0;
    while (start < text.size()) {
        std::size_t end = text.find(' ', start);
        if (end == std::string::npos) {
            end = text.size();
        }
        const std::string word = text.substr(start, end - start);
        const std::string candidate = line.empty() ? word : line + " " + word;
        if (!line.empty() && MeasureText(candidate.c_str(), size) > width) {
            lines.push_back(line);
            line = word;
        } else {
            line = candidate;
        }
        start = end + 1;
    }
    if (!line.empty()) {
        lines.push_back(line);
    }
    return lines;
}

inline void drawStar(Vector2 c, float r, Color color) {
    DrawPoly(c, 5, r * 0.45f, -90.0f + 36.0f, color);
    for (int i = 0; i < 5; ++i) {
        const float a = (-90.0f + 72.0f * static_cast<float>(i)) * DEG2RAD;
        const float l = a - 36.0f * DEG2RAD;
        const float rr = a + 36.0f * DEG2RAD;
        const Vector2 tip{c.x + std::cos(a) * r, c.y + std::sin(a) * r};
        const Vector2 left{c.x + std::cos(l) * r * 0.42f, c.y + std::sin(l) * r * 0.42f};
        const Vector2 right{c.x + std::cos(rr) * r * 0.42f, c.y + std::sin(rr) * r * 0.42f};
        DrawTriangle(tip, left, right, color);
        DrawTriangle(tip, right, left, color);
    }
}

inline void drawStars(Vector2 c, int earned, float r, float gap) {
    for (int i = 0; i < 3; ++i) {
        const Vector2 p{c.x + (static_cast<float>(i) - 1.0f) * gap, c.y};
        drawStar(p, r, i < earned ? kFlame : Fade(kMuted, 0.35f));
    }
}

inline std::uint32_t nextRandom(std::uint32_t& state) {
    state ^= state << 13;
    state ^= state >> 17;
    state ^= state << 5;
    return state;
}

inline std::string seconds(double s) { return TextFormat("%.1f s", s); }

} // namespace game
