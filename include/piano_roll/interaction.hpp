#pragma once

#include "piano_roll/coordinate_system.hpp"
#include "piano_roll/grid_snap.hpp"
#include "piano_roll/note_manager.hpp"

#include <vector>

namespace piano_roll {

// Simple mouse button enum used by the interaction layer.
enum class MouseButton {
    Left,
    Right,
    Middle,
};

// Modifier key state for an input event.
struct ModifierKeys {
    bool shift{false};
    bool ctrl{false};
    bool alt{false};
};

// Edge classification for hover feedback.
enum class HoverEdge {
    None,
    Body,
    Left,
    Right,
};

struct HoverState {
    bool has_note{false};
    NoteId note_id{0};
    HoverEdge edge{HoverEdge::None};
};

// Pointer-based interaction controller for basic editing:
// - Click to select notes.
// - Drag to move notes.
// - Drag near edges to resize notes.
// - Drag in empty space to perform rectangle selection.
// - Double-click to create/delete notes.
//
// Coordinates passed to the handlers are expected to be in the same local
// "screen" space used by CoordinateSystem::screen_to_world/world_to_screen,
// i.e. origin at the top-left of the piano-roll widget, with the piano-key
// strip at x in [0, piano_key_width()).
class PointerTool {
public:
    PointerTool(NoteManager& notes,
                CoordinateSystem& coords,
                GridSnapSystem* snap_system = nullptr);

    void set_snap_system(GridSnapSystem* snap_system) noexcept {
        snap_ = snap_system;
    }

    // Mouse event handlers.
    void on_mouse_down(MouseButton button,
                       double screen_x,
                       double screen_y,
                       const ModifierKeys& mods);

    void on_mouse_move(double screen_x,
                       double screen_y,
                       const ModifierKeys& mods);

    void on_mouse_up(MouseButton button,
                     double screen_x,
                     double screen_y,
                     const ModifierKeys& mods);

    // Double-click handler (host is responsible for detecting double-clicks
    // in the GUI framework and calling this).
    void on_double_click(MouseButton button,
                         double screen_x,
                         double screen_y,
                         const ModifierKeys& mods);

    // Selection rectangle visibility and bounds (world coordinates).
    bool has_selection_rectangle() const noexcept { return rect_active_; }

    void selection_rectangle_world(double& x1,
                                   double& y1,
                                   double& x2,
                                   double& y2) const noexcept;

    // Adjust the edge-detection threshold (in horizontal pixels) used to
    // decide between drag and resize when clicking near note edges.
    void set_edge_threshold_pixels(double value) noexcept {
        edge_threshold_world_ = value;
    }

    // Set the default duration used when creating new notes (in ticks).
    void set_default_note_duration(Duration duration) noexcept {
        default_note_duration_ = duration > 0 ? duration : default_note_duration_;
    }

    // Enable or disable Ctrl+drag duplication behaviour. When enabled,
    // Ctrl+dragging a note duplicates the current selection and drags the
    // duplicates, mirroring the Python behaviour.
    void set_enable_ctrl_drag_duplicate(bool enabled) noexcept {
        enable_ctrl_drag_duplicate_ = enabled;
    }

    // High-level drag state queries for overlays.
    bool is_dragging_note() const noexcept {
        return action_ == Action::DraggingNote;
    }

    bool is_resizing_note() const noexcept {
        return action_ == Action::ResizingLeft ||
               action_ == Action::ResizingRight;
    }

    bool is_duplicating() const noexcept { return is_duplicating_; }

    // Hover information for debug/overlay rendering.
    HoverState hover_state() const noexcept { return hover_; }

    // World-space bounds of the currently hovered note, if any.
    bool hovered_note_world(double& x1,
                            double& y1,
                            double& x2,
                            double& y2,
                            HoverEdge& edge) const noexcept;

    // Adjust drag threshold (in pixels of local screen space) before a click
    // turns into a drag/resize. This loosely mirrors the Python drag
    // threshold used to distinguish clicks from drags.
    void set_drag_threshold_pixels(double value) noexcept {
        drag_threshold_pixels_ = value;
    }

private:
    enum class Action {
        None,
        DraggingNote,
        ResizingLeft,
        ResizingRight,
        RectangleSelection,
    };

    NoteManager* notes_{nullptr};
    CoordinateSystem* coords_{nullptr};
    GridSnapSystem* snap_{nullptr};

    Action action_{Action::None};
    NoteId active_note_id_{0};

    // Note state at the start of drag/resize.
    Tick initial_tick_{0};
    Duration initial_duration_{0};
    MidiKey initial_key_{60};

    // Pointer offset from the note's top-left corner at drag start (world space).
    double drag_offset_world_x_{0.0};
    double drag_offset_world_y_{0.0};

    // Rectangle selection state (world space).
    double rect_start_world_x_{0.0};
    double rect_start_world_y_{0.0};
    double rect_end_world_x_{0.0};
    double rect_end_world_y_{0.0};
    bool rect_active_{false};
    std::vector<NoteId> initial_selection_;

    // Configuration
    double edge_threshold_world_{5.0};  // pixels (world X units)
    Duration default_note_duration_{480};  // one beat at 480 TPB

    bool enable_ctrl_drag_duplicate_{true};
    bool is_duplicating_{false};
    std::vector<NoteId> drag_original_selection_;

    // Drag threshold in pixels (local screen space) before we commit to a
    // drag/resize from a click, used to avoid accidental drags from tiny
    // movements.
    double drag_threshold_pixels_{3.0};
    bool pending_click_{false};
    double click_start_screen_x_{0.0};
    double click_start_screen_y_{0.0};

    bool pending_toggle_on_release_{false};

    HoverState hover_{};

    // Internal helpers
    Tick apply_snap(Tick raw_tick,
                    const ModifierKeys& mods) const;

    void begin_rectangle_selection(double world_x,
                                   double world_y,
                                   const ModifierKeys& mods);

    void update_rectangle_selection(const ModifierKeys& mods);
};

}  // namespace piano_roll
