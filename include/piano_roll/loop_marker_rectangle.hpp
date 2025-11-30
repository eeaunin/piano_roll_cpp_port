#pragma once

#include "piano_roll/draggable_rectangle.hpp"
#include "piano_roll/coordinate_system.hpp"
#include "piano_roll/render_config.hpp"
#include "piano_roll/types.hpp"

namespace piano_roll {

// Loop marker that can be dragged and resized on the ruler, modelled after
// the Python LoopMarkerRectangle and built on top of DraggableRectangle.
//
// Semantics:
// - Horizontal coordinates (left/right) are stored in world X (same space as
//   CoordinateSystem::tick_to_world).
// - Vertical coordinates (top/bottom) are stored in widget-local screen Y,
//   since the ruler does not scroll vertically.
// - Interaction uses widget-local screen coordinates (0,0 = top-left of the
//   piano-roll canvas, before adding the ImGui window origin).
class LoopMarkerRectangle : public DraggableRectangle {
public:
    LoopMarkerRectangle(CoordinateSystem* coords = nullptr,
                        Tick start_tick = 4 * 480,
                        Tick end_tick = 8 * 480);

    void set_coordinate_system(CoordinateSystem* coords) noexcept;

    // Layout parameters for the ruler band the loop region lives in.
    void set_layout(float top_padding,
                    float ruler_height,
                    double piano_key_width) noexcept;

    void set_tick_range(Tick start, Tick end) noexcept;
    std::pair<Tick, Tick> tick_range() const noexcept {
        return {start_tick_, end_tick_};
    }

    Tick start_tick() const noexcept { return start_tick_; }
    Tick end_tick() const noexcept { return end_tick_; }

    // Sync rectangle bounds from the current tick range and layout.
    void update_bounds_from_ticks() noexcept;

    // Sync tick range from the current rectangle bounds.
    void update_ticks_from_bounds() noexcept;

#ifdef PIANO_ROLL_USE_IMGUI
    // Render the loop region into the given ImGui draw list. The canvas
    // origin is the top-left corner of the piano-roll widget item.
    void render(void* draw_list_void,
                const PianoRollRenderConfig& config,
                float canvas_origin_x,
                float canvas_origin_y) const;
#endif

protected:
    std::optional<std::pair<double, double>> screen_to_world(
        double x, double y) const override;

    std::optional<std::pair<double, double>> world_to_screen(
        double x, double y) const override;

    std::optional<RectangleBounds> get_screen_bounds() const override;

    std::optional<RectangleBounds> world_to_screen_bounds(
        const RectangleBounds& b) const override;

    void on_bounds_finalized() override;

private:
    CoordinateSystem* coords_{nullptr};
    Tick start_tick_{0};
    Tick end_tick_{0};

    float top_padding_{0.0f};
    float ruler_height_{24.0f};
    double piano_key_width_{180.0};

    void update_snap_parameters() noexcept;
};

}  // namespace piano_roll

