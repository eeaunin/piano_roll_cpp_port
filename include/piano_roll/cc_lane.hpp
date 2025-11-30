#pragma once

#include "piano_roll/types.hpp"

#include <algorithm>
#include <vector>

namespace piano_roll {

// Single control point in a MIDI CC lane.
struct ControlPoint {
    Tick tick{0};
    int value{0};  // 0-127
};

// Simple MIDI CC lane: a CC number with a list of control points.
class ControlLane {
public:
    ControlLane() = default;

    explicit ControlLane(int cc_number)
        : cc_number_(cc_number) {}

    int cc_number() const noexcept { return cc_number_; }
    void set_cc_number(int cc) noexcept { cc_number_ = cc; }

    const std::vector<ControlPoint>& points() const noexcept {
        return points_;
    }

    std::vector<ControlPoint>& points() noexcept {
        return points_;
    }

    // Add a new point and keep the lane sorted by tick.
    void add_point(Tick tick, int value) {
        ControlPoint p{tick, clamp_value(value)};
        points_.push_back(p);
        std::sort(points_.begin(), points_.end(),
                  [](const ControlPoint& a, const ControlPoint& b) {
                      return a.tick < b.tick;
                  });
    }

    // Remove the first point whose tick is within max_delta of the given tick.
    // Returns true if a point was removed.
    bool remove_near(Tick tick, Tick max_delta) {
        for (auto it = points_.begin(); it != points_.end(); ++it) {
            if (std::llabs(it->tick - tick) <= max_delta) {
                points_.erase(it);
                return true;
            }
        }
        return false;
    }

    // Find index of point near the given tick (within max_delta), or -1.
    int index_near(Tick tick, Tick max_delta) const noexcept {
        for (std::size_t i = 0; i < points_.size(); ++i) {
            if (std::llabs(points_[i].tick - tick) <= max_delta) {
                return static_cast<int>(i);
            }
        }
        return -1;
    }

    ControlPoint* point_at_index(int index) noexcept {
        if (index < 0 ||
            static_cast<std::size_t>(index) >= points_.size()) {
            return nullptr;
        }
        return &points_[static_cast<std::size_t>(index)];
    }

    const ControlPoint* point_at_index(int index) const noexcept {
        if (index < 0 ||
            static_cast<std::size_t>(index) >= points_.size()) {
            return nullptr;
        }
        return &points_[static_cast<std::size_t>(index)];
    }

    // Clamp and set value at index.
    void set_value(int index, int value) {
        ControlPoint* p = point_at_index(index);
        if (!p) {
            return;
        }
        p->value = clamp_value(value);
    }

    // Update tick and keep lane sorted.
    void set_tick(int index, Tick tick) {
        ControlPoint* p = point_at_index(index);
        if (!p) {
            return;
        }
        p->tick = tick;
        std::sort(points_.begin(), points_.end(),
                  [](const ControlPoint& a, const ControlPoint& b) {
                      return a.tick < b.tick;
                  });
    }

private:
    int cc_number_{1};
    std::vector<ControlPoint> points_;

    static int clamp_value(int value) noexcept {
        if (value < 0) return 0;
        if (value > 127) return 127;
        return value;
    }
};

}  // namespace piano_roll

