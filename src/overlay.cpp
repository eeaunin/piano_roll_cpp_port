#include "piano_roll/overlay.hpp"

#ifdef PIANO_ROLL_USE_IMGUI
#include <imgui.h>
#endif

namespace piano_roll {

void RenderSelectionOverlay(const NoteManager& notes,
                            const PointerTool& tool,
                            const CoordinateSystem& coords,
                            const PianoRollRenderConfig& config) {
#ifdef PIANO_ROLL_USE_IMGUI
    ImDrawList* draw_list = ImGui::GetWindowDrawList();
    if (!draw_list) {
        return;
    }

    // The origin for the last item (piano roll) is the top-left rect min.
    ImVec2 origin = ImGui::GetItemRectMin();

    auto to_imvec4 = [](const ColorRGBA& c) {
        return ImVec4(c.r, c.g, c.b, c.a);
    };
    auto to_color = [&](const ColorRGBA& c) {
        return ImGui::ColorConvertFloat4ToU32(to_imvec4(c));
    };

    // Selection rectangle.
    if (tool.has_selection_rectangle()) {
        double wx1{}, wy1{}, wx2{}, wy2{};
        tool.selection_rectangle_world(wx1, wy1, wx2, wy2);

        auto [sx1_local, sy1_local] =
            coords.world_to_screen(wx1, wy1);
        auto [sx2_local, sy2_local] =
            coords.world_to_screen(wx2, wy2);

        float x1_local = static_cast<float>(sx1_local);
        float x2_local = static_cast<float>(sx2_local);
        float y1_local = static_cast<float>(sy1_local);
        float y2_local = static_cast<float>(sy2_local);

        if (x2_local > x1_local && y2_local > y1_local) {
            float grid_left =
                static_cast<float>(coords.piano_key_width());
            float grid_top = 0.0f;  // world-space Y already accounts for scroll
            float grid_right =
                grid_left +
                static_cast<float>(coords.viewport().width);
            float grid_bottom =
                static_cast<float>(coords.viewport().height);

            x1_local = std::max(x1_local, grid_left);
            x2_local = std::min(x2_local, grid_right);
            y1_local = std::max(y1_local, grid_top);
            y2_local = std::min(y2_local, grid_bottom);

            if (x2_local > x1_local && y2_local > y1_local) {
                ImVec2 min(origin.x + x1_local,
                           origin.y + y1_local);
                ImVec2 max(origin.x + x2_local,
                           origin.y + y2_local);

                draw_list->AddRectFilled(
                    min,
                    max,
                    to_color(config.selection_rect_fill_color));
                draw_list->AddRect(
                    min,
                    max,
                    to_color(config.selection_rect_border_color),
                    0.0f,
                    0,
                    1.0f);
            }
        }
    }

    // Hover highlight for note edges/body.
    double hx1{}, hy1{}, hx2{}, hy2{};
    HoverEdge edge{};
    if (tool.hovered_note_world(hx1, hy1, hx2, hy2, edge) &&
        edge != HoverEdge::None) {
        auto [sx1_local, sy1_local] =
            coords.world_to_screen(hx1, hy1);
        auto [sx2_local, sy2_local] =
            coords.world_to_screen(hx2, hy2);

        float x1_local = static_cast<float>(sx1_local);
        float x2_local = static_cast<float>(sx2_local);
        float y1_local = static_cast<float>(sy1_local);
        float y2_local = static_cast<float>(sy2_local);
        if (x2_local > x1_local && y2_local > y1_local) {
            float edge_thickness = 8.0f;
            float ex1 = x1_local;
            float ex2 = x2_local;
            switch (edge) {
            case HoverEdge::Left:
                ex2 = std::min(x2_local, x1_local + edge_thickness);
                break;
            case HoverEdge::Right:
                ex1 = std::max(x1_local, x2_local - edge_thickness);
                break;
            case HoverEdge::Body:
            default:
                break;
            }

            ImVec2 hmin(origin.x + ex1, origin.y + y1_local);
            ImVec2 hmax(origin.x + ex2, origin.y + y2_local);
            ColorRGBA hover_col = config.selected_note_border_color;
            hover_col.a = 0.7f;
            draw_list->AddRectFilled(hmin,
                                     hmax,
                                     to_color(hover_col));
        }
    }

    // Drag preview for moving / duplicating notes: draw overlays for all
    // selected notes with different colours for duplication.
    if (tool.is_dragging_note() || tool.is_resizing_note()) {
        bool duplicating = tool.is_duplicating();
        ColorRGBA base =
            duplicating ? config.drag_preview_duplicate_color
                        : config.drag_preview_move_color;

        for (const Note& n : notes.notes()) {
            if (!n.selected) {
                continue;
            }

            double wx1 = coords.tick_to_world(n.tick);
            double wx2 = coords.tick_to_world(n.end_tick());
            double wy1 = coords.key_to_world_y(n.key);
            double wy2 = wy1 + coords.key_height();

            auto [sx1_local, sy1_local] =
                coords.world_to_screen(wx1, wy1);
            auto [sx2_local, sy2_local] =
                coords.world_to_screen(wx2, wy2);

            float x1_local = static_cast<float>(sx1_local);
            float x2_local = static_cast<float>(sx2_local);
            float y1_local = static_cast<float>(sy1_local);
            float y2_local = static_cast<float>(sy2_local);
            if (x2_local <= x1_local || y2_local <= y1_local) {
                continue;
            }

            ImVec2 pmin(origin.x + x1_local,
                        origin.y + y1_local);
            ImVec2 pmax(origin.x + x2_local,
                        origin.y + y2_local);
            draw_list->AddRectFilled(pmin,
                                     pmax,
                                     to_color(base));
        }
    }
#else
    (void)notes;
    (void)tool;
    (void)coords;
    (void)config;
#endif
}

}  // namespace piano_roll
