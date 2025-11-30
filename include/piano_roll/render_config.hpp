#pragma once

// This header intentionally avoids depending on Dear ImGui so the core library
// can compile without ImGui present. When PIANO_ROLL_USE_IMGUI is defined, the
// implementation will convert these colours to ImGui types.

namespace piano_roll {

struct ColorRGBA {
    float r{0.0f};
    float g{0.0f};
    float b{0.0f};
    float a{1.0f};

    constexpr ColorRGBA() = default;
    constexpr ColorRGBA(float red, float green, float blue, float alpha = 1.0f)
        : r(red), g(green), b(blue), a(alpha) {}
};

// Visual configuration for the piano roll renderer.
struct PianoRollRenderConfig {
    // Colors
    ColorRGBA background_color{0.10f, 0.10f, 0.10f, 1.0f};
    ColorRGBA white_key_color{0.18f, 0.18f, 0.18f, 1.0f};
    ColorRGBA black_key_color{0.12f, 0.12f, 0.12f, 1.0f};

    ColorRGBA grid_line_color{0.18f, 0.18f, 0.18f, 1.0f};
    ColorRGBA beat_line_color{0.26f, 0.26f, 0.26f, 1.0f};
    ColorRGBA bar_line_color{0.32f, 0.32f, 0.32f, 1.0f};
    ColorRGBA subdivision_line_color{0.20f, 0.20f, 0.20f, 1.0f};

    ColorRGBA note_fill_color{0.24f, 0.58f, 0.96f, 1.0f};
    ColorRGBA note_border_color{0.24f, 0.58f, 0.96f, 1.0f};
    ColorRGBA selected_note_fill_color{0.98f, 0.82f, 0.25f, 1.0f};
    ColorRGBA selected_note_border_color{0.98f, 0.82f, 0.25f, 1.0f};
    ColorRGBA selected_note_inner_border_color{1.0f, 1.0f, 1.0f, 1.0f};
    ColorRGBA note_label_text_color{0.90f, 0.90f, 0.90f, 1.0f};

    ColorRGBA ruler_background_color{0.15f, 0.15f, 0.15f, 1.0f};
    ColorRGBA ruler_text_color{0.90f, 0.90f, 0.90f, 1.0f};
    ColorRGBA ruler_clip_boundary_color{0.78f, 0.78f, 0.78f, 1.0f};

    // Playback markers in the ruler.
    ColorRGBA playback_start_marker_color{0.0f, 0.59f, 1.0f, 1.0f};
    ColorRGBA cue_marker_color{0.0f, 0.59f, 1.0f, 1.0f};

    // Loop region (loop markers) in the ruler.
    ColorRGBA loop_region_fill_color{
        160.0f / 255.0f,
        160.0f / 255.0f,
        160.0f / 255.0f,
        1.0f};
    ColorRGBA loop_region_hover_fill_color{
        200.0f / 255.0f,
        200.0f / 255.0f,
        200.0f / 255.0f,
        1.0f};
    ColorRGBA loop_region_handle_hover_color{
        1.0f,
        200.0f / 255.0f,
        0.0f,
        1.0f};

    // Selection rectangle overlay
    ColorRGBA selection_rect_fill_color{1.0f, 1.0f, 1.0f, 0.10f};
    ColorRGBA selection_rect_border_color{1.0f, 1.0f, 1.0f, 0.30f};

    // Playhead
    ColorRGBA playhead_color{1.0f, 1.0f, 0.0f, 1.0f};
    bool playhead_auto_scroll{false};
    float playhead_auto_scroll_margin{100.0f};

    // Spotlight for selected notes (background band + edge lines).
    ColorRGBA spotlight_fill_color{1.0f, 1.0f, 1.0f, 0.05f};
    ColorRGBA spotlight_edge_color{1.0f, 1.0f, 1.0f, 0.9f};

    // Drag preview rectangles (move vs duplicate).
    ColorRGBA drag_preview_move_color{0.31f, 0.47f, 0.78f, 0.70f};
    ColorRGBA drag_preview_duplicate_color{0.31f, 0.78f, 0.47f, 0.70f};

    // CC lane (MIDI continuous controller) area under the notes grid
    bool show_cc_lane{true};
    float cc_lane_height{120.0f};  // pixels
    ColorRGBA cc_lane_background_color{0.08f, 0.08f, 0.08f, 1.0f};
    ColorRGBA cc_lane_border_color{0.25f, 0.25f, 0.25f, 1.0f};
    ColorRGBA cc_curve_color{0.35f, 0.75f, 0.95f, 1.0f};
    ColorRGBA cc_point_color{1.0f, 1.0f, 1.0f, 1.0f};

    // Geometry
    float note_corner_radius{3.0f};
    float grid_line_thickness{1.0f};
    float beat_line_thickness{1.0f};
    float bar_line_thickness{1.5f};
    float note_border_thickness{1.0f};

    // Apply a light theme base palette approximating the Python
    // UnifiedPianoRoll light theme. This adjusts background, key, grid,
    // and ruler colours but does not change note/marker colours; those
    // remain controlled by apply_clip_color.
    void apply_light_theme_base() noexcept {
        // Background and key area (light greys).
        background_color = ColorRGBA{
            170.0f / 255.0f,
            170.0f / 255.0f,
            170.0f / 255.0f,
            1.0f};
        white_key_color = ColorRGBA{
            240.0f / 255.0f,
            240.0f / 255.0f,
            240.0f / 255.0f,
            1.0f};
        black_key_color = ColorRGBA{
            40.0f / 255.0f,
            40.0f / 255.0f,
            40.0f / 255.0f,
            1.0f};

        // Grid lines (slightly darker greys for contrast).
        grid_line_color = ColorRGBA{
            120.0f / 255.0f,
            120.0f / 255.0f,
            120.0f / 255.0f,
            1.0f};
        beat_line_color = ColorRGBA{
            100.0f / 255.0f,
            100.0f / 255.0f,
            100.0f / 255.0f,
            1.0f};
        bar_line_color = ColorRGBA{
            80.0f / 255.0f,
            80.0f / 255.0f,
            80.0f / 255.0f,
            1.0f};
        subdivision_line_color = grid_line_color;

        // Ruler: dark strip with dark text (matching Python's ruler_color and ruler_text_color).
        ruler_background_color = ColorRGBA{
            50.0f / 255.0f,
            50.0f / 255.0f,
            50.0f / 255.0f,
            1.0f};
        ruler_text_color = ColorRGBA{0.0f, 0.0f, 0.0f, 1.0f};

        // CC lane background: slightly darker than main background for separation.
        cc_lane_background_color = ColorRGBA{
            150.0f / 255.0f,
            150.0f / 255.0f,
            150.0f / 255.0f,
            1.0f};
        cc_lane_border_color = ColorRGBA{
            110.0f / 255.0f,
            110.0f / 255.0f,
            110.0f / 255.0f,
            1.0f};
    }

    // Apply a Bitwig-style clip colour theme to the core note and marker
    // colours, loosely mirroring UnifiedPianoRoll._update_colors_from_clip_color.
    void apply_clip_color(const ColorRGBA& clip) noexcept {
        note_fill_color = clip;

        // Selected note: darker version of clip colour.
        selected_note_fill_color = ColorRGBA{
            clip.r * 0.5f,
            clip.g * 0.5f,
            clip.b * 0.5f,
            clip.a};

        // Border: even darker.
        note_border_color = ColorRGBA{
            clip.r * (1.0f / 3.0f),
            clip.g * (1.0f / 3.0f),
            clip.b * (1.0f / 3.0f),
            clip.a};
        selected_note_border_color = note_border_color;

        // Inner border for selected notes: lighter version of clip colour.
        auto clamp01 = [](float v) {
            return v < 0.0f ? 0.0f : (v > 1.0f ? 1.0f : v);
        };
        selected_note_inner_border_color = ColorRGBA{
            clamp01(clip.r * 1.7f),
            clamp01(clip.g * 1.7f),
            clamp01(clip.b * 1.7f),
            clip.a};

        // Cue/playback markers follow the clip colour for consistency.
        cue_marker_color = clip;
        playback_start_marker_color = clip;
    }

    // Convenience helper that first applies the light theme base palette and
    // then derives note/marker colours from the given clip colour, providing
    // a close approximation of UnifiedPianoRoll's light theme driven by
    // clip colour.
    void apply_light_theme_from_clip_color(const ColorRGBA& clip) noexcept {
        apply_light_theme_base();
        apply_clip_color(clip);
    }
};

}  // namespace piano_roll
