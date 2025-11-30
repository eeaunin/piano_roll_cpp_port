#pragma once

#include "piano_roll/types.hpp"

namespace piano_roll {

// High-level configuration for PianoRollWidget layout and musical defaults.
// This complements PianoRollRenderConfig (colours) and allows hosts to choose
// between basic layout presets without reaching into widget internals.
struct PianoRollConfig {
    // Layout / geometry
    double piano_key_width{180.0};      // Width of piano key strip + labels
    float ruler_height{24.0f};
    float top_padding{0.0f};
    float footer_height{0.0f};
    float note_label_width{180.0f};     // Left label column width

    // CC lane
    bool show_cc_lane{true};
    float cc_lane_height{120.0f};

    // Musical defaults
    int ticks_per_beat{480};
    int beats_per_measure{4};
    int default_clip_bars{4};           // Initial clip length in bars
    MidiKey initial_center_key{60};     // C4

    // Compact layout preset: narrower piano key area and CC lane height.
    static PianoRollConfig compact() noexcept {
        PianoRollConfig cfg;
        cfg.piano_key_width = 150.0;
        cfg.ruler_height = 22.0f;
        cfg.note_label_width = 150.0f;
        cfg.cc_lane_height = 90.0f;
        cfg.top_padding = 0.0f;
        cfg.footer_height = 0.0f;
        return cfg;
    }

    // Spacious layout preset: wider piano key area and taller CC lane,
    // roughly matching the defaults used elsewhere in this port.
    static PianoRollConfig spacious() noexcept {
        PianoRollConfig cfg;
        cfg.piano_key_width = 200.0;
        cfg.ruler_height = 24.0f;
        cfg.note_label_width = 200.0f;
        cfg.cc_lane_height = 140.0f;
        cfg.top_padding = 0.0f;
        cfg.footer_height = 0.0f;
        return cfg;
    }
};

}  // namespace piano_roll
