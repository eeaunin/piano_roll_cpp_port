#pragma once

#include "piano_roll/cc_lane.hpp"
#include "piano_roll/coordinate_system.hpp"
#include "piano_roll/render_config.hpp"

namespace piano_roll {

// Render a MIDI CC lane under the notes grid, using the rectangle of the
// last ImGui item as the overall piano roll area. When built without
// PIANO_ROLL_USE_IMGUI this function does nothing.
void RenderControlLane(const ControlLane& lane,
                       const CoordinateSystem& coords,
                       const PianoRollRenderConfig& config);

}  // namespace piano_roll

