#pragma once

#include "piano_roll/types.hpp"

#include <utility>

namespace piano_roll {

// Represents the visible area in world coordinates.
struct Viewport {
    double x{0.0};
    double y{0.0};
    double width{1200.0};
    double height{700.0};
};

// Manages coordinate transformations between screen, world, and musical time.
class CoordinateSystem {
public:
    explicit CoordinateSystem(double piano_key_width_pixels = 180.0);

    // Layout parameters
    double piano_key_width() const noexcept { return piano_key_width_pixels_; }
    void set_piano_key_width(double width_pixels) noexcept {
        piano_key_width_pixels_ = width_pixels;
    }

    int ticks_per_beat() const noexcept { return ticks_per_beat_; }
    void set_ticks_per_beat(int ticks);

    double pixels_per_beat() const noexcept { return pixels_per_beat_; }
    void set_pixels_per_beat(double value);

    double min_pixels_per_beat() const noexcept { return min_pixels_per_beat_; }
    double max_pixels_per_beat() const noexcept { return max_pixels_per_beat_; }

    double key_height() const noexcept { return key_height_pixels_; }
    void set_key_height(double height_pixels);

    int total_keys() const noexcept { return total_keys_; }
    void set_total_keys(int key_count);

    const Viewport& viewport() const noexcept { return viewport_; }
    Viewport& viewport() noexcept { return viewport_; }

    // Coordinate transforms
    std::pair<double, double> screen_to_world(double screen_x,
                                              double screen_y) const noexcept;
    std::pair<double, double> world_to_screen(double world_x,
                                              double world_y) const noexcept;

    Tick world_to_tick(double world_x) const noexcept;
    double tick_to_world(Tick tick) const noexcept;

    double key_to_world_y(MidiKey key) const noexcept;
    MidiKey world_y_to_key(double world_y) const noexcept;

    // Zoom and scroll
    void set_zoom(double pixels_per_beat_value);
    void zoom_in(double factor = 1.1);
    void zoom_out(double factor = 1.1);

    // Zoom around a specific anchor point in world X coordinates, keeping the
    // anchor at the same screen X position as much as possible.
    void zoom_at(double factor, double world_x_anchor);

    void set_scroll(double world_x, double world_y);
    void pan(double delta_x, double delta_y);

    // Visible ranges
    std::pair<Tick, Tick> visible_tick_range() const noexcept;
    std::pair<MidiKey, MidiKey> visible_key_range() const noexcept;

    void center_on_tick(Tick tick);
    void center_on_key(MidiKey key);

    // Maximum vertical scroll (world Y) that keeps the last key visible,
    // mirroring VirtualViewport.get_max_scroll_y in the Python version.
    double max_scroll_y() const noexcept;

private:
    double piano_key_width_pixels_{};
    Viewport viewport_{};

    int ticks_per_beat_{480};
    double pixels_per_beat_{60.0};
    double min_pixels_per_beat_{15.0};
    double max_pixels_per_beat_{4000.0};

    double key_height_pixels_{20.0};
    int total_keys_{128};
};

}  // namespace piano_roll
