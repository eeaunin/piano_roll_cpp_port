#pragma once

// Convenience umbrella header for the piano roll C++ port.
// Pulls in the main public types and the high-level widget.

#include "piano_roll/types.hpp"
#include "piano_roll/note.hpp"
#include "piano_roll/note_manager.hpp"
#include "piano_roll/config.hpp"
#include "piano_roll/coordinate_system.hpp"
#include "piano_roll/grid_snap.hpp"
#include "piano_roll/render_config.hpp"
#include "piano_roll/renderer.hpp"
#include "piano_roll/interaction.hpp"
#include "piano_roll/keyboard.hpp"
#include "piano_roll/overlay.hpp"
#include "piano_roll/playback.hpp"
#include "piano_roll/cc_lane.hpp"
#include "piano_roll/cc_lane_renderer.hpp"
#include "piano_roll/loop_marker_rectangle.hpp"
#include "piano_roll/demo.hpp"
#include "piano_roll/serialization.hpp"
#include "piano_roll/widget.hpp"
