#include "piano_roll/renderer.hpp"

#include <algorithm>

#ifdef PIANO_ROLL_USE_IMGUI
#include <imgui.h>
#endif

namespace piano_roll {

PianoRollRenderer::PianoRollRenderer(PianoRollRenderConfig config)
    : config_(config),
      grid_snap_(/*ticks_per_beat=*/480) {}

void PianoRollRenderer::render(const CoordinateSystem& coords,
                               const NoteManager& notes) {
    render(coords, notes,
           /*draw_background=*/true,
           /*draw_notes=*/true,
           /*draw_ruler=*/true,
           /*draw_playhead=*/true);
}

void PianoRollRenderer::render(const CoordinateSystem& coords,
                               const NoteManager& notes,
                               bool draw_background,
                               bool draw_notes,
                               bool draw_ruler,
                               bool draw_playhead) {
#ifdef PIANO_ROLL_USE_IMGUI
    // Full Dear ImGui rendering path.
    ImGuiWindow* window = ImGui::GetCurrentWindow();
    if (!window || window->SkipItems) {
        return;
    }

    ImDrawList* draw_list = ImGui::GetWindowDrawList();
    ImVec2 origin = ImGui::GetCursorScreenPos();

    const Viewport& vp = coords.viewport();

    auto to_imvec4 = [](const ColorRGBA& c) {
        return ImVec4(c.r, c.g, c.b, c.a);
    };
    auto to_color = [&](const ColorRGBA& c) {
        return ImGui::ColorConvertFloat4ToU32(to_imvec4(c));
    };

    // Reserve the layout space for the widget.
    ImVec2 widget_size{
        static_cast<float>(coords.piano_key_width() + vp.width),
        static_cast<float>(vp.height)};
    ImRect bb(origin, ImVec2(origin.x + widget_size.x,
                             origin.y + widget_size.y));
    ImGui::ItemSize(bb);
    if (!ImGui::ItemAdd(bb, 0)) {
        return;
    }

    // Use ImDrawList channels to approximate per-layer draw lists, so that
    // background, notes, ruler, and playhead can be rendered in distinct
    // layers while still sharing a single Dear ImGui draw list. This gives us
    // predictable z-ordering similar to the Python RenderSystem layers, while
    // allowing callers to select a subset of layers.
    constexpr int kLayerBackground = 0;
    constexpr int kLayerNotes = 1;
    constexpr int kLayerRuler = 2;
    constexpr int kLayerPlayhead = 3;

    draw_list->ChannelsSplit(4);

    draw_list->ChannelsSetCurrent(kLayerBackground);
    if (draw_background) {
        render_background_layer(draw_list, coords, vp, origin, notes);
    }

    draw_list->ChannelsSetCurrent(kLayerNotes);
    if (draw_notes) {
        render_notes_layer(draw_list, coords, vp, origin, notes);
    }

    draw_list->ChannelsSetCurrent(kLayerRuler);
    if (draw_ruler) {
        render_ruler_layer(draw_list, coords, vp, origin);
    }

    draw_list->ChannelsSetCurrent(kLayerPlayhead);
    if (draw_playhead) {
        render_playhead_layer(draw_list, coords, vp, origin);
    }

    draw_list->ChannelsMerge();
#else
    (void)coords;
    (void)notes;
    // Built without Dear ImGui; nothing to render.
#endif
}

}  // namespace piano_roll

#ifdef PIANO_ROLL_USE_IMGUI
namespace piano_roll {

void PianoRollRenderer::render_background_layer(
    ImDrawList* draw_list,
    const CoordinateSystem& coords,
    const Viewport& vp,
    const ImVec2& origin,
    const NoteManager& notes) const {
    auto to_imvec4 = [](const ColorRGBA& c) {
        return ImVec4(c.r, c.g, c.b, c.a);
    };
    auto to_color = [&](const ColorRGBA& c) {
        return ImGui::ColorConvertFloat4ToU32(to_imvec4(c));
    };

    ImVec2 widget_min = origin;
    ImVec2 widget_max = ImVec2(
        origin.x +
            static_cast<float>(coords.piano_key_width() + vp.width),
        origin.y + static_cast<float>(vp.height));
    draw_list->AddRectFilled(widget_min, widget_max,
                             to_color(config_.background_color));

    // Piano key strip.
    float keys_left = origin.x;
    float keys_right =
        origin.x + static_cast<float>(coords.piano_key_width());

    auto key_range = coords.visible_key_range();
    MidiKey min_key = key_range.first;
    MidiKey max_key = key_range.second;
    for (MidiKey key = min_key; key <= max_key; ++key) {
        bool is_black = (key % 12 == 1 || key % 12 == 3 ||
                         key % 12 == 6 || key % 12 == 8 ||
                         key % 12 == 10);

        double world_y = coords.key_to_world_y(key);
        double world_y_next =
            world_y + coords.key_height();

        auto [_, sy1_local] =
            coords.world_to_screen(0.0, world_y);
        auto [__, sy2_local] =
            coords.world_to_screen(0.0, world_y_next);

        float y1 = origin.y + static_cast<float>(sy1_local);
        float y2 = origin.y + static_cast<float>(sy2_local);

        if (y2 < origin.y ||
            y1 > origin.y + static_cast<float>(vp.height)) {
            continue;
        }

        if (y1 < origin.y) y1 = origin.y;
        if (y2 > origin.y + static_cast<float>(vp.height)) {
            y2 = origin.y + static_cast<float>(vp.height);
        }

        draw_list->AddRectFilled(
            ImVec2(keys_left, y1),
            ImVec2(keys_right, y2),
            to_color(is_black ? config_.black_key_color
                              : config_.white_key_color));
    }

    // Key row zebra stripes in grid area.
    float grid_left = origin.x +
                      static_cast<float>(coords.piano_key_width());
    float grid_right = origin.x +
                       static_cast<float>(coords.piano_key_width() +
                                          vp.width);

    ColorRGBA row_light = config_.background_color;
    row_light.r *= 1.15f;
    row_light.g *= 1.15f;
    row_light.b *= 1.15f;
    ColorRGBA row_dark = config_.background_color;
    row_dark.r *= 0.95f;
    row_dark.g *= 0.95f;
    row_dark.b *= 0.95f;

    for (MidiKey key = min_key; key <= max_key; ++key) {
        bool is_black = (key % 12 == 1 || key % 12 == 3 ||
                         key % 12 == 6 || key % 12 == 8 ||
                         key % 12 == 10);

        double world_y = coords.key_to_world_y(key);
        double world_y_next =
            world_y + coords.key_height();

        auto [_, sy1_local] =
            coords.world_to_screen(0.0, world_y);
        auto [__, sy2_local] =
            coords.world_to_screen(0.0, world_y_next);

        float y1 = origin.y + static_cast<float>(sy1_local);
        float y2 = origin.y + static_cast<float>(sy2_local);

        if (y2 < origin.y ||
            y1 > origin.y + static_cast<float>(vp.height)) {
            continue;
        }

        if (y1 < origin.y) y1 = origin.y;
        if (y2 > origin.y + static_cast<float>(vp.height)) {
            y2 = origin.y + static_cast<float>(vp.height);
        }

        draw_list->AddRectFilled(
            ImVec2(grid_left, y1),
            ImVec2(grid_right, y2),
            to_color(is_black ? row_dark : row_light));
    }

    // Spotlight band behind selected notes.
    const auto& all_notes = notes.notes();
    double min_x_world = 0.0;
    double max_x_world = 0.0;
    bool have_selection = false;
    for (const Note& n : all_notes) {
        if (!n.selected) {
            continue;
        }
        double x1w = coords.tick_to_world(n.tick);
        double x2w = coords.tick_to_world(n.end_tick());
        if (!have_selection) {
            min_x_world = x1w;
            max_x_world = x2w;
            have_selection = true;
        } else {
            min_x_world = std::min(min_x_world, x1w);
            max_x_world = std::max(max_x_world, x2w);
        }
    }
    if (have_selection && max_x_world > min_x_world) {
        auto [sx1_local, sy1_local] =
            coords.world_to_screen(min_x_world, 0.0);
        auto [sx2_local, sy2_local] =
            coords.world_to_screen(max_x_world, 0.0);
        (void)sy1_local;
        (void)sy2_local;
        float x1 = origin.x + static_cast<float>(sx1_local);
        float x2 = origin.x + static_cast<float>(sx2_local);
        float grid_top = origin.y;
        float grid_bottom =
            origin.y + static_cast<float>(vp.height);
        float grid_left_px =
            origin.x + static_cast<float>(coords.piano_key_width());
        float grid_right_px =
            origin.x + static_cast<float>(coords.piano_key_width() +
                                          vp.width);
        x1 = std::max(x1, grid_left_px);
        x2 = std::min(x2, grid_right_px);
        if (x2 > x1) {
            draw_list->AddRectFilled(
                ImVec2(x1, grid_top),
                ImVec2(x2, grid_bottom),
                to_color(config_.spotlight_fill_color));
            ImU32 edge_col =
                to_color(config_.spotlight_edge_color);
            draw_list->AddLine(ImVec2(x1, grid_top),
                               ImVec2(x1, grid_bottom),
                               edge_col,
                               1.0f);
            draw_list->AddLine(ImVec2(x2, grid_top),
                               ImVec2(x2, grid_bottom),
                               edge_col,
                               1.0f);
        }
    }
}

void PianoRollRenderer::render_notes_layer(
    ImDrawList* draw_list,
    const CoordinateSystem& coords,
    const Viewport& vp,
    const ImVec2& origin,
    const NoteManager& notes) const {
    auto to_imvec4 = [](const ColorRGBA& c) {
        return ImVec4(c.r, c.g, c.b, c.a);
    };
    auto to_color = [&](const ColorRGBA& c) {
        return ImGui::ColorConvertFloat4ToU32(to_imvec4(c));
    };

    float left_limit = origin.x +
                       static_cast<float>(coords.piano_key_width());
    float right_limit = origin.x +
                        static_cast<float>(coords.piano_key_width() +
                                           vp.width);

    auto draw_single_note = [&](const Note& note) {
        double world_x1 = coords.tick_to_world(note.tick);
        double world_x2 = coords.tick_to_world(note.end_tick());
        double world_y = coords.key_to_world_y(note.key);
        double world_y2 = world_y + coords.key_height();

        auto [screen_x1_local, screen_y1_local] =
            coords.world_to_screen(world_x1, world_y);
        auto [screen_x2_local, screen_y2_local] =
            coords.world_to_screen(world_x2, world_y2);

        float x1 = origin.x + static_cast<float>(screen_x1_local);
        float x2 = origin.x + static_cast<float>(screen_x2_local);
        float y1 = origin.y + static_cast<float>(screen_y1_local);
        float y2 = origin.y + static_cast<float>(screen_y2_local);

        x1 = std::max(x1, left_limit);
        x2 = std::min(x2, right_limit);
        if (x2 <= x1) {
            return;
        }

        ImVec2 min{x1, y1};
        ImVec2 max{x2, y2};

        const bool selected = note.selected;
        const ColorRGBA& fill =
            selected ? config_.selected_note_fill_color
                     : config_.note_fill_color;
        const ColorRGBA& border =
            selected ? config_.selected_note_border_color
                     : config_.note_border_color;

        if (!selected) {
            float shadow_offset = 1.0f;
            ImVec2 smin(min.x + shadow_offset,
                        min.y + shadow_offset);
            ImVec2 smax(max.x + shadow_offset,
                        max.y + shadow_offset);
            ImU32 shadow_fill =
                ImGui::GetColorU32(
                    ImVec4(0.0f, 0.0f, 0.0f, 0.12f));
            draw_list->AddRectFilled(smin,
                                     smax,
                                     shadow_fill,
                                     config_.note_corner_radius);
        }

        draw_list->AddRectFilled(min, max,
                                 to_color(fill),
                                 config_.note_corner_radius);
        draw_list->AddRect(min, max,
                           to_color(border),
                           config_.note_corner_radius,
                           0,
                           config_.note_border_thickness);

        if (selected) {
            float inset = 2.0f;
            ImVec2 inner_min(min.x + inset, min.y + inset);
            ImVec2 inner_max(max.x - inset, max.y - inset);
            draw_list->AddRect(
                inner_min,
                inner_max,
                to_color(
                    config_.selected_note_inner_border_color),
                config_.note_corner_radius,
                0,
                1.0f);
        }
    };

    // Draw non-selected notes first, then selected notes so that selected
    // notes (and their borders) appear on top of overlapping unselected notes.
    for (const Note& note : notes.notes()) {
        if (!note.selected) {
            draw_single_note(note);
        }
    }
    for (const Note& note : notes.notes()) {
        if (note.selected) {
            draw_single_note(note);
        }
    }

    if (coords.key_height() >= 16.0) {
        ImFont* font = ImGui::GetFont();
        float font_size =
            font ? font->FontSize : ImGui::GetFontSize();
        auto note_name = [](MidiKey key) {
            static const char* names[12] = {
                "C", "C#", "D", "D#",
                "E", "F",  "F#", "G",
                "G#", "A", "A#", "B"};
            int idx = key % 12;
            int octave = key / 12 - 2;
            std::string s = names[idx];
            s += std::to_string(octave);
            return s;
        };

        for (const Note& note : notes.notes()) {
            double world_x1 = coords.tick_to_world(note.tick);
            double world_x2 =
                coords.tick_to_world(note.end_tick());
            double world_y = coords.key_to_world_y(note.key);
            double world_y2 = world_y + coords.key_height();

            auto [sx1_local, sy1_local] =
                coords.world_to_screen(world_x1, world_y);
            auto [sx2_local, sy2_local] =
                coords.world_to_screen(world_x2, world_y2);

            float x1 = origin.x +
                       static_cast<float>(sx1_local);
            float x2 = origin.x +
                       static_cast<float>(sx2_local);
            float y1 = origin.y +
                       static_cast<float>(sy1_local);
            float y2 = origin.y +
                       static_cast<float>(sy2_local);

            float left_limit = origin.x +
                               static_cast<float>(
                                   coords.piano_key_width());
            float right_limit =
                origin.x +
                static_cast<float>(coords.piano_key_width() +
                                   vp.width);
            if (x2 <= left_limit || x1 >= right_limit) {
                continue;
            }
            if (x1 < left_limit) x1 = left_limit;
            if (x2 > right_limit) x2 = right_limit;

            float width = x2 - x1;
            if (width < 30.0f) {
                continue;
            }

            std::string label = note_name(note.key);
            float text_x =
                origin.x + static_cast<float>(sx1_local) +
                4.0f;
            float text_y =
                y1 + (y2 - y1 - font_size) * 0.5f;

            ImU32 col = ImGui::ColorConvertFloat4ToU32(
                ImVec4(config_.note_label_text_color.r,
                       config_.note_label_text_color.g,
                       config_.note_label_text_color.b,
                       config_.note_label_text_color.a));
            draw_list->AddText(
                font,
                font_size,
                ImVec2(text_x, text_y),
                col,
                label.c_str());
        }
    }
}

void PianoRollRenderer::render_ruler_layer(
    ImDrawList* draw_list,
    const CoordinateSystem& coords,
    const Viewport& vp,
    const ImVec2& origin) const {
    auto to_imvec4 = [](const ColorRGBA& c) {
        return ImVec4(c.r, c.g, c.b, c.a);
    };
    auto to_color = [&](const ColorRGBA& c) {
        return ImGui::ColorConvertFloat4ToU32(to_imvec4(c));
    };

    grid_snap_.set_ticks_per_beat(coords.ticks_per_beat());
    auto tick_range = coords.visible_tick_range();
    Tick start_tick = tick_range.first;
    Tick end_tick = tick_range.second;
    double ppb = coords.pixels_per_beat();

    auto lines = grid_snap_.grid_lines(start_tick, end_tick, ppb);

    float top = origin.y;
    float bottom =
        origin.y + static_cast<float>(vp.height);

    for (const GridLine& line : lines) {
        double world_x = coords.tick_to_world(line.tick);
        auto [screen_x_local, _] =
            coords.world_to_screen(world_x, 0.0);
        float x = origin.x +
                  static_cast<float>(screen_x_local);

        const ColorRGBA* color_ptr =
            &config_.grid_line_color;
        float thickness = config_.grid_line_thickness;

        switch (line.type) {
        case GridLineType::Measure:
            color_ptr = &config_.bar_line_color;
            thickness = config_.bar_line_thickness;
            break;
        case GridLineType::Beat:
            color_ptr = &config_.beat_line_color;
            thickness = config_.beat_line_thickness;
            break;
        case GridLineType::Subdivision:
            color_ptr = &config_.subdivision_line_color;
            thickness = config_.grid_line_thickness *
                        0.8f;
            break;
        }

        draw_list->AddLine(ImVec2(x, top),
                           ImVec2(x, bottom),
                           to_color(*color_ptr),
                           thickness);
    }

    auto key_range = coords.visible_key_range();
    MidiKey min_key = key_range.first;
    MidiKey max_key = key_range.second;

    float left = origin.x +
                 static_cast<float>(
                     coords.piano_key_width());
    float right = origin.x +
                  static_cast<float>(
                      coords.piano_key_width() +
                      vp.width);

    for (MidiKey key = min_key; key <= max_key; ++key) {
        double world_y = coords.key_to_world_y(key);
        auto [_, screen_y_local] =
            coords.world_to_screen(0.0, world_y);
        float y = origin.y +
                  static_cast<float>(screen_y_local);

        draw_list->AddLine(ImVec2(left, y),
                           ImVec2(right, y),
                           to_color(config_.grid_line_color),
                           config_.grid_line_thickness);
    }

    float ruler_height = 24.0f;

    ImVec2 ruler_min{
        origin.x +
            static_cast<float>(
                coords.piano_key_width()),
        origin.y};
    ImVec2 ruler_max{
        origin.x +
            static_cast<float>(
                coords.piano_key_width() + vp.width),
        origin.y + ruler_height};

    draw_list->AddRectFilled(
        ruler_min,
        ruler_max,
        to_color(config_.ruler_background_color));

    auto labels =
        grid_snap_.ruler_labels(start_tick, end_tick, ppb);
    for (const RulerLabel& label : labels) {
        double world_x = coords.tick_to_world(label.tick);
        auto [screen_x_local, _] =
            coords.world_to_screen(world_x, 0.0);
        float x = origin.x +
                  static_cast<float>(screen_x_local);

        ImVec2 text_pos{
            x + 2.0f,
            ruler_min.y + 4.0f};

        draw_list->AddText(
            text_pos,
            to_color(config_.ruler_text_color),
            label.text.c_str());
    }
}

void PianoRollRenderer::render_playhead_layer(
    ImDrawList* draw_list,
    const CoordinateSystem& coords,
    const Viewport& vp,
    const ImVec2& origin) const {
    if (!has_playhead_) {
        return;
    }

    auto to_imvec4 = [](const ColorRGBA& c) {
        return ImVec4(c.r, c.g, c.b, c.a);
    };
    auto to_color = [&](const ColorRGBA& c) {
        return ImGui::ColorConvertFloat4ToU32(to_imvec4(c));
    };

    double world_x = coords.tick_to_world(playhead_tick_);
    auto [screen_x_local, _] =
        coords.world_to_screen(world_x, 0.0);
    float x = origin.x +
              static_cast<float>(screen_x_local);

    float top_playhead = origin.y;
    float bottom_playhead =
        origin.y + static_cast<float>(vp.height);

    draw_list->AddLine(ImVec2(x, top_playhead),
                       ImVec2(x, bottom_playhead),
                       to_color(config_.playhead_color),
                       2.0f);

    float handle_size = 10.0f;
    float half = handle_size * 0.5f;
    ImVec2 p0(x, top_playhead);
    ImVec2 p1(x - half, top_playhead - half);
    ImVec2 p2(x + half, top_playhead - half);
    draw_list->AddTriangleFilled(
        p0,
        p1,
        p2,
        to_color(config_.playhead_color));
}

}  // namespace piano_roll
#endif
