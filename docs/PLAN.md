# piano_roll_cpp_port – Design and Implementation Plan

This document tracks the C++20 Dear ImGui piano roll port of `../dpg_piano_roll`.
It focuses on clear data/coord/render/interaction separation and on documenting
which subset of behaviour is implemented at each stage.

## Goals and Scope

- Provide a small, self‑contained C++20 library that exposes a piano roll widget
  suitable for integration into the DAW project.
- Reproduce the UX of `UnifiedPianoRoll` from `../dpg_piano_roll` as closely
  as possible, aiming for behaviour parity:
  - Note display and editing (create/move/resize/delete).
  - Piano keys, grid, and simple playhead visualization.
  - Bitwig‑style zoom/scroll (ruler + note‑name gestures and custom scrollbar)
    and snapping.
- Keep dependencies minimal: Dear ImGui + C++20 standard library only.
- Align conceptually with the DAW ADRs, but avoid hard coupling to DAW code.

Out of scope for the very first C++ drop (but planned once the core port is stable):

- Advanced playback integration and transport‑driven playhead logic.
- Loop markers, cue markers, and multi‑clip management.
- Interval‑tree / spatial index optimizations beyond the Python behaviour.

## Architecture Overview

The port mirrors the Python modules with a small set of C++ components:

- **Data layer**
  - `Note`: value type for a single note (tick, duration, key, velocity, channel, selection flag, id).
  - `NoteManager`: owns notes, selection state, simple spatial index, and undo/redo hooks.

- **Coordinate / viewport layer**
  - `Viewport`: visible region in world space (x/y/width/height).
  - `CoordinateSystem`: tick↔world, key↔world, and world↔screen transforms, plus zoom/scroll.
  - A later refinement may introduce a `VirtualViewport`‑style type for more detailed layout
    (piano key width, ruler height, footer, etc.).

- **Grid and snapping**
  - `GridSnapSystem`: adaptive grid division and snapping similar to
    `piano_roll/grid_snap_system.py`, but initially limited to:
    - A fixed set of musical divisions.
    - Snap on tick create/move/resize and a simple magnetic snap.

- **Selection**
  - `SelectionRectangle`: screen‑space drag rectangle.
  - `SelectionSystem`: set of selected notes (by id), rectangle application, and index updates.

- **Rendering**
  - `RenderConfig`: colours and sizes for the widget (later M4).
  - `PianoRollRenderer`: issues Dear ImGui draw calls for:
    - Background (piano keys + grid).
    - Notes.
    - Optional playhead/clip bounds.

- **Interaction / input**
  - `InputState`: mouse/keyboard modifiers, drag state.
  - `InputHandler`: translates ImGui events into high‑level actions (click/drag, wheel, key shortcuts).
  - Simple “pointer tool” behaviour for note editing; more tools can be added later.

The C++ API should be usable both as:

- A self‑contained demo (single window with the piano roll), and
- An embeddable widget that can be driven by a DAW host.

## Milestones (refining root PLAN.md)

These correspond to, and refine, the milestones in the top‑level `PLAN.md`.

### M1: Core data + coordinates

**Objective:** Have a compilable, non‑rendering library layer that can represent
notes and map between ticks/keys and pixels.

- Data
  - [x] `Note` struct mirroring Python’s `note_manager.Note`.
  - [x] `NoteManager` with:
    - [x] Add/remove/move/resize operations with basic overlap checks.
    - [x] Simple per‑key index for queries.
    - [x] Selection state stored on notes plus an ID set.
    - [x] Snapshot‑based undo/redo stacks.
- Coordinates
  - [x] `Viewport` struct.
  - [x] `CoordinateSystem` class with:
    - [x] `screen_to_world` / `world_to_screen`.
    - [x] `tick_to_world` / `world_to_tick`.
    - [x] `key_to_world_y` / `world_y_to_key`.
    - [x] Zoom in/out and scroll management.

**Deviations from Python implementation (M1):**

- IDs are simple integral types (`NoteId`) instead of UUID strings.
- Interval‑tree optimizations are not implemented; we use a straightforward
  per‑key index similar to `NoteManager`’s dictionary.
- Overlap checks and range queries are implemented per key but use linear scans
  within each key’s note list.
- No DearPyGUI or Dear ImGui calls appear in this layer; it is purely
  logic/model code.

### M2: Basic rendering

**Objective:** Render a static piano roll view in Dear ImGui using the M1
structures; no advanced interaction yet.

- [x] Define a `PianoRollRenderConfig` with colours and basic geometry
      (`include/piano_roll/render_config.hpp`).
- [x] Implement `PianoRollRenderer` (`include/piano_roll/renderer.hpp`,
      `src/renderer.cpp`):
  - [x] Background rectangle and piano‑key strip.
  - [x] Grid lines based on `CoordinateSystem` + `GridSnapSystem`.
  - [x] Note rectangles for a given set of notes.
- [x] Provide a small demo helper `RenderPianoRollDemo()`:
  - [x] Uses fixed test notes (simple C major pattern).
  - [x] Uses a viewport derived from the current ImGui content region and a
        basic zoom slider (`src/demo.cpp`).

**Deviations (M2):**

- Only one layer (no multi‑layer/double buffering like the Python render
  system). Layering can be added later if needed.
- At the M2 stage there were no custom scrollbars; a Bitwig‑style horizontal
  `CustomScrollbar` and explored‑area handling were added later as part of the
  interaction work (see M3 and "Current Status").
- The renderer is compiled in two modes:
  - Without `PIANO_ROLL_USE_IMGUI`: `PianoRollRenderer::render` is a no‑op
    so the core library can build without Dear ImGui.
  - With `PIANO_ROLL_USE_IMGUI` defined and ImGui available: `render` draws
    into the current ImGui window using `ImDrawList`.

### M3: Core interactions

**Objective:** Mouse‑based note editing with a minimal but usable UX.

- [x] Mapping Dear ImGui mouse events to `PointerTool` for:
  - [x] Click‑to‑select and rectangle selection (via `PointerTool` in
        `include/piano_roll/interaction.hpp`).
  - [x] Drag‑to‑move notes (with snap using `GridSnapSystem` when provided).
  - [x] Drag‑from‑edge to resize notes (with snap).
  - [x] Double‑click to create/delete notes when the host calls
        `PointerTool::on_double_click`.
- [x] Basic keyboard shortcuts:
  - [x] Delete selected notes (via `KeyboardController`).
  - [x] Select all (via `KeyboardController`).
  - [x] Copy/paste selected notes into an internal clipboard.
  - [x] Undo/redo using `NoteManager`'s snapshot-based stacks.
- [x] Simple playhead position update (no actual audio).

**Deviations (M3):**

- Bitwig‑style scroll/zoom is only partially ported:
  - Horizontal scrollbar uses explored‑area semantics and edge‑resize zoom, as
    in the Python `CustomScrollbar`, but rendering is still single‑layer.
  - Ruler and note‑name area pan/zoom gestures are mapped into
    `PianoRollWidget::handle_pointer_events`, with vertical zoom currently
    keeping approximate (not exact) key anchoring.
- The originally planned standalone `InputState`/`InputHandler` layer has been
  folded into `PianoRollWidget::handle_pointer_events` and
  `PianoRollWidget::handle_keyboard_events`, which translate Dear ImGui events
  into `PointerTool` and `KeyboardController` calls.
- No multi‑tool system; we currently expose a single pointer/edit tool
  (`PointerTool`) operating on `NoteManager` and `CoordinateSystem`.
- The interaction layer remains GUI‑agnostic: host code maps GUI events
  (e.g. Dear ImGui mouse events) to `PointerTool` calls and uses
  `RenderSelectionOverlay` for the rectangle visualization.
- Undo/redo integration for pointer and keyboard edits now uses
  `NoteManager`'s snapshot‑based stacks:
  - Pointer drags/resizes (including Ctrl+drag duplication) take a single
    snapshot at the start of the gesture and apply changes with
    `record_undo=false`, so each gesture is one undo step.
  - Double‑click create/delete uses `record_undo=true`.
  - Keyboard move/delete/paste operations group their edits into single undo
    steps via `NoteManager::snapshot_for_undo`.

### M4: Configuration and presets

**Objective:** Make the widget configurable and ready to embed.

- [x] `PianoRollConfig` struct for:
  - [x] Dimensions (piano key width, ruler height, footer height, note‑label width).
  - [x] Musical defaults (ticks‑per‑beat, initial clip length, initial centered key).
  - [x] CC lane defaults (visibility and height).
- [x] Presets for:
  - [x] “Compact” layout (`PianoRollConfig::compact`).
  - [x] “Spacious” layout (`PianoRollConfig::spacious`).
- [x] Hooks for DAW integration:
  - [x] Ability to render using an external `NoteManager` and `CoordinateSystem`
        via the overloaded `RenderPianoRollDemo(note_manager, coords, renderer)`
        helper.
  - [x] Optional callback for “playhead changed” events exposed via
        `PianoRollWidget::set_playhead_changed_callback`, fired on internal
        playhead updates (ruler clicks, `update_playback`) and host‑driven
        `set_playhead` calls.
  - [x] Additional callbacks for playback markers
        (`set_playback_markers_changed_callback`) and piano‑key presses/
        releases (`set_piano_key_pressed_callback`,
        `set_piano_key_released_callback`) on `PianoRollWidget`.

### MIDI CC lane (initial support)

Additional standalone feature, not part of the original milestones but useful
for DAW-style workflows:

- [x] `ControlLane` data structure for MIDI CC points (`include/piano_roll/cc_lane.hpp`).
- [x] `RenderControlLane` to draw a CC lane under the notes grid in ImGui
      (`include/piano_roll/cc_lane_renderer.hpp`, `src/cc_lane_renderer.cpp`).
- [x] Basic CC editing in `PianoRollWidget`:
  - [x] Click in the CC lane to create a new point at the clicked tick/value.
  - [x] Drag vertically/horizontally to adjust value and tick.
  - [x] Ctrl+click near a point to delete it.
- [x] Configurable CC lane appearance via `PianoRollRenderConfig`
      (`show_cc_lane`, `cc_lane_height`, colours for background/curve/points).

### CC lanes and serialization refinements

- [x] Support for multiple CC lanes:
  - [x] `PianoRollWidget` now owns a `std::vector<ControlLane>` and an
        `active_cc_lane_index`.
  - [x] Simple CC lane selector UI in ImGui (`None` or a specific CC number),
        with the active lane rendered under the notes grid.
  - [x] CC editing (click/drag/Ctrl‑click) operates on the currently active
        lane only.
- [x] Snap CC points to the same grid as notes using `GridSnapSystem` when
      mapping X positions to ticks.
- [x] Serialization helpers:
  - [x] `serialize_notes_and_cc(const NoteManager&, const std::vector<ControlLane>&, std::ostream&)`.
  - [x] `deserialize_notes_and_cc(NoteManager&, std::vector<ControlLane>&, std::istream&)`.
  - [x] Simple, versioned line‑based text format compatible with future
        extensions.

**Deviations (M4):**

- Detailed playback UI (playback engine, tempo‑driven playhead advancement,
  interactive drag logic for playback start/cue markers) remains host‑driven;
  the C++ port currently exposes visual loop regions, playback start markers,
  and cue markers, but expects the DAW transport to update their tick
  positions.
 - Colours (keys, grid, notes, background) are configured via
   `PianoRollRenderConfig` rather than `PianoRollConfig`, keeping layout and
   theme concerns separate in the C++ port.
 - Clip‑colour theming is supported via `PianoRollRenderConfig::apply_clip_color`
   and `PianoRollWidget::set_clip_color`, which approximate
   `UnifiedPianoRoll._update_colors_from_clip_color` for note and marker
   colours but do not yet adjust the full light‑theme background palette.
 - Ticks‑per‑beat, snapping, and renderer grid are kept in sync via
   `PianoRollWidget::set_ticks_per_beat`, and MIDI clip boundaries can be
   configured through `PianoRollWidget::set_clip_bounds` for ruler brackets
   and scrollbar fit‑to‑clip behaviour.

## Current Status

At the moment:

- [x] Core data and coordinate layers implemented:
  - `Note`, `NoteManager` with selection and snapshot‑based undo/redo.
  - `CoordinateSystem` with tick/key/world transforms and Bitwig‑style
    horizontal scrolling (negative X allowed) plus clamped vertical scroll.
  - `GridSnapSystem` for adaptive grid/snap divisions and ruler labels.
  - [x] Basic Dear ImGui rendering via `PianoRollRenderer` with background,
      grid, notes, ruler, and optional playhead.
- [x] Core interactions:
  - `PointerTool` for click‑select, rectangle selection, drag‑move,
    edge‑resize, and double‑click create/delete.
    - Hover tracking for notes to support edge/body overlays.
    - Dragging a selected note moves the entire selection group, matching the
      Python group‑drag semantics.
    - Rectangle selection respects Ctrl/Shift modes:
      - Ctrl‑drag: add to existing selection.
      - Shift‑drag: toggle notes inside the rectangle.
      - Plain drag: replace selection.
    - Ctrl+drag duplication enabled via `PointerTool` in `PianoRollWidget`
      so that holding Ctrl while dragging a selection creates and drags
      duplicates, with overlay colour indicating duplication.
  - `KeyboardController` for delete, select‑all, copy/paste, undo/redo.
    - Arrow keys to move selected notes:
      - Up/Down: semitones or octaves (with Shift).
      - Left/Right: by current snap division or fine 1/128‑note step (with Shift),
        using the same tick math as the Python snap system.
    - Snap/grid configuration UI:
      - Snap mode combo (Off/Adaptive/Manual) and snap division selector wired to
        `GridSnapSystem` so snapping and grid lines follow the same musical
        divisions as the Python snap menu.
  - Playhead:
    - Renderer supports a simple Bitwig‑style line+handle playhead at a given
      tick, with optional auto‑scroll behaviour (disabled by default but
      configurable via `PianoRollRenderConfig` to follow playback similarly to
      the Python `PlaybackIndicator.should_auto_scroll` logic.
    - Clicking in the ruler (when not starting a pan/zoom gesture or hitting
      playback/loop markers) updates the playhead position, approximating the
      Python `PlaybackIndicator.handle_click` semantics.
  - Vertical mouse‑wheel scrolling wired in `PianoRollWidget`, matching the
    Python `MouseWheelHandler` behaviour (no wheel‑zoom; zoom is via ruler and
    note‑name gestures).
- [x] Bitwig‑style horizontal scrollbar and explored‑area handling ported to
      `CustomScrollbar` and `PianoRollWidget`, including edge‑resize zoom and
      double‑click to fit the MIDI clip.
  - Scrollbar geometry and explored‑area updates follow
    `piano_roll_interaction._handle_scrollbar_edge_resize` and friends.
- [x] Initial MIDI CC lane support with `ControlLane`, `RenderControlLane`,
      and CC point editing in `PianoRollWidget`.
- [x] Serialization helpers for notes and CC lanes.
- [ ] Multi‑layer render system and full debug overlays from
      `render_system.py` are partially ported:
  - [x] Simple spotlight background behind the horizontal span of selected
        notes in `PianoRollRenderer`, with Bitwig‑style vertical edge lines.
  - [x] Edge/body hover highlight for notes in `RenderSelectionOverlay`.
  - [x] Ruler padding and piano‑key area visual feedback during
        ruler/note‑name interactions in `PianoRollWidget`.
  - [x] Double‑border styling for selected notes (outer clip‑coloured border +
        inner light border) in `PianoRollRenderer`.
  - [x] Drag preview overlays for all selected notes during drag/resize, with
        colour‑coded distinction between move and duplicate operations, keyed
        off `PointerTool` drag/duplicate state.
  - [x] Key‑row “zebra” backgrounds in the grid area based on visible keys
        (black vs white) using `CoordinateSystem::key_to_world_y`.
  - [x] Debug overlays: vertical cursor line and clicked‑cell highlight computed
        from tick/key coordinates (for verifying coordinate parity).
  - [x] Edge scrolling for rectangle selection near widget edges, matching
        the Python `_check_rectangle_edge_scrolling` behaviour.
  - [x] Piano key strip: per‑key rectangles and hover/press highlights driven by
        `PianoRollWidget` state (hovered/pressed key), matching the Python key
        interaction feedback.
  - [x] MIDI clip boundary brackets drawn in the ruler using `clip_start_tick`
        and `clip_end_tick`, matching the Bitwig‑style clip range indicators.
  - [x] Note labels (e.g. `C4`) rendered on notes when zoomed in sufficiently,
        using MIDI key → name+octave mapping and minimum height/width thresholds.
  - [x] Piano-key labels (C, C#4, etc.) rendered in the left label column with
        zoom-dependent density (all notes vs. only C/F vs. only C), and C-line
        separators, matching the Python `_render_piano_keys_to` behaviour.
  - [x] Simple note shadows for non‑selected notes to approximate Bitwig-style
        depth cues.
  - [x] Grid line styling distinguishes measures, beats, and subdivisions with
        separate colours/thicknesses, mirroring the Python grid density logic.
- [x] Bitwig‑style loop region in the ruler using `LoopMarkerRectangle` on
        top of `DraggableRectangle` (horizontal drag/resize with snap to 1/4
        beat, hover handles, and ghost preview during interaction).
  - [x] Playback start and cue markers drawn in the ruler (top and bottom
        sections) using host‑provided tick positions, approximating the Python
        `PlaybackMarker` and cue marker visuals without embedding a full
        playback engine.
  - [x] Lightweight playback helpers:
        - [x] `advance_playback_ticks` free function for tempo‑driven tick
              accumulation with optional loop wrapping (mirroring the core
              logic in `update_playback` from the Python implementation).
        - [x] `PlaybackState` struct for hosts that prefer a small stateful
              helper with `advance(delta_seconds)` semantics.
        - [x] `PianoRollWidget::update_playback(current_tick, tempo_bpm, dt)`
              convenience method that advances a playback tick using the
              widget’s current `ticks_per_beat` and loop region and updates
              the internal playhead for auto‑scroll.
  - [x] View helpers:
        - [x] `PianoRollWidget::fit_view_to_clip()` mirroring the scrollbar
              double‑click “fit to clip” behaviour.
        - [x] `PianoRollWidget::fit_view_to_selection()` to zoom and scroll
              so the current note selection is framed with a small padding in
              both time and pitch, approximating the Python selection‑fit
              logic.
        - [x] Drag/duplicate state exposure on `PianoRollWidget` for host UI
              hints (`is_dragging_note`, `is_resizing_note`,
              `is_duplicating_notes`).
  - [x] Theme helpers:
        - [x] `PianoRollRenderConfig::apply_light_theme_base()` to approximate
              the light grey background/key/grid palette used by
              `UnifiedPianoRoll`.
        - [x] `PianoRollRenderConfig::apply_light_theme_from_clip_color` and
              `PianoRollWidget::apply_light_theme_from_clip_color` to combine
              the light theme palette with clip‑colour‑driven note/marker
              colours, mirroring `_update_colors_from_clip_color` while keeping
              theme choice host‑controlled.
  - [x] Demo helpers:
        - [x] `RenderPianoRollDemo` now includes simple transport controls
              (tempo slider + play/pause/stop) and a clip‑colour picker that
              calls `PianoRollRenderConfig::apply_clip_color`, making it easy
              to exercise playback and clip‑color theming without a full host.
  - [ ] Full per‑layer draw‑list separation, z‑indexed note ordering, rich
        group drag/resize previews, and cell debug overlays remain to be
        transliterated.

When new features are implemented, this file should be updated with:

- Which parts of each milestone are complete.
- Any intentional behaviour differences vs. the Python implementation.
