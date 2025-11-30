#pragma once

#include "piano_roll/coordinate_system.hpp"
#include "piano_roll/grid_snap.hpp"
#include "piano_roll/note_manager.hpp"
#include "piano_roll/render_config.hpp"

namespace piano_roll {

// Basic renderer that draws the piano roll into a Dear ImGui window
// when built with PIANO_ROLL_USE_IMGUI. Without ImGui, render() is a no-op.
class PianoRollRenderer {
public:
    explicit PianoRollRenderer(PianoRollRenderConfig config = {});

    const PianoRollRenderConfig& config() const noexcept { return config_; }
    PianoRollRenderConfig& config() noexcept { return config_; }

    void set_ticks_per_beat(int ticks) noexcept {
        grid_snap_.set_ticks_per_beat(ticks);
    }

    // Optional playhead rendering. When enabled, the renderer draws a vertical
    // line at the given tick position.
    void set_playhead(Tick tick) noexcept {
        playhead_tick_ = tick;
        has_playhead_ = true;
    }

    void clear_playhead() noexcept {
        has_playhead_ = false;
    }

    bool has_playhead() const noexcept { return has_playhead_; }
    Tick playhead_tick() const noexcept { return playhead_tick_; }

    // Render the entire piano roll into the current ImGui window.
    // The widget will consume a region of size (content_width, viewport.height),
    // where content_width is derived from the coordinate system.
    void render(const CoordinateSystem& coords,
                const NoteManager& notes);

private:
    PianoRollRenderConfig config_;
    GridSnapSystem grid_snap_;

    bool has_playhead_{false};
    Tick playhead_tick_{0};

#ifdef PIANO_ROLL_USE_IMGUI
    // Layer-style helpers used internally by render() to mirror the logical
    // separation in the Python render_system: background, notes, ruler, etc.
    void render_background_layer(ImDrawList* draw_list,
                                 const CoordinateSystem& coords,
                                 const Viewport& vp,
                                 const ImVec2& origin,
                                 const NoteManager& notes) const;

    void render_notes_layer(ImDrawList* draw_list,
                            const CoordinateSystem& coords,
                            const Viewport& vp,
                            const ImVec2& origin,
                            const NoteManager& notes) const;

    void render_ruler_layer(ImDrawList* draw_list,
                            const CoordinateSystem& coords,
                            const Viewport& vp,
                            const ImVec2& origin) const;

    void render_playhead_layer(ImDrawList* draw_list,
                               const CoordinateSystem& coords,
                               const Viewport& vp,
                               const ImVec2& origin) const;
#endif
};

}  // namespace piano_roll
