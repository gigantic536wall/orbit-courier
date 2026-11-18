#pragma once

#include <optional>
#include <string>
#include <vector>

#include "oc/mission.hpp"

namespace oc {

/// Best results across all attempts of one level.
struct LevelRecord {
    int stars = 0;
    int score = 0;
    double time = 0.0; ///< Fastest completion.

    bool operator==(const LevelRecord&) const = default;
};

/// The player's campaign progress and settings, stored as a small versioned text file.
class Progress {
public:
    explicit Progress(std::size_t levelCount);

    /// Merges a finished run into the records. Returns true if anything improved.
    bool record(std::size_t level, const Result& result);

    [[nodiscard]] const std::optional<LevelRecord>& best(std::size_t level) const { return records_.at(level); }
    /// The first level is always open; every other level opens when the previous one is completed.
    [[nodiscard]] bool unlocked(std::size_t level) const;
    [[nodiscard]] int totalStars() const;
    [[nodiscard]] std::size_t levelCount() const { return records_.size(); }
    /// First level without three stars (or the last level), a good default selection for the menu.
    [[nodiscard]] std::size_t suggestedLevel() const;

    bool soundOn = true;

    [[nodiscard]] std::string serialize() const;
    /// Tolerant parser: unknown lines, bad numbers and out-of-range levels are ignored, so a damaged or
    /// older save never crashes the game.
    [[nodiscard]] static Progress parse(const std::string& text, std::size_t levelCount);

private:
    std::vector<std::optional<LevelRecord>> records_;
};

} // namespace oc
