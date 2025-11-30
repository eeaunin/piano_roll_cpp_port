#pragma once

#include "piano_roll/draggable_rectangle.hpp"

#include <functional>
#include <optional>

namespace piano_roll {

enum class ScrollbarOrientation {
    Horizontal,
    Vertical,
};

// Custom-rendered scrollbar, transliterated from the Python CustomScrollbar.
// It uses DraggableRectangle for interaction state but overrides behaviour to
// support Bitwig-style zoom/scroll and explored-area semantics.
class CustomScrollbar : public DraggableRectangle {
public:
    explicit CustomScrollbar(ScrollbarOrientation orientation);

    ScrollbarOrientation orientation() const noexcept { return orientation_; }

    // Geometry configuration.
    void update_geometry(int x, int y, int length);

    void set_content_size(double size);
    void set_viewport_size(double size);
    void set_scroll_position(double position);
    double scroll_position() const noexcept { return scroll_position_; }

    void set_explored_area(double min_pos, double max_pos);
    void expand_explored_area(double position);

    // Interaction handlers. Return true if handled.
    bool handle_mouse_move(double mouse_x, double mouse_y);
    bool handle_mouse_down(double mouse_x, double mouse_y, int button);
    bool handle_mouse_up(double mouse_x, double mouse_y, int button);

#ifdef PIANO_ROLL_USE_IMGUI
    // Rendering using ImGui drawlist. Assumes screen coordinates.
    void render(void* draw_list_void) const;
#endif

    // Callbacks (set by host).
    // Called when thumb drag translates into a scroll position change.
    std::function<void(double new_scroll_pos)> on_scroll_update;
    // Called when thumb edges are dragged for zooming ('left' / 'right').
    std::function<void(const char* edge, double delta_x)> on_edge_resize;
    // Called when the thumb is double-clicked.
    std::function<void()> on_double_click;
    // Called when a drag operation ends.
    std::function<void()> on_drag_end;

    // Flags.
    bool zoom_scroll_enabled{true};
    bool scroll_only_mode{false};

    // Track visual properties (used in ImGui rendering).
    float track_size{15.0f};  // height for horizontal, width for vertical

    // Accessors for geometry/state used by higher-level handlers.
    const std::pair<double, double>& track_pos() const noexcept { return track_pos_; }
    const std::pair<double, double>& track_size_px() const noexcept { return track_size_px_; }
    const std::optional<std::pair<double, double>>& manual_thumb_pos() const noexcept { return manual_thumb_pos_; }
    const std::optional<std::pair<double, double>>& manual_thumb_size() const noexcept { return manual_thumb_size_; }
    double explored_min() const noexcept { return explored_min_; }
    double explored_max() const noexcept { return explored_max_; }
    double viewport_size() const noexcept { return viewport_size_; }

private:
    ScrollbarOrientation orientation_;

    // Hover / drag tracking.
    double last_mouse_x_{0.0};
    double last_mouse_y_{0.0};
    bool suppress_hover_{false};

    // Drag threshold to prevent accidental drags.
    int drag_threshold_{3};
    bool drag_intent_{false};
    std::optional<std::pair<double, double>> drag_start_mouse_;

    // Edge resize mode (manual thumb geometry).
    bool edge_resize_mode_{false};
    std::optional<std::pair<double, double>> manual_thumb_pos_;
    std::optional<std::pair<double, double>> manual_thumb_size_;

    // Scrollbar geometry (screen space).
    std::pair<double, double> track_pos_{0.0, 0.0};
    std::pair<double, double> track_size_px_{0.0, 0.0};

    // Scroll properties.
    double content_size_{1000.0};
    double viewport_size_{100.0};
    double scroll_position_{0.0};
    double min_scroll_{0.0};
    double explored_min_{0.0};
    double explored_max_{100.0};

    // Double-click detection.
    double last_click_time_{0.0};
    double double_click_threshold_{0.8};

    // Internal flags.
    bool just_finished_edge_resize_{false};

    // Internal helpers.
    void update_thumb();
    void handle_bounds_changed_internal(const RectangleBounds& new_bounds);

    // DraggableRectangle overrides for coordinate conversion.
    std::optional<std::pair<double, double>> screen_to_world(
        double x, double y) const override;
    std::optional<std::pair<double, double>> world_to_screen(
        double x, double y) const override;
    std::optional<RectangleBounds> get_screen_bounds() const override;
    std::optional<RectangleBounds> world_to_screen_bounds(
        const RectangleBounds& bounds) const override;
};

}  // namespace piano_roll
