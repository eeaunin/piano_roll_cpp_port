#pragma once

#include "piano_roll/cc_lane.hpp"
#include "piano_roll/cc_lane_renderer.hpp"
#include "piano_roll/coordinate_system.hpp"
#include "piano_roll/custom_scrollbar.hpp"
#include "piano_roll/grid_snap.hpp"
#include "piano_roll/interaction.hpp"
#include "piano_roll/keyboard.hpp"
#include "piano_roll/playback.hpp"
#include "piano_roll/note_manager.hpp"
#include "piano_roll/loop_marker_rectangle.hpp"
#include "piano_roll/overlay.hpp"
#include "piano_roll/render_config.hpp"
#include "piano_roll/renderer.hpp"

#include <functional>
#include <vector>

namespace piano_roll {

// High-level, self-contained piano roll widget that ties together the
// core components (NoteManager, CoordinateSystem, snapping, renderer,
// pointer tool, keyboard shortcuts). This is intended for standalone
// usage in a Dear ImGui app without any DAW integration.
class PianoRollWidget {
public:
    PianoRollWidget();

    // Access to core components for external configuration.
    NoteManager& notes() noexcept { return notes_; }
    const NoteManager& notes() const noexcept { return notes_; }

    CoordinateSystem& coords() noexcept { return coords_; }
    const CoordinateSystem& coords() const noexcept { return coords_; }

    GridSnapSystem& snap() noexcept { return snap_; }
    const GridSnapSystem& snap() const noexcept { return snap_; }

    PianoRollRenderer& renderer() noexcept { return renderer_; }
    const PianoRollRenderer& renderer() const noexcept { return renderer_; }

    PianoRollRenderConfig& config() noexcept { return config_; }
    const PianoRollRenderConfig& config() const noexcept { return config_; }

    // Apply a Bitwig-style clip colour theme to core note/marker colours.
    void set_clip_color(const ColorRGBA& color) noexcept {
        config_.apply_clip_color(color);
    }

    // Apply a light theme palette and derive note/marker colours from the
    // given clip colour, approximating the Python light theme driven by
    // clip colour while keeping the same interaction model.
    void apply_light_theme_from_clip_color(const ColorRGBA& color) noexcept {
        config_.apply_light_theme_from_clip_color(color);
    }

    // Playback start and cue markers (render-only; the host is expected to
    // update these from the transport system).
    void set_playback_start_tick(Tick tick) noexcept;
    Tick playback_start_tick() const noexcept {
        return playback_start_tick_;
    }

    void set_cue_markers(Tick left, Tick right) noexcept;
    std::pair<Tick, Tick> cue_markers() const noexcept {
        return {cue_left_tick_, cue_right_tick_};
    }

    // Loop marker control (optional Bitwig-style loop region in the ruler).
    void set_loop_enabled(bool enabled) noexcept;
    bool loop_enabled() const noexcept { return loop_markers_.enabled; }

    void set_loop_range(Tick start, Tick end) noexcept;
    std::pair<Tick, Tick> loop_range() const noexcept {
        return loop_markers_.tick_range();
    }

    // Optional playhead control.
    void set_playhead(Tick tick) noexcept;
    void clear_playhead() noexcept { renderer_.clear_playhead(); }
    bool has_playhead() const noexcept { return renderer_.has_playhead(); }
    Tick playhead_tick() const noexcept { return renderer_.playhead_tick(); }

    // Access to CC lanes.
    std::vector<ControlLane>& cc_lanes() noexcept { return cc_lanes_; }
    const std::vector<ControlLane>& cc_lanes() const noexcept { return cc_lanes_; }

    int active_cc_lane_index() const noexcept { return active_cc_lane_; }
    void set_active_cc_lane_index(int index) noexcept;

    // Hover information for host overlays: returns true if a note is hovered
    // and fills out parameters with note id and edge classification.
    bool hovered_note(NoteId& id_out,
                      HoverEdge& edge_out) const noexcept;

    // World-space bounds of the currently hovered note, if any. Returns true
    // when a note is hovered and fills out bounds plus edge classification.
    bool hovered_note_world(double& x1,
                            double& y1,
                            double& x2,
                            double& y2,
                            HoverEdge& edge_out) const noexcept;

    // High-level drag/duplicate state for host UI hints.
    bool is_dragging_note() const noexcept {
        return pointer_.is_dragging_note();
    }
    bool is_resizing_note() const noexcept {
        return pointer_.is_resizing_note();
    }
    bool is_duplicating_notes() const noexcept {
        return pointer_.is_duplicating();
    }

    // Bounds of the current note selection in tick/key space. Returns false
    // if no notes are selected.
    bool selection_bounds(Tick& min_tick,
                          Tick& max_tick,
                          MidiKey& min_key,
                          MidiKey& max_key) const noexcept;

    // Human-readable snap description for UI/status displays, mirroring the
    // Python GridSnapSystem.get_snap_info helper.
    std::string snap_info() const { return snap_.snap_info(); }

    // Debug overlay toggles (crosshair + clicked-cell highlight). These are
    // primarily useful during development or when comparing against the
    // Python implementation's debug layers.
    void set_show_debug_crosshair(bool enabled) noexcept {
        show_debug_crosshair_ = enabled;
    }
    bool show_debug_crosshair() const noexcept {
        return show_debug_crosshair_;
    }

    // Visible ranges in tick/key space (for host-side fit/scroll logic).
    std::pair<Tick, Tick> visible_ticks() const noexcept {
        return coords_.visible_tick_range();
    }
    std::pair<MidiKey, MidiKey> visible_keys() const noexcept {
        return coords_.visible_key_range();
    }

    // Fit the horizontal view to the current MIDI clip bounds, mirroring the
    // scrollbar double-click behaviour from the Python implementation.
    void fit_view_to_clip() noexcept;

    // Fit view to the current note selection in both time and pitch. If no
    // notes are selected this is a no-op.
    void fit_view_to_selection() noexcept;

    // Keep ticks-per-beat in sync across coordinate system, snapping, and
    // renderer, preserving bar-relative defaults like clip length.
    void set_ticks_per_beat(int ticks) noexcept;

    // MIDI clip boundaries (for ruler brackets and scrollbar fit behaviour).
    void set_clip_bounds(Tick start, Tick end) noexcept;
    std::pair<Tick, Tick> clip_bounds() const noexcept {
        return {clip_start_tick_, clip_end_tick_};
    }

    // Convenience helper for host playback integration: advance a playback
    // position by delta_seconds at the given tempo (in BPM), applying the
    // widget's current ticks-per-beat and loop region (if enabled). The
    // playhead in the renderer is updated to the returned tick.
    //
    // Hosts are expected to hold the current playback tick externally:
    //
    //   Tick playhead = ...;
    //   if (is_playing) {
    //       playhead = widget.update_playback(playhead, tempo_bpm, dt);
    //   }
    //
    // Auto-scroll behaviour continues to be driven by the playhead value
    // inside draw(), mirroring the Python UnifiedPianoRoll.update_playback
    // semantics.
    Tick update_playback(Tick current_tick,
                         double tempo_bpm,
                         double delta_seconds) noexcept;

    using PlayheadChangedCallback = std::function<void(Tick)>;

    // Optional callback that is invoked whenever the widget updates the
    // playhead internally (e.g. via ruler clicks or update_playback). Host-
    // driven calls to set_playhead will also trigger this callback.
    void set_playhead_changed_callback(
        PlayheadChangedCallback cb) noexcept {
        on_playhead_changed_ = std::move(cb);
    }

    // Draw the widget inside the current Dear ImGui window. This must be
    // called after an ImGui window is begun. When built without
    // PIANO_ROLL_USE_IMGUI, this function is a no-op.
    void draw();

private:
    NoteManager notes_;
    CoordinateSystem coords_;
    GridSnapSystem snap_;
    PianoRollRenderConfig config_;
    PianoRollRenderer renderer_;
    PointerTool pointer_;
    KeyboardController keyboard_;
    LoopMarkerRectangle loop_markers_;
    std::vector<ControlLane> cc_lanes_;
    int active_cc_lane_{-1};
    bool cc_dragging_{false};
    int cc_drag_index_{-1};

    bool dragging_playback_start_{false};
    bool dragging_cue_left_{false};
    bool dragging_cue_right_{false};

    void handle_cc_pointer_events(float local_x,
                                  float local_y,
                                  float lane_top_local,
                                  float lane_bottom_local,
                                  const ModifierKeys& mods);

    void handle_pointer_events();
    void handle_keyboard_events();

    // Scrollbar-related helpers.
    void update_scrollbar_geometry();
    void handle_scrollbar_events();
    bool check_rectangle_edge_scrolling(float local_x, float local_y);

    // Viewport helpers.
    void ensure_selected_notes_visible();

    CustomScrollbar h_scrollbar_{ScrollbarOrientation::Horizontal};
    double explored_min_x_{0.0};
    double explored_max_x_{0.0};

    // Clip boundaries for scrollbar double-click behaviour.
    Tick clip_start_tick_{0};
    Tick clip_end_tick_{4 * 4 * 480};  // Default 4 bars at 480 TPB

    // Playback markers (start and cue positions); purely visual and driven by
    // the host transport for now.
    Tick playback_start_tick_{0};
    bool show_playback_start_marker_{false};
    Tick cue_left_tick_{0};
    Tick cue_right_tick_{0};
    bool show_cue_markers_{false};

    PlayheadChangedCallback on_playhead_changed_{};

    // Scrollbar handler equivalents from Python.
    void handle_scrollbar_scroll(double new_scroll);
    void handle_scrollbar_edge_resize(const char* edge, double delta_x);
    void handle_scrollbar_double_click();
    void handle_scrollbar_drag_end();

    // Explored-area helpers transliterated from Python.
    void expand_explored_area(double new_x);
    void update_explored_area_for_notes();

    // Ruler and note-name interaction state (partial port from Python).
    bool ruler_interaction_active_{false};
    bool ruler_pan_active_{false};
    bool horizontal_zoom_active_{false};
    double ruler_start_x_{0.0};
    double ruler_start_y_{0.0};
    double ruler_start_viewport_x_{0.0};
    double ruler_initial_mouse_x_{0.0};
    double ruler_initial_mouse_y_{0.0};
    double horizontal_zoom_start_pixels_per_beat_{60.0};
    double horizontal_zoom_start_y_{0.0};
    double horizontal_zoom_anchor_x_{0.0};
    double horizontal_zoom_anchor_beat_{0.0};

    bool note_names_interaction_active_{false};
    bool note_names_pan_active_{false};
    bool vertical_zoom_active_{false};
    double note_names_start_x_{0.0};
    double note_names_start_y_{0.0};
    double vertical_zoom_anchor_y_{0.0};
    double note_names_start_viewport_y_{0.0};
    double note_names_initial_mouse_x_{0.0};
    double note_names_initial_mouse_y_{0.0};
    double vertical_zoom_start_pixels_per_key_{20.0};
    double vertical_zoom_start_x_{0.0};

    // Layout parameters (approximated from Python).
    float top_padding_{0.0f};
    float ruler_height_{24.0f};      // renderer uses 24px for ruler
    float footer_height_{0.0f};
    float note_label_width_{180.0f}; // left label column

    // Debug crosshair overlay (partial port of Python debug layer).
    bool show_debug_crosshair_{true};
    float debug_mouse_x_local_{-1.0f};
    float debug_mouse_y_local_{-1.0f};

    // Debug clicked-cell highlight (for coordinate verification).
    bool has_last_clicked_cell_{false};
    Tick last_clicked_tick_start_{0};
    Tick last_clicked_tick_end_{0};
    MidiKey last_clicked_key_{60};

    // Piano-key hover/press state for visual feedback.
    bool has_hovered_piano_key_{false};
    MidiKey hovered_piano_key_{60};
    bool has_pressed_piano_key_{false};
    MidiKey pressed_piano_key_{60};
};

}  // namespace piano_roll
