#include "piano_roll/grid_snap.hpp"

#include <algorithm>
#include <limits>

namespace piano_roll {

GridSnapSystem::GridSnapSystem(int ticks_per_beat)
    : ticks_per_beat_(ticks_per_beat) {
    initialise_default_divisions();
    // Default to quarter notes for both snap and grid.
    const SnapDivision* quarter = find_division("1/4");
    if (quarter != nullptr) {
        snap_division_ = *quarter;
        grid_division_ = *quarter;
    }
}

void GridSnapSystem::set_ticks_per_beat(int ticks_per_beat) noexcept {
    if (ticks_per_beat <= 0) {
        return;
    }
    ticks_per_beat_ = ticks_per_beat;
}

void GridSnapSystem::set_beats_per_measure(int beats_per_measure) noexcept {
    if (beats_per_measure <= 0) {
        return;
    }
    beats_per_measure_ = beats_per_measure;
}

bool GridSnapSystem::set_snap_division(const std::string& label) {
    const SnapDivision* division = find_division(label);
    if (!division) {
        return false;
    }
    snap_division_ = *division;
    return true;
}

bool GridSnapSystem::set_grid_division(const std::string& label) {
    const SnapDivision* division = find_division(label);
    if (!division) {
        return false;
    }
    grid_division_ = *division;
    return true;
}

void GridSnapSystem::cycle_snap_division(bool forward) {
    if (divisions_.empty()) {
        return;
    }

    auto it = std::find_if(divisions_.begin(), divisions_.end(),
                           [this](const SnapDivision& d) {
                               return d.label == snap_division_.label;
                           });

    std::size_t index = 0;
    if (it != divisions_.end()) {
        index = static_cast<std::size_t>(std::distance(divisions_.begin(), it));
    }

    if (forward) {
        index = (index + 1) % divisions_.size();
    } else {
        index = (index + divisions_.size() - 1) % divisions_.size();
    }

    snap_division_ = divisions_[index];
}

const SnapDivision& GridSnapSystem::adaptive_division(double pixels_per_beat,
                                                      bool for_grid) const {
    // These thresholds mirror the Python implementation conceptually.
    const double min_grid_spacing = 10.0;
    const double ideal_grid_spacing = 30.0;
    const double max_grid_spacing = 100.0;

    const SnapDivision* best = nullptr;
    double best_score = std::numeric_limits<double>::infinity();

    for (const SnapDivision& division : divisions_) {
        double beats_per_division =
            static_cast<double>(division.ticks) /
            static_cast<double>(ticks_per_beat_);
        double pixels_per_division = beats_per_division * pixels_per_beat;

        if (for_grid) {
            if (pixels_per_division < min_grid_spacing ||
                pixels_per_division > max_grid_spacing) {
                continue;
            }
            double distance =
                std::abs(pixels_per_division - ideal_grid_spacing);
            if (distance < best_score) {
                best_score = distance;
                best = &division;
            }
        } else {
            if (pixels_per_division < min_grid_spacing) {
                continue;
            }
            // For snapping we prefer finer resolutions when available.
            double score = -pixels_per_division;
            if (score < best_score) {
                best_score = score;
                best = &division;
            }
        }
    }

    if (!best) {
        // Fallback to quarter notes.
        const SnapDivision* quarter = find_division("1/4");
        if (quarter) {
            return *quarter;
        }
        // If quarters are missing, fall back to the first defined division.
        return divisions_.front();
    }

    return *best;
}

Tick GridSnapSystem::snap_tick(Tick tick,
                               SnapMode mode_override) const {
    SnapMode effective_mode =
        (mode_override == SnapMode::Adaptive) ? snap_mode_ : mode_override;

    if (effective_mode == SnapMode::Off) {
        return tick;
    }

    Tick snap_size = snap_division_.ticks;
    if (snap_size <= 0) {
        return tick;
    }

    double value = static_cast<double>(tick) /
                   static_cast<double>(snap_size);
    Tick snapped = static_cast<Tick>(std::llround(value)) * snap_size;
    return snapped;
}

Tick GridSnapSystem::snap_tick_floor(Tick tick) const {
    if (snap_mode_ == SnapMode::Off) {
        return tick;
    }
    Tick snap_size = snap_division_.ticks;
    if (snap_size <= 0) {
        return tick;
    }
    if (tick < 0) {
        return 0;
    }
    return (tick / snap_size) * snap_size;
}

Tick GridSnapSystem::snap_tick_ceil(Tick tick) const {
    if (snap_mode_ == SnapMode::Off) {
        return tick;
    }
    Tick snap_size = snap_division_.ticks;
    if (snap_size <= 0) {
        return tick;
    }
    if (tick < 0) {
        return 0;
    }
    return ((tick + snap_size - 1) / snap_size) * snap_size;
}

std::pair<Tick, bool> GridSnapSystem::magnetic_snap(
    Tick tick, double pixels_per_beat, double magnetic_range_pixels) const {
    if (snap_mode_ == SnapMode::Off) {
        return {tick, false};
    }

    Tick snap_size{};
    if (snap_mode_ == SnapMode::Adaptive) {
        const SnapDivision& div =
            adaptive_division(pixels_per_beat, /*for_grid=*/false);
        snap_size = div.ticks;
    } else {
        snap_size = snap_division_.ticks;
    }
    if (snap_size <= 0) {
        return {tick, false};
    }

    double division = static_cast<double>(tick) /
                      static_cast<double>(snap_size);
    Tick nearest_grid =
        static_cast<Tick>(std::llround(division)) * snap_size;

    Tick tick_difference = std::abs(tick - nearest_grid);
    double beats_difference =
        static_cast<double>(tick_difference) /
        static_cast<double>(ticks_per_beat_);
    double pixels_difference = beats_difference * pixels_per_beat;

    if (pixels_difference <= magnetic_range_pixels) {
        return {nearest_grid, true};
    }

    return {tick, false};
}

std::vector<GridLine> GridSnapSystem::grid_lines(Tick start_tick,
                                                 Tick end_tick,
                                                 double pixels_per_beat) const {
    std::vector<GridLine> lines;
    if (start_tick >= end_tick) {
        return lines;
    }

    const SnapDivision& division = adaptive_division(pixels_per_beat, true);
    Tick grid_size = division.ticks;
    if (grid_size <= 0) {
        return lines;
    }

    // Align to the nearest grid boundary at or before start_tick.
    Tick aligned_start = (start_tick / grid_size) * grid_size;

    for (Tick t = aligned_start; t <= end_tick; t += grid_size) {
        GridLineType type = GridLineType::Subdivision;

        Tick measure_ticks =
            static_cast<Tick>(ticks_per_beat_) *
            static_cast<Tick>(beats_per_measure_);
        if (t % measure_ticks == 0) {
            type = GridLineType::Measure;
        } else if (t % ticks_per_beat_ == 0) {
            type = GridLineType::Beat;
        } else {
            type = GridLineType::Subdivision;
        }

        lines.push_back(GridLine{t, type});
    }

    return lines;
}

std::vector<RulerLabel> GridSnapSystem::ruler_labels(
    Tick start_tick, Tick end_tick, double pixels_per_beat) const {
    std::vector<RulerLabel> labels;
    if (start_tick >= end_tick) {
        return labels;
    }

    Tick label_interval{};
    bool use_beat_labels = true;

    // Bitwig-style density:
    // - Very zoomed in: show 16th-style beat labels.
    // - Medium zoom: show beats.
    // - Zoomed out: show only bar numbers.
    if (pixels_per_beat >= 460.0) {
        // Use 1/16 note resolution for labels.
        label_interval = (ticks_per_beat_ * 4) / 16;
    } else if (pixels_per_beat >= 67.0) {
        // Beats.
        label_interval = ticks_per_beat_;
    } else if (pixels_per_beat >= 40.0) {
        // Bars only.
        label_interval =
            ticks_per_beat_ * static_cast<Tick>(beats_per_measure_);
        use_beat_labels = false;
    } else {
        // Very zoomed out: show every 2 bars.
        label_interval =
            ticks_per_beat_ * static_cast<Tick>(beats_per_measure_) * 2;
        use_beat_labels = false;
    }

    if (label_interval <= 0) {
        return labels;
    }

    Tick aligned_start =
        (start_tick / label_interval) * label_interval;

    for (Tick t = aligned_start; t <= end_tick; t += label_interval) {
        RulerLabel label;
        label.tick = t;

        double total_beats =
            static_cast<double>(t) /
            static_cast<double>(ticks_per_beat_);
        int measure = static_cast<int>(total_beats /
                                       static_cast<double>(beats_per_measure_)) +
                      1;

        if (use_beat_labels) {
            int beat = static_cast<int>(std::fmod(
                           total_beats,
                           static_cast<double>(beats_per_measure_))) +
                       1;
            label.text = std::to_string(measure) + "." +
                         std::to_string(beat);
        } else {
            label.text = std::to_string(measure);
        }

        labels.push_back(std::move(label));
    }

    return labels;
}

const SnapDivision* GridSnapSystem::find_division(
    const std::string& label) const noexcept {
    auto it = std::find_if(divisions_.begin(), divisions_.end(),
                           [&label](const SnapDivision& d) {
                               return d.label == label;
                           });
    if (it == divisions_.end()) {
        return nullptr;
    }
    return &(*it);
}

void GridSnapSystem::initialise_default_divisions() {
    divisions_.clear();

    // Match the Python SNAP_DIVISIONS (from fine to coarse).
    divisions_.push_back(SnapDivision{30, "1/64", 4});
    divisions_.push_back(SnapDivision{60, "1/32", 4});
    divisions_.push_back(SnapDivision{120, "1/16", 4});
    divisions_.push_back(SnapDivision{240, "1/8", 4});
    divisions_.push_back(SnapDivision{480, "1/4", 4});
    divisions_.push_back(SnapDivision{960, "1/2", 4});
    divisions_.push_back(SnapDivision{1920, "1 bar", 4});
    divisions_.push_back(SnapDivision{3840, "2 bars", 4});
    divisions_.push_back(SnapDivision{7680, "4 bars", 4});
}

std::string GridSnapSystem::snap_info() const {
    switch (snap_mode_) {
    case SnapMode::Off:
        return "Snap: OFF";
    case SnapMode::Adaptive:
        return "Snap: ADAPTIVE (" + snap_division_.label + ")";
    case SnapMode::Manual:
    default:
        return "Snap: " + snap_division_.label;
    }
}

}  // namespace piano_roll
