#include "piano_roll/custom_scrollbar.hpp"

#include <algorithm>
#include <cmath>
#include <ctime>

#ifdef PIANO_ROLL_USE_IMGUI
#include <imgui.h>
#endif

namespace piano_roll {

CustomScrollbar::CustomScrollbar(ScrollbarOrientation orientation)
    : orientation_(orientation) {
    // Override DraggableRectangle behaviour: snapping is disabled, we operate
    // directly in screen space.
    snap_enabled = false;
    show_drag_preview = false;
    // Bind bounds-changed callback.
    on_bounds_changed = [this](const RectangleBounds& b) {
        handle_bounds_changed_internal(b);
    };
}

void CustomScrollbar::update_geometry(int x, int y, int length) {
    if (orientation_ == ScrollbarOrientation::Horizontal) {
        track_pos_ = {static_cast<double>(x), static_cast<double>(y)};
        track_size_px_ = {static_cast<double>(length),
                          static_cast<double>(track_size)};
    } else {
        track_pos_ = {static_cast<double>(x), static_cast<double>(y)};
        track_size_px_ = {static_cast<double>(track_size),
                          static_cast<double>(length)};
    }
    update_thumb();
}

void CustomScrollbar::set_content_size(double size) {
    content_size_ = std::max(1.0, size);
    update_thumb();
}

void CustomScrollbar::set_viewport_size(double size) {
    viewport_size_ = std::max(1.0, size);
    if (!edge_resize_mode_) {
        update_thumb();
    }
}

void CustomScrollbar::set_scroll_position(double position) {
    if (orientation_ == ScrollbarOrientation::Horizontal) {
        // Horizontal: allow scroll beyond explored area; explored_min/max
        // are managed separately.
        scroll_position_ = position;
    } else {
        // Vertical: clamp to content range.
        double max_scroll = std::max(0.0, content_size_ - viewport_size_);
        scroll_position_ = std::max(0.0, std::min(position, max_scroll));
    }
    if (!edge_resize_mode_) {
        update_thumb();
    }
}

void CustomScrollbar::set_explored_area(double min_pos, double max_pos) {
    double old_min = explored_min_;
    double old_max = explored_max_;
    explored_min_ = min_pos;
    explored_max_ = max_pos;
    if (!edge_resize_mode_ &&
        (old_min != min_pos || old_max != max_pos)) {
        update_thumb();
    }
}

void CustomScrollbar::expand_explored_area(double position) {
    explored_min_ = std::min(explored_min_, position);
    explored_max_ =
        std::max(explored_max_, position + viewport_size_);
    update_thumb();
}

bool CustomScrollbar::handle_mouse_move(double mouse_x, double mouse_y) {
    InteractionState prev_state = interaction_state;

    // Drag threshold handling when we have drag intent.
    if (drag_intent_ && drag_start_mouse_) {
        double dx = std::abs(mouse_x - drag_start_mouse_->first);
        double dy = std::abs(mouse_y - drag_start_mouse_->second);
        if (dx > drag_threshold_ || dy > drag_threshold_) {
            // Start actual dragging.
            drag_intent_ = false;
            interaction_state = InteractionState::Dragging;
            drag_start_pos_ = drag_start_mouse_;
            original_bounds_ = bounds;
            // CustomScrollbar keeps its own offsets.
            last_mouse_x_ = mouse_x;
            last_mouse_y_ = mouse_y;
            drag_start_mouse_.reset();
        }
    }

    // Suppress hover after edge release.
    if (suppress_hover_) {
        // We emulate the Python logic loosely: require some movement before
        // re-enabling hover.
        double dx = std::abs(mouse_x - last_mouse_x_);
        double dy = std::abs(mouse_y - last_mouse_y_);
        if (dx > 5.0 || dy > 5.0) {
            suppress_hover_ = false;
        } else {
            return false;
        }
    }

    // Base hover detection.
    InteractionState new_state =
        DraggableRectangle::handle_mouse_move(mouse_x, mouse_y);

    // Handle edge resize for horizontal orientation.
    if (interaction_state == InteractionState::ResizingLeft ||
        interaction_state == InteractionState::ResizingRight) {
        if (orientation_ == ScrollbarOrientation::Horizontal &&
            on_edge_resize) {
            double delta_x = mouse_x - last_mouse_x_;
            edge_resize_mode_ = true;

            double current_x1 = bounds.left;
            double current_x2 = bounds.right;
            double current_y = bounds.top;

            double new_x1 = current_x1;
            double new_x2 = current_x2;
            const double min_width = 20.0;
            if (interaction_state == InteractionState::ResizingLeft) {
                new_x1 =
                    std::max(track_pos_.first, current_x1 + delta_x);
                new_x2 = current_x2;
                if (new_x2 - new_x1 < min_width) {
                    new_x1 = new_x2 - min_width;
                }
            } else {
                new_x1 = current_x1;
                new_x2 = std::min(track_pos_.first + track_size_px_.first,
                                  current_x2 + delta_x);
                if (new_x2 - new_x1 < min_width) {
                    new_x2 = new_x1 + min_width;
                }
            }

            manual_thumb_pos_ = {new_x1, current_y};
            manual_thumb_size_ = {new_x2 - new_x1,
                                  static_cast<double>(track_size)};

            bounds.left = new_x1;
            bounds.top = current_y;
            bounds.right = new_x1 + (new_x2 - new_x1);
            bounds.bottom = bounds.top + track_size;

            const char* edge_side =
                (interaction_state == InteractionState::ResizingLeft)
                    ? "left"
                    : "right";
            on_edge_resize(edge_side, delta_x);

            last_mouse_x_ = mouse_x;
            return true;
        }
    }

    // Handle normal dragging using our custom update (screen coords).
    if (interaction_state == InteractionState::Dragging) {
        DraggableRectangle::update_drag(mouse_x, mouse_y);
        last_mouse_x_ = mouse_x;
        last_mouse_y_ = mouse_y;
        return true;
    }

    last_mouse_x_ = mouse_x;
    last_mouse_y_ = mouse_y;
    return new_state != prev_state;
}

bool CustomScrollbar::handle_mouse_down(double mouse_x,
                                        double mouse_y,
                                        int button) {
    if (button != 0) {
        return false;
    }

    // Store last mouse position.
    last_mouse_x_ = mouse_x;
    last_mouse_y_ = mouse_y;

    // Double-click detection on thumb.
    double now = static_cast<double>(std::time(nullptr));
    bool within_thumb =
        bounds.left <= mouse_x && mouse_x <= bounds.right &&
        bounds.top <= mouse_y && mouse_y <= bounds.bottom;
    if (within_thumb) {
        double time_diff = now - last_click_time_;
        if (time_diff < double_click_threshold_ && time_diff > 0.05) {
            if (on_double_click) {
                on_double_click();
            }
            last_click_time_ = 0.0;
            return true;
        }
        last_click_time_ = now;
    }

    // Check clicks on the track but not on the thumb.
    bool in_track =
        (track_pos_.first <= mouse_x &&
         mouse_x <= track_pos_.first + track_size_px_.first &&
         track_pos_.second <= mouse_y &&
         mouse_y <= track_pos_.second + track_size_px_.second);
    bool on_thumb = within_thumb;
    if (in_track && !on_thumb) {
        // Page up/down based on click relative to thumb.
        if (orientation_ == ScrollbarOrientation::Horizontal) {
            double max_scroll = std::max(
                0.0, explored_max_ - explored_min_ - viewport_size_);
            if (mouse_x < bounds.left) {
                double new_pos =
                    scroll_position_ - viewport_size_ * 0.9;
                scroll_position_ =
                    std::max(explored_min_, new_pos);
            } else {
                double new_pos =
                    scroll_position_ + viewport_size_ * 0.9;
                scroll_position_ =
                    std::min(explored_min_ + max_scroll, new_pos);
            }
        } else {
            double max_scroll =
                std::max(0.0, content_size_ - viewport_size_);
            if (mouse_y < bounds.top) {
                scroll_position_ =
                    std::max(0.0,
                             scroll_position_ - viewport_size_ * 0.9);
            } else {
                scroll_position_ = std::min(
                    max_scroll,
                    scroll_position_ + viewport_size_ * 0.9);
            }
        }

        update_thumb();
        if (on_scroll_update) {
            on_scroll_update(scroll_position_);
        }
        return true;
    }

    // Thumb interactions: edge-resize vs body drag.
    if (on_thumb) {
        if (orientation_ == ScrollbarOrientation::Horizontal) {
            if (std::abs(mouse_x - bounds.left) <= edge_threshold) {
                interaction_state = InteractionState::ResizingLeft;
                edge_resize_mode_ = true;
                manual_thumb_pos_ = {bounds.left, bounds.top};
                manual_thumb_size_ = {bounds.right - bounds.left,
                                      bounds.bottom - bounds.top};
                drag_start_pos_ = std::make_pair(mouse_x, mouse_y);
                original_bounds_ = bounds;
                return true;
            }
            if (std::abs(mouse_x - bounds.right) <= edge_threshold) {
                interaction_state = InteractionState::ResizingRight;
                edge_resize_mode_ = true;
                manual_thumb_pos_ = {bounds.left, bounds.top};
                manual_thumb_size_ = {bounds.right - bounds.left,
                                      bounds.bottom - bounds.top};
                drag_start_pos_ = std::make_pair(mouse_x, mouse_y);
                original_bounds_ = bounds;
                return true;
            }
            // Body click: set drag intent; actual drag will start after
            // exceeding the threshold.
            drag_intent_ = true;
            drag_start_mouse_ =
                std::make_pair(mouse_x, mouse_y);
            return true;
        }

        // Vertical scrollbar uses base behaviour.
        return DraggableRectangle::handle_mouse_down(mouse_x, mouse_y,
                                                     button);
    }

    return false;
}

bool CustomScrollbar::handle_mouse_up(double mouse_x,
                                      double mouse_y,
                                      int button) {
    if (button != 0) {
        return false;
    }

    if (drag_intent_) {
        drag_intent_ = false;
        drag_start_mouse_.reset();
        return true;
    }

    bool was_resizing =
        interaction_state == InteractionState::ResizingLeft ||
        interaction_state == InteractionState::ResizingRight;
    bool was_dragging =
        interaction_state == InteractionState::Dragging;

    bool result =
        DraggableRectangle::handle_mouse_up(mouse_x, mouse_y, button);

    if (was_resizing &&
        orientation_ == ScrollbarOrientation::Horizontal) {
        edge_resize_mode_ = false;
        manual_thumb_pos_.reset();
        manual_thumb_size_.reset();
        just_finished_edge_resize_ = true;
        update_thumb();
    }

    if (was_dragging && on_drag_end) {
        on_drag_end();
    }

    return result;
}

void CustomScrollbar::update_thumb() {
    if (edge_resize_mode_ && manual_thumb_pos_ &&
        manual_thumb_size_) {
        bounds.left = manual_thumb_pos_->first;
        bounds.top = manual_thumb_pos_->second;
        bounds.right =
            manual_thumb_pos_->first + manual_thumb_size_->first;
        bounds.bottom =
            manual_thumb_pos_->second + manual_thumb_size_->second;
        return;
    }

    if (orientation_ == ScrollbarOrientation::Horizontal) {
        double explored_range = explored_max_ - explored_min_;
        if (viewport_size_ >= explored_range) {
            bounds.left = track_pos_.first;
            bounds.top = track_pos_.second;
            bounds.right =
                track_pos_.first + track_size_px_.first;
            bounds.bottom =
                track_pos_.second + static_cast<double>(track_size);
            return;
        }

        double thumb_ratio = viewport_size_ / explored_range;
        double thumb_length =
            std::max(20.0, track_size_px_.first * thumb_ratio);
        double available_space =
            track_size_px_.first - thumb_length;

        double thumb_offset = 0.0;
        if (available_space > 0.0 &&
            explored_range > viewport_size_) {
            double normalized_pos =
                (scroll_position_ - explored_min_) /
                (explored_range - viewport_size_);
            normalized_pos =
                std::max(0.0, std::min(1.0, normalized_pos));
            thumb_offset = normalized_pos * available_space;
        }

        bounds.left = track_pos_.first + thumb_offset;
        bounds.top = track_pos_.second;
        bounds.right = bounds.left + thumb_length;
        bounds.bottom =
            bounds.top + static_cast<double>(track_size);
    } else {
        // Vertical scrollbar: simple content/viewport ratio.
        if (content_size_ <= 0.0) {
            return;
        }
        if (viewport_size_ >= content_size_) {
            bounds.left = track_pos_.first;
            bounds.top = track_pos_.second;
            bounds.right =
                track_pos_.first +
                static_cast<double>(track_size);
            bounds.bottom =
                track_pos_.second + track_size_px_.second;
            return;
        }

        double thumb_ratio =
            viewport_size_ / content_size_;
        double thumb_length =
            std::max(20.0, track_size_px_.second * thumb_ratio);
        double available_space =
            track_size_px_.second - thumb_length;

        double thumb_offset = 0.0;
        if (available_space > 0.0 &&
            content_size_ > viewport_size_) {
            double scroll_ratio =
                scroll_position_ /
                (content_size_ - viewport_size_);
            thumb_offset = scroll_ratio * available_space;
        }

        bounds.left = track_pos_.first;
        bounds.top = track_pos_.second + thumb_offset;
        bounds.right =
            bounds.left + static_cast<double>(track_size);
        bounds.bottom = bounds.top + thumb_length;
    }
}

void CustomScrollbar::handle_bounds_changed_internal(
    const RectangleBounds& new_bounds) {
    if (orientation_ == ScrollbarOrientation::Horizontal) {
        double thumb_x_relative =
            new_bounds.left - track_pos_.first;
        double thumb_length = new_bounds.width();
        double available_space =
            track_size_px_.first - thumb_length;
        double explored_range = explored_max_ - explored_min_;
        if (available_space > 0.0 &&
            explored_range > viewport_size_) {
            double scroll_ratio =
                thumb_x_relative / available_space;
            scroll_ratio =
                std::max(0.0, std::min(1.0, scroll_ratio));
            scroll_position_ =
                explored_min_ +
                scroll_ratio * (explored_range - viewport_size_);
        }
    } else {
        double thumb_y_relative =
            new_bounds.top - track_pos_.second;
        double thumb_length = new_bounds.height();
        double available_space =
            track_size_px_.second - thumb_length;
        if (available_space > 0.0 &&
            content_size_ > viewport_size_) {
            double scroll_ratio =
                thumb_y_relative / available_space;
            scroll_position_ =
                scroll_ratio *
                (content_size_ - viewport_size_);
        }
    }

    if (on_scroll_update && !edge_resize_mode_) {
        on_scroll_update(scroll_position_);
    }
}

std::optional<std::pair<double, double>>
CustomScrollbar::screen_to_world(double x, double y) const {
    return std::make_pair(x, y);
}

std::optional<std::pair<double, double>>
CustomScrollbar::world_to_screen(double x, double y) const {
    return std::make_pair(x, y);
}

std::optional<RectangleBounds>
CustomScrollbar::get_screen_bounds() const {
    return bounds;
}

std::optional<RectangleBounds>
CustomScrollbar::world_to_screen_bounds(
    const RectangleBounds& b) const {
    return b;
}

#ifdef PIANO_ROLL_USE_IMGUI
void CustomScrollbar::render(void* draw_list_void) const {
    ImDrawList* draw_list =
        static_cast<ImDrawList*>(draw_list_void);
    if (!draw_list || !visible) {
        return;
    }

    // Simple visual style; more detailed styling can be ported later.
    ImU32 track_col =
        ImGui::GetColorU32(ImVec4(0.14f, 0.14f, 0.14f, 1.0f));
    ImU32 thumb_col =
        ImGui::GetColorU32(ImVec4(0.4f, 0.4f, 0.4f, 1.0f));

    ImVec2 track_min(
        static_cast<float>(track_pos_.first),
        static_cast<float>(track_pos_.second));
    ImVec2 track_max(
        static_cast<float>(track_pos_.first +
                           track_size_px_.first),
        static_cast<float>(track_pos_.second +
                           track_size_px_.second));
    draw_list->AddRectFilled(track_min, track_max, track_col,
                             0.0f);

    ImVec2 thumb_min(static_cast<float>(bounds.left),
                     static_cast<float>(bounds.top));
    ImVec2 thumb_max(static_cast<float>(bounds.right),
                     static_cast<float>(bounds.bottom));
    draw_list->AddRectFilled(thumb_min, thumb_max, thumb_col,
                             4.0f);
}
#endif

}  // namespace piano_roll

