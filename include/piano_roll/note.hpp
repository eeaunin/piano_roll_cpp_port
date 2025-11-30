#pragma once

#include "piano_roll/types.hpp"

#include <stdexcept>

namespace piano_roll {

// Represents a single musical note in the piano roll.
struct Note {
    NoteId id{0};          // Assigned by NoteManager, 0 means "unassigned"
    Tick tick{0};          // Start time in ticks
    Duration duration{0};  // Length in ticks (must be > 0)
    MidiKey key{60};       // MIDI note number (0-127, default to middle C)
    Velocity velocity{100};
    Channel channel{0};
    bool selected{false};

    Note() = default;

    Note(Tick tick_value,
         Duration duration_value,
         MidiKey key_value,
         Velocity velocity_value = 100,
         Channel channel_value = 0,
         bool selected_value = false,
         NoteId id_value = 0)
        : id(id_value),
          tick(tick_value),
          duration(duration_value),
          key(key_value),
          velocity(velocity_value),
          channel(channel_value),
          selected(selected_value) {
        validate();
    }

    Tick end_tick() const noexcept {
        return tick + duration;
    }

    bool overlaps(const Note& other) const noexcept {
        if (key != other.key) {
            return false;
        }
        return tick < other.end_tick() && other.tick < end_tick();
    }

    bool contains_tick(Tick tick_value) const noexcept {
        return tick <= tick_value && tick_value < end_tick();
    }

    void move_to(Tick new_tick, MidiKey new_key) {
        if (new_tick < 0) {
            throw std::invalid_argument("Note tick must be non-negative");
        }
        if (new_key < 0 || new_key > 127) {
            throw std::invalid_argument("MIDI key must be in range 0-127");
        }
        tick = new_tick;
        key = new_key;
    }

    void move_by(Tick delta_tick, int key_delta) {
        Tick new_tick = tick + delta_tick;
        if (new_tick < 0) {
            new_tick = 0;
        }
        MidiKey new_key = key + key_delta;
        if (new_key < 0) {
            new_key = 0;
        } else if (new_key > 127) {
            new_key = 127;
        }
        move_to(new_tick, new_key);
    }

    void resize_to(Duration new_duration) {
        if (new_duration <= 0) {
            throw std::invalid_argument("Note duration must be positive");
        }
        duration = new_duration;
    }

    void resize_by(Duration delta_duration) {
        Duration new_duration = duration + delta_duration;
        if (new_duration <= 0) {
            new_duration = 1;
        }
        resize_to(new_duration);
    }

private:
    void validate() const {
        if (tick < 0) {
            throw std::invalid_argument("Note tick must be non-negative");
        }
        if (duration <= 0) {
            throw std::invalid_argument("Note duration must be positive");
        }
        if (key < 0 || key > 127) {
            throw std::invalid_argument("MIDI key must be in range 0-127");
        }
        if (velocity < 0 || velocity > 127) {
            throw std::invalid_argument("Velocity must be in range 0-127");
        }
        if (channel < 0 || channel > 15) {
            throw std::invalid_argument("Channel must be in range 0-15");
        }
    }
};

}  // namespace piano_roll

