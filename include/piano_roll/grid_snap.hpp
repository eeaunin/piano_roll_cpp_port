#pragma once

#include "piano_roll/types.hpp"

#include <cmath>
#include <cstdint>
#include <string>
#include <utility>
#include <vector>

namespace piano_roll {

// Snap modes for note placement and editing.
enum class SnapMode {
    Off,
    Adaptive,
    Manual,
};

// Represents a grid/snap division (e.g. "1/16").
struct SnapDivision {
    Tick ticks{};
    std::string label;
    int beats_per_measure{4};

    Tick ticks_per_measure() const noexcept {
        return ticks * static_cast<Tick>(beats_per_measure);
    }
};

// Types of grid lines used for rendering.
enum class GridLineType {
    Measure,
    Beat,
    Subdivision,
};

struct GridLine {
    Tick tick{};
    GridLineType type{GridLineType::Subdivision};
};

struct RulerLabel {
    Tick tick{};
    std::string text;
};

// Manages grid rendering and note snapping behaviour.
class GridSnapSystem {
public:
    explicit GridSnapSystem(int ticks_per_beat = 480);

    void set_ticks_per_beat(int ticks_per_beat) noexcept;
    int ticks_per_beat() const noexcept { return ticks_per_beat_; }

    void set_beats_per_measure(int beats_per_measure) noexcept;
    int beats_per_measure() const noexcept { return beats_per_measure_; }

    SnapMode snap_mode() const noexcept { return snap_mode_; }
    void set_snap_mode(SnapMode mode) noexcept { snap_mode_ = mode; }

    const SnapDivision& snap_division() const noexcept { return snap_division_; }
    const SnapDivision& grid_division() const noexcept { return grid_division_; }

    // Set snap/grid divisions by label (e.g. "1/4").
    // Returns true if the division was found.
    bool set_snap_division(const std::string& label);
    bool set_grid_division(const std::string& label);

    // Cycle through snap divisions in predefined order.
    void cycle_snap_division(bool forward = true);

    // Get a suitable division for the current zoom level.
    const SnapDivision& adaptive_division(double pixels_per_beat,
                                          bool for_grid) const;

    // Core snapping helpers.
    Tick snap_tick(Tick tick, SnapMode mode_override = SnapMode::Adaptive) const;
    Tick snap_tick_floor(Tick tick) const;
    Tick snap_tick_ceil(Tick tick) const;

    // Magnetic snap: only snap if close to a grid line (in pixels).
    std::pair<Tick, bool> magnetic_snap(Tick tick,
                                        double pixels_per_beat,
                                        double magnetic_range_pixels = 8.0) const;

    // Grid/ruler helpers for rendering.
    std::vector<GridLine> grid_lines(Tick start_tick,
                                     Tick end_tick,
                                     double pixels_per_beat) const;

    std::vector<RulerLabel> ruler_labels(Tick start_tick,
                                         Tick end_tick,
                                         double pixels_per_beat) const;

    // Human-readable snap description (e.g. "Snap: OFF", "Snap: ADAPTIVE (1/16)"),
    // mirroring the Python GridSnapSystem.get_snap_info helper.
    std::string snap_info() const;

private:
    int ticks_per_beat_{480};
    int beats_per_measure_{4};

    SnapMode snap_mode_{SnapMode::Adaptive};
    SnapDivision snap_division_;
    SnapDivision grid_division_;

    // Predefined divisions in ascending order of coarseness.
    std::vector<SnapDivision> divisions_;

    const SnapDivision* find_division(const std::string& label) const noexcept;
    void initialise_default_divisions();
};

}  // namespace piano_roll
