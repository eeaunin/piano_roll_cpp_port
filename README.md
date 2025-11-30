# piano_roll_cpp_port

Experimental C++20 + Dear ImGui piano roll, inspired by the
`UnifiedPianoRoll` implementation in `../dpg_piano_roll`.

The goal is to provide a small, self‑contained widget that:

- Uses clean C++20 data structures for notes, coordinates, and snapping.
- Can render a piano roll in Dear ImGui when `PIANO_ROLL_USE_IMGUI` is defined.
- Exposes a GUI‑agnostic interaction API (pointer + keyboard) that a host
  application can wire to any UI framework.

For a detailed roadmap and status, see `docs/PLAN.md`.

## Core components

The main headers are:

- `include/piano_roll/types.hpp` – shared aliases (`Tick`, `Duration`, `MidiKey`, `NoteId`).
- `include/piano_roll/note.hpp` – `Note` value type (tick, duration, key, velocity, channel, selection).
- `include/piano_roll/note_manager.hpp` – `NoteManager` managing a collection of notes, selection, and snapshot‑based undo/redo.
- `include/piano_roll/coordinate_system.hpp` – `CoordinateSystem` and `Viewport` for tick↔world and key↔world transforms, zoom, and scroll.
- `include/piano_roll/grid_snap.hpp` – `GridSnapSystem` for adaptive grid and tick snapping, plus ruler label helpers.
- `include/piano_roll/render_config.hpp` – `PianoRollRenderConfig` colours and geometry (ImGui‑free).
- `include/piano_roll/renderer.hpp` – `PianoRollRenderer` that draws the piano roll into the current ImGui window when `PIANO_ROLL_USE_IMGUI` is defined.
- `include/piano_roll/interaction.hpp` – `PointerTool` for mouse‑based note editing (select, drag, resize, rectangle select, double‑click create/delete).
- `include/piano_roll/keyboard.hpp` – `KeyboardController` for basic shortcuts (select all, delete, copy/paste, undo/redo).
- `include/piano_roll/overlay.hpp` – `RenderSelectionOverlay` to draw a selection rectangle overlay in ImGui.
- `include/piano_roll/cc_lane.hpp` – `ControlLane` data for a single MIDI CC lane.
- `include/piano_roll/cc_lane_renderer.hpp` – `RenderControlLane` to draw a CC lane under the notes grid in ImGui.
- `include/piano_roll/demo.hpp` – `RenderPianoRollDemo` helpers for quick demos.
- `include/piano_roll/widget.hpp` – `PianoRollWidget`, a self‑contained ImGui widget that ties everything together.
- `include/piano_roll/serialization.hpp` – helpers to serialize/deserialize notes
  and CC lanes to/from a simple text format.

All ImGui usage is gated on `PIANO_ROLL_USE_IMGUI`. The core logic (notes,
coordinates, snapping, interactions) can be built without ImGui present.

## Building the core library

To compile the core logic without ImGui (e.g. for tests or non‑GUI tools):

```bash
cmake -S . -B build
cmake --build build --config Release -j4
```

This builds a static library `libpiano_roll.a` that does not require ImGui at
link time as long as `PIANO_ROLL_USE_IMGUI` is **not** defined.

## Using the ImGui renderer

To enable rendering, build with `PIANO_ROLL_USE_IMGUI` defined and include
ImGui headers before using the widget:

```cpp
#define PIANO_ROLL_USE_IMGUI
#include <imgui.h>
#include "piano_roll/widget.hpp"
```

You must create and manage the Dear ImGui context and backend (GLFW/SDL, etc.)
in your application. This project does not include backend setup code.

When using CMake, enable the option:

```bash
cmake -S . -B build -DPIANO_ROLL_USE_IMGUI=ON
cmake --build build --config Release -j4
```

Then, in your parent project, add the appropriate ImGui include directories and
link libraries and link against the `piano_roll` target.

## Quick usage with `PianoRollWidget`

The easiest way to embed a piano roll in an existing ImGui window is to use
`PianoRollWidget`:

```cpp
#define PIANO_ROLL_USE_IMGUI
#include <imgui.h>
#include "piano_roll/widget.hpp"

piano_roll::PianoRollWidget g_piano_roll;

void ShowPianoRollWindow() {
    if (ImGui::Begin("Piano Roll")) {
        g_piano_roll.draw();
    }
    ImGui::End();
}
```

`PianoRollWidget`:

- Adjusts the viewport to fit the available content region.
- Provides a simple zoom slider.
- Renders background, grid, notes, ruler, and optional playhead.
- Handles pointer interactions (click/drag/resize/rectangle select/double‑click).
- Handles keyboard shortcuts (select all, delete, copy/paste, undo/redo).
- Draws the selection rectangle overlay.
- Optionally renders a single MIDI CC lane under the notes grid and supports
  simple CC editing (click to add, drag to adjust, Ctrl+click to delete).

You can customise the widget by accessing its internals:

```cpp
auto& notes = g_piano_roll.notes();
auto& coords = g_piano_roll.coords();
auto& snap = g_piano_roll.snap();
auto& cfg = g_piano_roll.config();

// Example: change note colour
cfg.note_fill_color = {0.3f, 0.8f, 0.4f, 1.0f};

// Example: set a playhead at tick 0
g_piano_roll.set_playhead(0);
```

If you want more control over layout and musical defaults, construct the
widget with a `PianoRollConfig`:

```cpp
#include "piano_roll/config.hpp"

piano_roll::PianoRollConfig cfg = piano_roll::PianoRollConfig::compact();
cfg.ticks_per_beat = 480;
cfg.beats_per_measure = 4;
cfg.initial_center_key = 60;  // C4

piano_roll::PianoRollWidget g_piano_roll(cfg);
```

You can also toggle debug visuals and snapping helpers via the config:

```cpp
auto& rc = g_piano_roll.config();
rc.show_magnetic_zones = true;          // show magnetic snap zones
rc.playhead_auto_scroll = true;         // auto-follow playhead
```

Keyboard shortcuts (when internal keyboard handling is enabled) include:

- `Ctrl+A` – select all notes.
- `Delete` / `Backspace` – delete selected notes.
- `Ctrl+C` – copy selected notes to the internal clipboard.
- `Ctrl+V` – paste at original tick positions.
- `Ctrl+Shift+V` – paste so the earliest copied note aligns with the current
  playhead tick (if a playhead is set).
- `Ctrl+Alt+V` – paste so the earliest copied note aligns with the start tick
  of the last clicked grid cell (if one is recorded).

If you prefer to handle input in your own system (or share keyboard focus with
other widgets), you can ask `PianoRollWidget` to render only and wire
mouse/keyboard events yourself:

```cpp
// Disable built-in ImGui-based handlers:
g_piano_roll.set_internal_pointer_enabled(false);
g_piano_roll.set_internal_keyboard_enabled(false);

// In your event layer, call PointerTool / KeyboardController directly using
// g_piano_roll.notes(), g_piano_roll.coords(), etc., then call draw() each
// frame to render the current state.
```

## Demo helpers

If you prefer a more manual integration, you can use the lower‑level demo
helpers instead of `PianoRollWidget`:

- `RenderPianoRollDemo()` – uses internal static state and draws a simple C‑major pattern.
- `RenderPianoRollDemo(NoteManager&, CoordinateSystem&, PianoRollRenderer*)` –
  renders using caller‑provided note storage and coordinate system.

With these you are responsible for:

- Adjusting viewport size to the ImGui content region.
- Mapping mouse and keyboard events into `PointerTool` and `KeyboardController`.
- Calling `RenderSelectionOverlay` after the piano roll item is rendered.

## Future work

- More advanced zoom and scroll behaviours (e.g. additional Bitwig‑style
  gestures beyond the current ruler/note‑name pan/zoom and custom scrollbar).
- Improved clipboard semantics (e.g. paste at playhead or mouse position).
- Higher‑level integration hooks for external DAWs (transport/tempo objects,
  clip management) while keeping this library GUI‑ and host‑agnostic.
