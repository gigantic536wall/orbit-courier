#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <vector>

#include <raylib.h>

#include "oc/vec2.hpp"

namespace game {

/// Purely visual particles (exhaust, debris, sparkles) in world coordinates. They do not affect the simulation.
class Particles {
public:
    void emit(oc::Vec2 position, oc::Vec2 velocity, float life, float size, Color color) {
        if (items_.size() < kMax) {
            items_.push_back({position, velocity, life, life, size, color});
        }
    }

    /// A burst of `count` particles flying outwards at up to `speed`.
    void burst(oc::Vec2 position, oc::Vec2 baseVelocity, int count, double speed, float life, Color color) {
        for (int i = 0; i < count; ++i) {
            const double angle = random() * oc::kTau;
            const double s = speed * (0.3 + 0.7 * random());
            emit(position, baseVelocity + oc::Vec2::fromAngle(angle) * s, life * static_cast<float>(0.5 + random()),
                 static_cast<float>(1.5 + 2.5 * random()), color);
        }
    }

    void update(float dt) {
        for (Item& p : items_) {
            p.position += p.velocity * static_cast<double>(dt);
            p.velocity = p.velocity * std::pow(0.4, static_cast<double>(dt)); // drag, purely cosmetic
            p.life -= dt;
        }
        std::erase_if(items_, [](const Item& p) { return p.life <= 0.0f; });
    }

    void draw(float zoom) const {
        for (const Item& p : items_) {
            const float k = p.life / p.maxLife;
            DrawCircleV({static_cast<float>(p.position.x), static_cast<float>(p.position.y)},
                        std::max(p.size * (0.4f + 0.6f * k), 0.8f / zoom), Fade(p.color, k));
        }
    }

    void clear() { items_.clear(); }

    /// Small xorshift generator: cosmetic randomness only.
    double random() {
        seed_ ^= seed_ << 13;
        seed_ ^= seed_ >> 17;
        seed_ ^= seed_ << 5;
        return static_cast<double>(seed_ % 100'000) / 100'000.0;
    }

private:
    struct Item {
        oc::Vec2 position;
        oc::Vec2 velocity;
        float life;
        float maxLife;
        float size;
        Color color;
    };
    static constexpr std::size_t kMax = 1500;
    std::vector<Item> items_;
    std::uint32_t seed_ = 0xC0FFEEu;
};

} // namespace game
