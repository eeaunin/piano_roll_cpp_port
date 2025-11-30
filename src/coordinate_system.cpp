#include "piano_roll/coordinate_system.hpp"

#include <algorithm>

namespace piano_roll {

CoordinateSystem::CoordinateSystem(double piano_key_width_pixels)
    : piano_key_width_pixels_(piano_key_width_pixels) {}

void CoordinateSystem::set_ticks_per_beat(int ticks) {
    if (ticks <= 0) {
        return;
    }
    ticks_per_beat_ = ticks;
}

void CoordinateSystem::set_pixels_per_beat(double value) {
    if (value <= 0.0) {
        return;
    }
    pixels_per_beat_ =
        std::clamp(value, min_pixels_per_beat_, max_pixels_per_beat_);
}

void CoordinateSystem::set_key_height(double height_pixels) {
    if (height_pixels <= 0.0) {
        return;
    }
    key_height_pixels_ = height_pixels;
}

void CoordinateSystem::set_total_keys(int key_count) {
    if (key_count <= 0) {
        return;
    }
    total_keys_ = key_count;
}

double CoordinateSystem::max_scroll_y() const noexcept {
    // Content height is total_keys_ * key_height, matching the Python
    // VirtualViewport convention (128 keys stacked vertically).
    double content_height =
        static_cast<double>(total_keys_) * key_height_pixels_;
    double max_y = content_height - viewport_.height;
    if (max_y < 0.0) {
        max_y = 0.0;
    }
    return max_y;
}

std::pair<double, double> CoordinateSystem::screen_to_world(
    double screen_x, double screen_y) const noexcept {
    double world_x = screen_x - piano_key_width_pixels_ + viewport_.x;
    double world_y = screen_y + viewport_.y;
    return {world_x, world_y};
}

std::pair<double, double> CoordinateSystem::world_to_screen(
    double world_x, double world_y) const noexcept {
    double screen_x = world_x - viewport_.x + piano_key_width_pixels_;
    double screen_y = world_y - viewport_.y;
    return {screen_x, screen_y};
}

Tick CoordinateSystem::world_to_tick(double world_x) const noexcept {
    double beats = world_x / pixels_per_beat_;
    double tick_value = beats * static_cast<double>(ticks_per_beat_);
    if (tick_value < 0.0) {
        tick_value = 0.0;
    }
    return static_cast<Tick>(tick_value);
}

double CoordinateSystem::tick_to_world(Tick tick) const noexcept {
    if (ticks_per_beat_ <= 0) {
        return 0.0;
    }
    double beats = static_cast<double>(tick) / static_cast<double>(ticks_per_beat_);
    return beats * pixels_per_beat_;
}

double CoordinateSystem::key_to_world_y(MidiKey key) const noexcept {
    if (key < 0) {
        key = 0;
    } else if (key >= total_keys_) {
        key = total_keys_ - 1;
    }
    int inverted_index = total_keys_ - 1 - key;
    return static_cast<double>(inverted_index) * key_height_pixels_;
}

MidiKey CoordinateSystem::world_y_to_key(double world_y) const noexcept {
    if (key_height_pixels_ <= 0.0 || total_keys_ <= 0) {
        return 0;
    }
    double key_float = world_y / key_height_pixels_;
    int base_index = static_cast<int>(key_float);
    int key_from_top = total_keys_ - 1 - base_index;
    if (key_from_top < 0) {
        key_from_top = 0;
    } else if (key_from_top >= total_keys_) {
        key_from_top = total_keys_ - 1;
    }
    return static_cast<MidiKey>(key_from_top);
}

void CoordinateSystem::set_zoom(double pixels_per_beat_value) {
    set_pixels_per_beat(pixels_per_beat_value);
}

void CoordinateSystem::zoom_in(double factor) {
    set_pixels_per_beat(pixels_per_beat_ * factor);
}

void CoordinateSystem::zoom_out(double factor) {
    set_pixels_per_beat(pixels_per_beat_ / factor);
}

void CoordinateSystem::zoom_at(double factor, double world_x_anchor) {
    if (factor <= 0.0) {
        return;
    }

    double old_ppb = pixels_per_beat_;
    if (old_ppb <= 0.0) {
        return;
    }

    double target_ppb = old_ppb * factor;
    double new_ppb =
        std::clamp(target_ppb, min_pixels_per_beat_, max_pixels_per_beat_);

    // Effective factor may be smaller/larger after clamping.
    double effective_factor = new_ppb / old_ppb;

    // Scale the anchor's world coordinate and adjust the viewport so that
    // the anchor stays at the same screen X position.
    double world_x_scaled = world_x_anchor * effective_factor;
    double delta_world_x = world_x_scaled - world_x_anchor;

    pixels_per_beat_ = new_ppb;
    viewport_.x += delta_world_x;
    if (viewport_.x < 0.0) {
        viewport_.x = 0.0;
    }
}

void CoordinateSystem::set_scroll(double world_x, double world_y) {
    // Allow negative X (Bitwig-style infinite timeline).
    // Clamp Y to [0, max_scroll_y()] so the last key stays visible.
    double clamped_y = world_y;
    if (clamped_y < 0.0) {
        clamped_y = 0.0;
    } else {
        double max_y = max_scroll_y();
        if (clamped_y > max_y) {
            clamped_y = max_y;
        }
    }
    viewport_.x = world_x;
    viewport_.y = clamped_y;
}

void CoordinateSystem::pan(double delta_x, double delta_y) {
    // Horizontal pan: allow negative X; vertical pan: clamp using set_scroll.
    double new_x = viewport_.x + delta_x;
    double new_y = viewport_.y + delta_y;
    set_scroll(new_x, new_y);
}

std::pair<Tick, Tick> CoordinateSystem::visible_tick_range() const noexcept {
    Tick start_tick = world_to_tick(viewport_.x);
    Tick end_tick = world_to_tick(viewport_.x + viewport_.width);
    if (end_tick < start_tick) {
        end_tick = start_tick;
    }
    return {start_tick, end_tick};
}

std::pair<MidiKey, MidiKey> CoordinateSystem::visible_key_range() const noexcept {
    MidiKey highest_key = world_y_to_key(viewport_.y);
    MidiKey lowest_key = world_y_to_key(viewport_.y + viewport_.height);
    if (lowest_key > highest_key) {
        std::swap(lowest_key, highest_key);
    }
    return {lowest_key, highest_key};
}

void CoordinateSystem::center_on_tick(Tick tick) {
    double world_x = tick_to_world(tick);
    double new_x = world_x - viewport_.width / 2.0;
    if (new_x < 0.0) {
        new_x = 0.0;
    }
    viewport_.x = new_x;
}

void CoordinateSystem::center_on_key(MidiKey key) {
    double world_y = key_to_world_y(key);
    double new_y =
        world_y - viewport_.height / 2.0 + key_height_pixels_ / 2.0;
    set_scroll(viewport_.x, new_y);
}

}  // namespace piano_roll
