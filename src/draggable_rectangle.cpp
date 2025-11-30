#include "piano_roll/draggable_rectangle.hpp"

#include <cmath>

namespace piano_roll {

DraggableRectangle::DraggableRectangle() = default;

double DraggableRectangle::snap_value(double value) const {
    if (!snap_enabled || snap_size <= 0.0) {
        return value;
    }
    return std::round(value / snap_size) * snap_size;
}

InteractionState DraggableRectangle::handle_mouse_move(double x, double y) {
    if (!enabled || !visible) {
        return InteractionState::Idle;
    }

    // Don't update hover during active interactions.
    if (interaction_state == InteractionState::Dragging ||
        interaction_state == InteractionState::ResizingLeft ||
        interaction_state == InteractionState::ResizingRight) {
        return interaction_state;
    }

    auto screen_bounds_opt = get_screen_bounds();
    if (!screen_bounds_opt) {
        interaction_state = InteractionState::Idle;
        return interaction_state;
    }
    RectangleBounds screen_bounds = *screen_bounds_opt;

    if (!(screen_bounds.left <= x && x <= screen_bounds.right &&
          screen_bounds.top <= y && y <= screen_bounds.bottom)) {
        interaction_state = InteractionState::Idle;
        if (on_interaction_state_changed) {
            on_interaction_state_changed(interaction_state);
        }
        return interaction_state;
    }

    InteractionState old_state = interaction_state;

    if (std::abs(x - screen_bounds.left) <= edge_threshold) {
        interaction_state = InteractionState::HoveringLeftEdge;
    } else if (std::abs(x - screen_bounds.right) <= edge_threshold) {
        interaction_state = InteractionState::HoveringRightEdge;
    } else {
        interaction_state = InteractionState::HoveringBody;
    }

    if (interaction_state != old_state && on_interaction_state_changed) {
        on_interaction_state_changed(interaction_state);
    }

    return interaction_state;
}

bool DraggableRectangle::handle_mouse_down(double x, double y, int button) {
    if (!enabled || !visible || button != 0) {
        return false;
    }

    if (interaction_state == InteractionState::HoveringLeftEdge) {
        start_resize_left(x, y);
        return true;
    }
    if (interaction_state == InteractionState::HoveringRightEdge) {
        start_resize_right(x, y);
        return true;
    }
    if (interaction_state == InteractionState::HoveringBody) {
        start_drag(x, y);
        return true;
    }

    return false;
}

bool DraggableRectangle::handle_mouse_drag(double x, double y) {
    if (!enabled) {
        return false;
    }

    if (interaction_state == InteractionState::Dragging) {
        return update_drag(x, y);
    }
    if (interaction_state == InteractionState::ResizingLeft) {
        return update_resize_left(x, y);
    }
    if (interaction_state == InteractionState::ResizingRight) {
        return update_resize_right(x, y);
    }

    return false;
}

bool DraggableRectangle::handle_mouse_up(double x, double y, int button) {
    if (button != 0) {
        return false;
    }

    if (interaction_state == InteractionState::Dragging ||
        interaction_state == InteractionState::ResizingLeft ||
        interaction_state == InteractionState::ResizingRight) {
        end_interaction();
        return true;
    }

    return false;
}

void DraggableRectangle::start_drag(double x, double y) {
    interaction_state = InteractionState::Dragging;
    drag_start_pos_ = std::make_pair(x, y);
    original_bounds_ = bounds;

    auto world_pos_opt = screen_to_world(x, y);
    if (world_pos_opt) {
        double wx = world_pos_opt->first;
        double wy = world_pos_opt->second;
        drag_offset_ = {wx - bounds.left, wy - bounds.top};
    } else {
        drag_offset_ = {0.0, 0.0};
    }

    if (show_drag_preview) {
        preview_bounds_ = bounds;
    }

    if (on_interaction_state_changed) {
        on_interaction_state_changed(interaction_state);
    }
}

void DraggableRectangle::start_resize_left(double x, double y) {
    interaction_state = InteractionState::ResizingLeft;
    drag_start_pos_ = std::make_pair(x, y);
    original_bounds_ = bounds;

    if (show_drag_preview) {
        preview_bounds_ = bounds;
    }

    if (on_interaction_state_changed) {
        on_interaction_state_changed(interaction_state);
    }
}

void DraggableRectangle::start_resize_right(double x, double y) {
    interaction_state = InteractionState::ResizingRight;
    drag_start_pos_ = std::make_pair(x, y);
    original_bounds_ = bounds;

    if (show_drag_preview) {
        preview_bounds_ = bounds;
    }

    if (on_interaction_state_changed) {
        on_interaction_state_changed(interaction_state);
    }
}

bool DraggableRectangle::update_drag(double x, double y) {
    if (!drag_start_pos_ || !original_bounds_) {
        return false;
    }

    auto world_pos_opt = screen_to_world(x, y);
    if (!world_pos_opt) {
        return false;
    }

    double wx = world_pos_opt->first;
    double wy = world_pos_opt->second;

    double new_left = wx - drag_offset_.first;
    double new_top = wy - drag_offset_.second;

    if (snap_enabled) {
        new_left = snap_value(new_left);
        new_top = snap_value(new_top);
    }

    double width = bounds.width();
    double height = bounds.height();

    if (show_drag_preview && preview_bounds_) {
        preview_bounds_->left = new_left;
        preview_bounds_->right = new_left + width;
        preview_bounds_->top = new_top;
        preview_bounds_->bottom = new_top + height;
    } else {
        bounds.left = new_left;
        bounds.right = new_left + width;
        bounds.top = new_top;
        bounds.bottom = new_top + height;

        if (on_bounds_changed) {
            on_bounds_changed(bounds);
        }
    }

    return true;
}

bool DraggableRectangle::update_resize_left(double x, double y) {
    if (!original_bounds_) {
        return false;
    }

    auto world_pos_opt = screen_to_world(x, y);
    if (!world_pos_opt) {
        return false;
    }

    double new_left = world_pos_opt->first;

    if (snap_enabled) {
        new_left = snap_value(new_left);
    }

    double max_left = bounds.right - min_width;
    if (new_left > max_left) {
        new_left = max_left;
    }

    if (show_drag_preview && preview_bounds_) {
        preview_bounds_->left = new_left;
    } else {
        bounds.left = new_left;
        if (on_bounds_changed) {
            on_bounds_changed(bounds);
        }
    }

    return true;
}

bool DraggableRectangle::update_resize_right(double x, double y) {
    if (!original_bounds_) {
        return false;
    }

    auto world_pos_opt = screen_to_world(x, y);
    if (!world_pos_opt) {
        return false;
    }

    double new_right = world_pos_opt->first;

    if (snap_enabled) {
        new_right = snap_value(new_right);
    }

    double min_right = bounds.left + min_width;
    if (new_right < min_right) {
        new_right = min_right;
    }

    if (show_drag_preview && preview_bounds_) {
        preview_bounds_->right = new_right;
    } else {
        bounds.right = new_right;
        if (on_bounds_changed) {
            on_bounds_changed(bounds);
        }
    }

    return true;
}

void DraggableRectangle::end_interaction() {
    if (show_drag_preview && preview_bounds_) {
        bounds = *preview_bounds_;
        if (on_bounds_changed) {
            on_bounds_changed(bounds);
        }
        on_bounds_finalized();
    }

    interaction_state = InteractionState::Idle;
    drag_start_pos_.reset();
    drag_offset_ = {0.0, 0.0};
    original_bounds_.reset();
    preview_bounds_.reset();

    if (on_interaction_state_changed) {
        on_interaction_state_changed(interaction_state);
    }
}

}  // namespace piano_roll
