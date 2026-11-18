#pragma once

#include <array>
#include <cstddef>

#include <raylib.h>

namespace game {

enum class Cue { Click, Pickup, Deliver, TooFast, Crash, Lost, Complete, Count };

/// All sound effects are synthesized at start-up: no audio files, no licensing questions.
/// Does nothing if the audio device is unavailable (headless CI, blocked browser audio).
class Audio {
public:
    Audio();
    ~Audio();
    Audio(const Audio&) = delete;
    Audio& operator=(const Audio&) = delete;

    void play(Cue cue);
    /// Engine noise: 0 = silent, 1 = full thrust. Call every frame.
    void setThrust(float level, bool fine);

    void setEnabled(bool enabled);
    [[nodiscard]] bool enabled() const { return enabled_; }

private:
    bool ready_ = false;
    bool enabled_ = true;
    std::array<Sound, static_cast<std::size_t>(Cue::Count)> cues_{};
    Sound engine_{};
};

} // namespace game
