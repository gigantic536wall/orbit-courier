#include "audio.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <initializer_list>

namespace game {

namespace {

constexpr int kRate = 44100;
constexpr float kTau = 6.28318530718f;

/// Renders `seconds` of audio from a sample function f(t) -> [-1, 1] into a raylib sound.
template <typename F> Sound synth(float seconds, F sample) {
    const auto frames = static_cast<unsigned int>(seconds * kRate);
    auto* data = static_cast<short*>(MemAlloc(frames * sizeof(short)));
    for (unsigned int i = 0; i < frames; ++i) {
        const float t = static_cast<float>(i) / kRate;
        data[i] = static_cast<short>(std::clamp(sample(t), -1.0f, 1.0f) * 32767.0f * 0.8f);
    }
    Wave wave{.frameCount = frames, .sampleRate = kRate, .sampleSize = 16, .channels = 1, .data = data};
    Sound sound = LoadSoundFromWave(wave);
    UnloadWave(wave);
    return sound;
}

float envelope(float t, float attack, float decay) { return std::min(t / attack, 1.0f) * std::exp(-t / decay); }

float tone(float t, float freq) { return std::sin(kTau * freq * t); }

/// Deterministic white noise.
struct Noise {
    std::uint32_t state = 0x12345678u;
    float next() {
        state = state * 1664525u + 1013904223u;
        return static_cast<float>(state >> 8) / static_cast<float>(1u << 23) - 1.0f;
    }
};

/// A short melody: each note starts at `start` and rings with a bell-like decay.
float notes(float t, std::initializer_list<std::pair<float, float>> score, float decay) {
    float sum = 0.0f;
    for (const auto& [start, freq] : score) {
        if (t >= start) {
            const float local = t - start;
            sum += (tone(local, freq) + 0.3f * tone(local, freq * 2.0f)) * envelope(local, 0.004f, decay);
        }
    }
    return sum * 0.5f;
}

} // namespace

Audio::Audio() {
    if (!IsAudioDeviceReady()) {
        return;
    }
    ready_ = true;

    cues_[static_cast<std::size_t>(Cue::Click)] = synth(0.06f, [](float t) { return tone(t, 1400.0f) * envelope(t, 0.001f, 0.012f) * 0.5f; });
    cues_[static_cast<std::size_t>(Cue::Pickup)] =
        synth(0.45f, [](float t) { return notes(t, {{0.0f, 880.0f}, {0.08f, 1320.0f}}, 0.12f); });
    cues_[static_cast<std::size_t>(Cue::Deliver)] =
        synth(0.8f, [](float t) { return notes(t, {{0.0f, 660.0f}, {0.09f, 880.0f}, {0.18f, 1320.0f}}, 0.18f); });
    cues_[static_cast<std::size_t>(Cue::Complete)] = synth(1.6f, [](float t) {
        return notes(t, {{0.0f, 523.25f}, {0.12f, 659.25f}, {0.24f, 783.99f}, {0.36f, 1046.5f}, {0.36f, 523.25f}}, 0.45f);
    });
    cues_[static_cast<std::size_t>(Cue::TooFast)] = synth(0.35f, [](float t) {
        const float buzz = std::tanh(3.0f * tone(t, 150.0f + 10.0f * tone(t, 30.0f)));
        return buzz * envelope(t, 0.005f, 0.15f) * 0.6f;
    });
    cues_[static_cast<std::size_t>(Cue::Lost)] = synth(0.9f, [](float t) {
        // Falling glide from 600 Hz: the phase is the integral of f(t) = 600 * e^(-1.4 t).
        const float phase = 600.0f * (1.0f - std::exp(-1.4f * t)) / 1.4f;
        return std::sin(kTau * phase) * envelope(t, 0.01f, 0.4f) * 0.6f;
    });
    {
        Noise noise;
        float low = 0.0f;
        cues_[static_cast<std::size_t>(Cue::Crash)] = synth(1.3f, [&](float t) {
            low += (noise.next() - low) * 0.08f; // one-pole low-pass: a dull rumble
            const float thump = tone(t, 55.0f) * envelope(t, 0.002f, 0.18f);
            return (low * 2.5f * envelope(t, 0.003f, 0.35f) + thump) * 0.8f;
        });
    }
    {
        // Engine: low-passed noise, 2 s long, restarted while the engine burns.
        Noise noise;
        float low = 0.0f;
        engine_ = synth(2.0f, [&](float t) {
            low += (noise.next() - low) * 0.05f;
            const float fade = std::min({1.0f, t / 0.02f, (2.0f - t) / 0.02f}); // no clicks at the loop seam
            return low * 3.0f * fade;
        });
    }
}

Audio::~Audio() {
    if (!ready_) {
        return;
    }
    for (Sound& s : cues_) {
        UnloadSound(s);
    }
    UnloadSound(engine_);
}

void Audio::play(Cue cue) {
    if (ready_ && enabled_) {
        PlaySound(cues_[static_cast<std::size_t>(cue)]);
    }
}

void Audio::setThrust(float level, bool fine) {
    if (!ready_) {
        return;
    }
    if (!enabled_ || level <= 0.0f) {
        if (IsSoundPlaying(engine_)) {
            StopSound(engine_);
        }
        return;
    }
    SetSoundVolume(engine_, std::clamp(level, 0.0f, 1.0f) * (fine ? 0.35f : 0.7f));
    SetSoundPitch(engine_, fine ? 1.4f : 1.0f);
    if (!IsSoundPlaying(engine_)) {
        PlaySound(engine_);
    }
}

void Audio::setEnabled(bool enabled) {
    enabled_ = enabled;
    if (!enabled_ && ready_ && IsSoundPlaying(engine_)) {
        StopSound(engine_);
    }
}

} // namespace game
