#pragma once

#include "piano_roll/coordinate_system.hpp"
#include "piano_roll/note_manager.hpp"
#include "piano_roll/renderer.hpp"

namespace piano_roll {

// Simple demo helper that renders a basic piano roll view with its own
// internal NoteManager and CoordinateSystem. When built with
// PIANO_ROLL_USE_IMGUI, this draws into the current Dear ImGui window.
// Otherwise it is a no-op.
void RenderPianoRollDemo();

// Variant that renders using a caller-provided NoteManager and
// CoordinateSystem. This is the preferred entry point for DAW integration,
// where the host owns note storage and viewport state.
//
// If renderer_override is nullptr, an internal static PianoRollRenderer
// instance is used.
void RenderPianoRollDemo(NoteManager& note_manager,
                         CoordinateSystem& coords,
                         PianoRollRenderer* renderer_override = nullptr);

}  // namespace piano_roll

