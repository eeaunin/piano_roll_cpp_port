#pragma once

#include <functional>
#include <optional>

namespace piano_roll {

// Interaction states for draggable rectangles (notes, scrollbars, markers, etc.).
enum class InteractionState {
    Idle,
    HoveringBody,
    HoveringLeftEdge,
    HoveringRightEdge,
    Dragging,
    ResizingLeft,
    ResizingRight,
};

// Represents rectangle bounds in the rectangle's native coordinate space.
struct RectangleBounds {
    double left{0.0};
    double right{0.0};
    double top{0.0};
    double bottom{0.0};

    double width() const noexcept { return right - left; }
    double height() const noexcept { return bottom - top; }
    double center_x() const noexcept { return (left + right) * 0.5; }
    double center_y() const noexcept { return (top + bottom) * 0.5; }
};

// Base class for draggable/resizable rectangles, adapted from the Python
// DraggableRectangle. Subclasses provide coordinate conversion and rendering.
class DraggableRectangle {
public:
    DraggableRectangle();
    virtual ~DraggableRectangle() = default;

    // Core state
    RectangleBounds bounds;
    InteractionState interaction_state{InteractionState::Idle};
    bool visible{true};
    bool enabled{true};

    // Interaction configuration
    double edge_threshold{5.0};  // pixels for edge detection
    double min_width{10.0};      // minimum width when resizing
    bool snap_enabled{true};
    double snap_size{1.0};       // grid size for snapping

    // Visual configuration flags (actual drawing is done by subclasses)
    bool show_resize_handles{true};
    bool show_drag_preview{true};

    // Callbacks
    std::function<void(const RectangleBounds&)> on_bounds_changed;
    std::function<void(InteractionState)> on_interaction_state_changed;

    // Event handlers ----------------------------------------------------------

    // Mouse move for hover detection. Returns current interaction state.
    InteractionState handle_mouse_move(double x, double y);

    // Mouse press to start drag/resize. Returns true if interaction started.
    bool handle_mouse_down(double x, double y, int button = 0);

    // Mouse drag to update position or size. Returns true if bounds changed.
    bool handle_mouse_drag(double x, double y);

    // Mouse release to end interaction. Returns true if interaction ended.
    bool handle_mouse_up(double x, double y, int button = 0);

protected:
    // Drag/resize state
    std::optional<std::pair<double, double>> drag_start_pos_;
    std::pair<double, double> drag_offset_{0.0, 0.0};
    std::optional<RectangleBounds> original_bounds_;
    std::optional<RectangleBounds> preview_bounds_;

    // Subclass hooks for coordinate conversion --------------------------------

    // Convert screen coordinates to world coordinates. May return std::nullopt
    // if the conversion is not valid in the given context.
    virtual std::optional<std::pair<double, double>> screen_to_world(
        double x, double y) const = 0;

    // Convert world coordinates to screen coordinates. May return std::nullopt
    // if the conversion is not valid.
    virtual std::optional<std::pair<double, double>> world_to_screen(
        double x, double y) const = 0;

    // Get the current bounds in screen coordinates.
    virtual std::optional<RectangleBounds> get_screen_bounds() const = 0;

    // Convert bounds from world to screen coordinates.
    virtual std::optional<RectangleBounds> world_to_screen_bounds(
        const RectangleBounds& bounds) const = 0;

    // Internal helpers ---------------------------------------------------------

    virtual void on_bounds_finalized() {}

    double snap_value(double value) const;

    void start_drag(double x, double y);
    void start_resize_left(double x, double y);
    void start_resize_right(double x, double y);

    bool update_drag(double x, double y);
    bool update_resize_left(double x, double y);
    bool update_resize_right(double x, double y);

    void end_interaction();
};

}  // namespace piano_roll

