#include "piano_roll/cc_lane_renderer.hpp"

#ifdef PIANO_ROLL_USE_IMGUI
#include <imgui.h>
#endif

namespace piano_roll {

void RenderControlLane(const ControlLane& lane,
                       const CoordinateSystem& coords,
                       const PianoRollRenderConfig& config) {
#ifdef PIANO_ROLL_USE_IMGUI
    if (!config.show_cc_lane) {
        return;
    }

    ImDrawList* draw_list = ImGui::GetWindowDrawList();
    if (!draw_list) {
        return;
    }

    // Use the rect of the last item (the piano roll) as the overall area.
    ImVec2 canvas_min = ImGui::GetItemRectMin();
    ImVec2 canvas_max = ImGui::GetItemRectMax();

    float total_height = canvas_max.y - canvas_min.y;
    float lane_height = config.cc_lane_height;
    if (lane_height <= 0.0f || lane_height > total_height * 0.8f) {
        lane_height = total_height * 0.25f;
    }

    float lane_bottom = canvas_max.y;
    float lane_top = lane_bottom - lane_height;

    float left = canvas_min.x +
                 static_cast<float>(coords.piano_key_width());
    float right = canvas_max.x;

    auto to_imvec4 = [](const ColorRGBA& c) {
        return ImVec4(c.r, c.g, c.b, c.a);
    };
    auto to_color = [&](const ColorRGBA& c) {
        return ImGui::ColorConvertFloat4ToU32(to_imvec4(c));
    };

    // Background.
    draw_list->AddRectFilled(ImVec2(left, lane_top),
                             ImVec2(right, lane_bottom),
                             to_color(config.cc_lane_background_color));
    draw_list->AddRect(ImVec2(left, lane_top),
                       ImVec2(right, lane_bottom),
                       to_color(config.cc_lane_border_color),
                       0.0f,
                       0,
                       1.0f);

    // Draw the curve connecting points.
    const auto& points = lane.points();
    if (points.size() >= 2) {
        ImVec2 prev{};
        bool has_prev = false;

        for (const ControlPoint& p : points) {
            double world_x = coords.tick_to_world(p.tick);
            auto [screen_x_local, _] =
                coords.world_to_screen(world_x, 0.0);
            float x = canvas_min.x +
                      static_cast<float>(screen_x_local);

            float t = 0.0f;
            if (p.value <= 0) {
                t = 1.0f;
            } else if (p.value >= 127) {
                t = 0.0f;
            } else {
                t = 1.0f - static_cast<float>(p.value) / 127.0f;
            }
            float y = lane_top +
                      t * (lane_bottom - lane_top);

            ImVec2 current(x, y);
            if (has_prev) {
                draw_list->AddLine(prev,
                                   current,
                                   to_color(config.cc_curve_color),
                                   2.0f);
            }
            prev = current;
            has_prev = true;
        }
    }

    // Draw individual points as small circles.
    for (const ControlPoint& p : lane.points()) {
        double world_x = coords.tick_to_world(p.tick);
        auto [screen_x_local, _] =
            coords.world_to_screen(world_x, 0.0);
        float x = canvas_min.x +
                  static_cast<float>(screen_x_local);

        float t = 0.0f;
        if (p.value <= 0) {
            t = 1.0f;
        } else if (p.value >= 127) {
            t = 0.0f;
        } else {
            t = 1.0f - static_cast<float>(p.value) / 127.0f;
        }
        float y = lane_top +
                  t * (lane_bottom - lane_top);

        float radius = 4.0f;
        draw_list->AddCircleFilled(ImVec2(x, y),
                                   radius,
                                   to_color(config.cc_point_color));
    }
#else
    (void)lane;
    (void)coords;
    (void)config;
#endif
}

}  // namespace piano_roll

