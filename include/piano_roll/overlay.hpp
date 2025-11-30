#pragma once

#include "piano_roll/coordinate_system.hpp"
#include "piano_roll/grid_snap.hpp"
#include "piano_roll/interaction.hpp"
#include "piano_roll/note_manager.hpp"
#include "piano_roll/render_config.hpp"

namespace piano_roll {

// Helper for drawing a selection rectangle overlay on top of the last
// rendered piano roll item when using Dear ImGui.
//
// When built without PIANO_ROLL_USE_IMGUI this function does nothing.
void RenderSelectionOverlay(const NoteManager& notes,
                            const PointerTool& tool,
                            const CoordinateSystem& coords,
                            const PianoRollRenderConfig& config,
                            const GridSnapSystem* snap_system = nullptr);

}  // namespace piano_roll
