#pragma once

#include "piano_roll/types.hpp"

namespace piano_roll {

// Lightweight helper for integrating transport-driven playback with the
// piano roll. This mirrors the Python PlaybackIndicator tick advancement
// logic but leaves actual timing and transport state to the host.
//
// Typical usage pattern:
//   - Host owns a PlaybackState instance.
//   - Each frame, host calls advance(delta_seconds) when playing.
//   - Host then passes the resulting tick to PianoRollWidget::set_playhead.
//
// The free function advance_playback_ticks provides the same behaviour in
// a stateless form when a full PlaybackState is not needed.

// Stateless helper: compute the next playback tick given tempo, ticks-per-beat,
// and an optional loop range. This matches the core tick update and loop
// wrapping logic from the Python UnifiedPianoRoll.update_playback method.
inline Tick advance_playback_ticks(Tick current_position,
                                   double tempo_bpm,
                                   int ticks_per_beat,
                                   double delta_seconds,
                                   bool loop_enabled,
                                   Tick loop_start_tick,
                                   Tick loop_end_tick) noexcept {
    if (delta_seconds <= 0.0 || tempo_bpm <= 0.0 || ticks_per_beat <= 0) {
        return current_position;
    }

    const double ticks_per_second =
        (tempo_bpm * static_cast<double>(ticks_per_beat)) / 60.0;
    const double delta_ticks = ticks_per_second * delta_seconds;
    if (delta_ticks <= 0.0) {
        return current_position;
    }

    Tick new_pos =
        current_position + static_cast<Tick>(delta_ticks);
    if (new_pos < 0) {
        new_pos = 0;
    }

    if (loop_enabled && loop_end_tick > loop_start_tick) {
        // Follow the Python behaviour: when we step past the loop end, wrap
        // back into the loop by the overshoot amount.
        if (new_pos >= loop_end_tick) {
            const Tick overshoot =
                new_pos - loop_end_tick;
            new_pos = loop_start_tick + overshoot;
            if (new_pos < loop_start_tick) {
                new_pos = loop_start_tick;
            }
        }
    }

    return new_pos;
}

// Small stateful playback helper that keeps track of the current tick
// position, tempo, ticks-per-beat, and optional loop range. Hosts are
// expected to hold this alongside their transport state and call advance()
// from their main update loop.
struct PlaybackState {
    Tick position_ticks{0};
    double tempo_bpm{120.0};
    int ticks_per_beat{480};

    bool playing{false};

    bool loop_enabled{false};
    Tick loop_start_tick{0};
    Tick loop_end_tick{0};

    void set_tempo(double bpm) noexcept {
        if (bpm > 0.0) {
            tempo_bpm = bpm;
        }
    }

    void set_ticks_per_beat(int tpb) noexcept {
        if (tpb > 0) {
            ticks_per_beat = tpb;
        }
    }

    void set_position(Tick tick) noexcept {
        position_ticks = tick >= 0 ? tick : 0;
    }

    void set_loop_range(Tick start, Tick end) noexcept {
        if (end < start) {
            std::swap(start, end);
        }
        if (start < 0) {
            start = 0;
        }
        loop_start_tick = start;
        loop_end_tick = end;
        if (loop_end_tick < loop_start_tick) {
            loop_end_tick = loop_start_tick;
        }
    }

    void set_loop_enabled(bool enabled) noexcept {
        loop_enabled = enabled;
    }

    void play() noexcept { playing = true; }
    void pause() noexcept { playing = false; }

    // Advance the playback position by the given delta time (in seconds),
    // applying tempo and loop handling when enabled. Returns the new
    // position in ticks.
    Tick advance(double delta_seconds) noexcept {
        if (!playing) {
            return position_ticks;
        }
        position_ticks = advance_playback_ticks(
            position_ticks,
            tempo_bpm,
            ticks_per_beat,
            delta_seconds,
            loop_enabled,
            loop_start_tick,
            loop_end_tick);
        return position_ticks;
    }
};

}  // namespace piano_roll

