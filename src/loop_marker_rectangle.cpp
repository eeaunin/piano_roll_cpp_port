#include "piano_roll/loop_marker_rectangle.hpp"

#include <algorithm>
#include <cmath>

#ifdef PIANO_ROLL_USE_IMGUI
#include <imgui.h>
#endif

namespace piano_roll {

LoopMarkerRectangle::LoopMarkerRectangle(CoordinateSystem* coords,
                                         Tick start_tick,
                                         Tick end_tick)
    : coords_(coords),
      start_tick_(start_tick),
      end_tick_(end_tick) {
    // Wider edge threshold and preview behaviour, mirroring the Python
    // LoopMarkerRectangle defaults.
    edge_threshold = 20.0;
    show_resize_handles = true;
    show_drag_preview = true;

    visible = true;
    enabled = true;

    update_bounds_from_ticks();
}

void LoopMarkerRectangle::set_coordinate_system(
    CoordinateSystem* coords) noexcept {
    coords_ = coords;
    update_bounds_from_ticks();
}

void LoopMarkerRectangle::set_layout(float top_padding,
                                     float ruler_height,
                                     double piano_key_width) noexcept {
    top_padding_ = top_padding;
    ruler_height_ = ruler_height;
    piano_key_width_ = piano_key_width;
    update_bounds_from_ticks();
}

void LoopMarkerRectangle::set_tick_range(Tick start,
                                         Tick end) noexcept {
    start_tick_ = start;
    end_tick_ = end;
    if (end_tick_ < start_tick_) {
        end_tick_ = start_tick_;
    }
    update_bounds_from_ticks();
}

void LoopMarkerRectangle::update_snap_parameters() noexcept {
    if (!coords_) {
        return;
    }

    // Snap to 1/4 beat, matching the Python snap_size=120 with TPB=480.
    const int tpb = coords_->ticks_per_beat();
    if (tpb <= 0) {
        return;
    }
    const double quarter_ticks =
        static_cast<double>(tpb) / 4.0;
    const double world_zero = coords_->tick_to_world(0);
    const double world_quarter =
        coords_->tick_to_world(
            static_cast<Tick>(quarter_ticks));
    const double snap_world =
        std::max(1.0, std::abs(world_quarter - world_zero));

    snap_enabled = true;
    snap_size = snap_world;
    min_width = snap_world;
}

void LoopMarkerRectangle::update_bounds_from_ticks() noexcept {
    if (!coords_) {
        return;
    }

    update_snap_parameters();

    const double start_world =
        coords_->tick_to_world(start_tick_);
    const double end_world =
        coords_->tick_to_world(end_tick_);

    // Vertical band in the middle of the ruler, mirroring the Python
    // implementation (40–65% of ruler height).
    const double ruler_top =
        static_cast<double>(top_padding_) +
        static_cast<double>(ruler_height_) * 0.40;
    const double ruler_bottom =
        static_cast<double>(top_padding_) +
        static_cast<double>(ruler_height_) * 0.65;

    bounds.left = start_world;
    bounds.right = end_world;
    bounds.top = ruler_top;
    bounds.bottom = ruler_bottom;
}

void LoopMarkerRectangle::update_ticks_from_bounds() noexcept {
    if (!coords_) {
        return;
    }

    const double left_world = bounds.left;
    const double right_world = bounds.right;

    const Tick raw_start =
        coords_->world_to_tick(left_world);
    const Tick raw_end =
        coords_->world_to_tick(right_world);

    const int tpb = coords_->ticks_per_beat();
    if (tpb <= 0) {
        start_tick_ = raw_start;
        end_tick_ = std::max(raw_start, raw_end);
        return;
    }

    const Tick quarter_ticks =
        static_cast<Tick>(tpb / 4);
    const auto round_to_grid = [quarter_ticks](Tick v) {
        if (quarter_ticks <= 0) {
            return v;
        }
        const double step =
            static_cast<double>(quarter_ticks);
        const double normalized =
            static_cast<double>(v) / step;
        const double rounded =
            std::round(normalized) * step;
        return static_cast<Tick>(rounded);
    };

    start_tick_ = round_to_grid(raw_start);
    end_tick_ = round_to_grid(raw_end);
    if (end_tick_ <= start_tick_) {
        end_tick_ = start_tick_ + quarter_ticks;
    }
}

std::optional<std::pair<double, double>>
LoopMarkerRectangle::screen_to_world(double x,
                                     double y) const {
    if (!coords_) {
        return std::nullopt;
    }

    // Screen coordinates here are widget-local (0 at item left/top).
    const double world_x =
        x - coords_->piano_key_width() + coords_->viewport().x;
    const double world_y = y;  // ruler does not scroll vertically
    return std::make_pair(world_x, world_y);
}

std::optional<std::pair<double, double>>
LoopMarkerRectangle::world_to_screen(double x,
                                     double y) const {
    if (!coords_) {
        return std::nullopt;
    }

    const double screen_x =
        x - coords_->viewport().x +
        coords_->piano_key_width();
    const double screen_y = y;
    return std::make_pair(screen_x, screen_y);
}

std::optional<RectangleBounds>
LoopMarkerRectangle::get_screen_bounds() const {
    if (!coords_) {
        return std::nullopt;
    }

    auto start =
        world_to_screen(bounds.left, bounds.top);
    auto end =
        world_to_screen(bounds.right, bounds.bottom);
    if (!start || !end) {
        return std::nullopt;
    }

    RectangleBounds out;
    out.left = start->first;
    out.top = start->second;
    out.right = end->first;
    out.bottom = end->second;
    return out;
}

std::optional<RectangleBounds>
LoopMarkerRectangle::world_to_screen_bounds(
    const RectangleBounds& b) const {
    if (!coords_) {
        return std::nullopt;
    }

    auto start = world_to_screen(b.left, b.top);
    auto end = world_to_screen(b.right, b.bottom);
    if (!start || !end) {
        return std::nullopt;
    }

    RectangleBounds out;
    out.left = start->first;
    out.top = start->second;
    out.right = end->first;
    out.bottom = end->second;
    return out;
}

void LoopMarkerRectangle::on_bounds_finalized() {
    update_ticks_from_bounds();
    // Re-sync bounds to the snapped tick range so vertical band and world
    // coordinates are consistent for subsequent interactions.
    update_bounds_from_ticks();
}

#ifdef PIANO_ROLL_USE_IMGUI
void LoopMarkerRectangle::render(
    void* draw_list_void,
    const PianoRollRenderConfig& config,
    float canvas_origin_x,
    float canvas_origin_y) const {
    ImDrawList* draw_list =
        static_cast<ImDrawList*>(draw_list_void);
    if (!draw_list || !visible) {
        return;
    }

    auto screen_bounds_opt = get_screen_bounds();
    if (!screen_bounds_opt) {
        return;
    }
    RectangleBounds screen_bounds = *screen_bounds_opt;

    const double local_min_x = piano_key_width_;
    const double local_max_x =
        piano_key_width_ + coords_->viewport().width;

    double screen_start_x =
        std::max(screen_bounds.left, local_min_x);
    double screen_end_x =
        std::min(screen_bounds.right, local_max_x);

    if (screen_start_x >= screen_end_x) {
        return;
    }

    const auto to_imvec4 = [](const ColorRGBA& c) {
        return ImVec4(c.r, c.g, c.b, c.a);
    };
    const auto to_color = [&](const ColorRGBA& c) {
        return ImGui::ColorConvertFloat4ToU32(
            to_imvec4(c));
    };

    const float x1 =
        canvas_origin_x +
        static_cast<float>(screen_start_x);
    const float x2 =
        canvas_origin_x +
        static_cast<float>(screen_end_x);
    const float y1 =
        canvas_origin_y +
        static_cast<float>(screen_bounds.top);
    const float y2 =
        canvas_origin_y +
        static_cast<float>(screen_bounds.bottom);

    const bool has_preview =
        show_drag_preview && preview_bounds_ &&
        (interaction_state == InteractionState::Dragging ||
         interaction_state ==
             InteractionState::ResizingLeft ||
         interaction_state ==
             InteractionState::ResizingRight);

    if (has_preview) {
        // Original ghost position.
        if (original_bounds_) {
            auto original_screen =
                world_to_screen_bounds(*original_bounds_);
            if (original_screen) {
                double ghost_start =
                    std::max(original_screen->left,
                             local_min_x);
                double ghost_end =
                    std::min(original_screen->right,
                             local_max_x);
                if (ghost_end > ghost_start) {
                    const float gx1 =
                        canvas_origin_x +
                        static_cast<float>(ghost_start);
                    const float gx2 =
                        canvas_origin_x +
                        static_cast<float>(ghost_end);
                    const float gy1 =
                        canvas_origin_y +
                        static_cast<float>(original_screen
                                               ->top);
                    const float gy2 =
                        canvas_origin_y +
                        static_cast<float>(original_screen
                                               ->bottom);

                    ColorRGBA ghost_col{
                        160.0f / 255.0f,
                        160.0f / 255.0f,
                        160.0f / 255.0f,
                        80.0f / 255.0f};
                    ImU32 col = to_color(ghost_col);
                    draw_list->AddRectFilled(
                        ImVec2(gx1, gy1),
                        ImVec2(gx2, gy2),
                        col);
                }
            }
        }

        // Preview position (bright ghost).
        auto preview_screen =
            world_to_screen_bounds(*preview_bounds_);
        if (preview_screen) {
            double preview_start =
                std::max(preview_screen->left,
                         local_min_x);
            double preview_end =
                std::min(preview_screen->right,
                         local_max_x);
            if (preview_end > preview_start) {
                const float px1 =
                    canvas_origin_x +
                    static_cast<float>(preview_start);
                const float px2 =
                    canvas_origin_x +
                    static_cast<float>(preview_end);
                const float py1 =
                    canvas_origin_y +
                    static_cast<float>(preview_screen
                                           ->top);
                const float py2 =
                    canvas_origin_y +
                    static_cast<float>(preview_screen
                                           ->bottom);

                ColorRGBA fill{
                    1.0f,
                    1.0f,
                    1.0f,
                    50.0f / 255.0f};
                ColorRGBA border{
                    1.0f,
                    1.0f,
                    1.0f,
                    100.0f / 255.0f};
                draw_list->AddRectFilled(
                    ImVec2(px1, py1),
                    ImVec2(px2, py2),
                    to_color(fill));
                draw_list->AddRect(
                    ImVec2(px1, py1),
                    ImVec2(px2, py2),
                    to_color(border));
            }
        }
    } else {
        // Normal loop region bar.
        ColorRGBA base_color =
            config.loop_region_fill_color;
        if (interaction_state ==
            InteractionState::HoveringBody) {
            base_color =
                config.loop_region_hover_fill_color;
        }
        draw_list->AddRectFilled(ImVec2(x1, y1),
                                 ImVec2(x2, y2),
                                 to_color(base_color));
    }

    // Resize handles (hover only).
    if (show_resize_handles) {
        const float handle_width_px = 60.0f;

        if (interaction_state ==
            InteractionState::HoveringLeftEdge) {
            const float max_width =
                std::min(handle_width_px, (x2 - x1) *
                                              0.5f);
            if (max_width > 0.0f) {
                ColorRGBA col =
                    config.loop_region_handle_hover_color;
                draw_list->AddRectFilled(
                    ImVec2(x1, y1),
                    ImVec2(x1 + max_width, y2),
                    to_color(col));
            }
        } else if (interaction_state ==
                   InteractionState::HoveringRightEdge) {
            const float max_width =
                std::min(handle_width_px, (x2 - x1) *
                                              0.5f);
            if (max_width > 0.0f) {
                ColorRGBA col =
                    config.loop_region_handle_hover_color;
                draw_list->AddRectFilled(
                    ImVec2(x2 - max_width, y1),
                    ImVec2(x2, y2),
                    to_color(col));
            }
        }
    }

    // Border when hovering the body.
    if (interaction_state ==
        InteractionState::HoveringBody) {
        ColorRGBA border{
            1.0f,
            1.0f,
            1.0f,
            150.0f / 255.0f};
        draw_list->AddRect(ImVec2(x1, y1),
                           ImVec2(x2, y2),
                           to_color(border));
    }
}
#endif

}  // namespace piano_roll

