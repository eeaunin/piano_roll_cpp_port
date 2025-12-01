# Piano Roll ↔ daw_widgets Integration – Session Handover (2025‑12‑01T00:00:00Z)

This handover summarizes the current state of integrating the C++ piano roll library (`piano_roll_cpp_port`) into the `daw_widgets` UI app, with a focus on the piano‑roll panel, scrollbars, CC lane, and panel alignment issues.

## 1. High‑level state

- `piano_roll_cpp_port` is built as a static library from within `daw_widgets`:
  - Wired in `external/CMakeLists.txt` via `add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/../../piano_roll_cpp_port ...)`.
  - `PIANO_ROLL_USE_IMGUI` is enabled and `piano_roll` links against the shared `imgui` target.
  - `src/CMakeLists.txt` links `daw_widgets` against `piano_roll`.
- The DAW’s main UI app (`ui_app`) now embeds the C++ `piano_roll::PianoRollWidget` in the piano‑roll panel instead of the old placeholder knob grid.
- All ImGui integration for piano roll is delegated to the DAW app; the piano roll library remains “widget‑level” only (no window management / docking).

## 2. Piano‑roll panel wiring in daw_widgets

File: `src/ui/app/editor_piano_roll.cpp`

- The piano‑roll panel renderer is registered via:

  ```cpp
  panels.SetRenderer("piano_roll", [&](const PanelRenderContext& ctx) -> ImVec2 { ... });
  ```

- Inside this renderer:
  - A header strip is drawn at the top of the panel card (`"Piano Roll"` label).
  - Debug lines are currently drawn:
    - **Panel‑level cyan line** at `ctx.rect.Min.x` (panel content left edge).
    - **Panel‑level orange line** at `ctx.rect.Min.x + 180.0f` (approximate grid start from the panel’s POV).
  - Piano roll content rect:

    ```cpp
    ImVec2 origin = ctx.ContentOrigin();
    ImVec2 roll_pos(ctx.rect.Min.x,           // left edge of panel card
                    origin.y + header_h);     // below header, follows vertical scroll
    ImVec2 roll_size(ctx.rect.GetWidth(),
                     ctx.rect.GetHeight() - header_h);
    ```

  - The widget is called as:

    ```cpp
    auto& widget = GetPianoRollWidget();
    widget.set_canvas_rect(roll_pos.x, roll_pos.y,
                           roll_size.x, roll_size.y);
    widget.draw();
    ```

  - The renderer returns `ImVec2(ctx.rect.GetWidth(), ctx.rect.GetHeight() * 2.0f)` to allow PanelManager to add a vertical scrollbar when needed.

**Current behaviour:**

- Input: clicking in Inspector / Browser no longer activates piano‑roll interactions.
- Layout: when the piano‑roll panel is moved, the piano‑roll widget’s internal debug lines move with the panel and stay inside the card.
- The piano‑key strip is *logically* aligned to the panel content rect, but due to internal ImGui/canvas usage the keys can still appear visually “under” Inspector when Inspector is visible.

## 3. Canvas rect and widget internals

File: `piano_roll_cpp_port/include/piano_roll/widget.hpp`  
File: `piano_roll_cpp_port/src/widget.cpp`

- `PianoRollWidget` now has an explicit canvas rect API:

  ```cpp
  void set_canvas_rect(float x, float y,
                       float width, float height) noexcept;
  ```

  Internal fields:

  ```cpp
  double canvas_origin_x_{0.0};
  double canvas_origin_y_{0.0};
  double canvas_width_{0.0};
  double canvas_height_{0.0};
  bool   canvas_rect_valid_{false};
  ```

- `set_canvas_rect` simply stores these values and marks the rect as valid.

- `draw_in_rect(x, y, w, h)` is now just:

  ```cpp
  set_canvas_rect(x, y, width, height);
  draw();
  canvas_rect_valid_ = false;
  ```

- In `PianoRollWidget::draw()`:
  - If a canvas rect is valid, the cursor is moved **once** at the start:

    ```cpp
    if (canvas_rect_valid_) {
        ImGui::SetCursorScreenPos(
            ImVec2(static_cast<float>(canvas_origin_x_),
                   static_cast<float>(canvas_origin_y_)));
    }
    ```

  - Width/height are derived from the explicit rect when provided:

    ```cpp
    ImVec2 avail = ImGui::GetContentRegionAvail();
    double width =
        (forced_width_ > 0.0) ? forced_width_
        : (canvas_rect_valid_ ? canvas_width_
                              : static_cast<double>(avail.x));
    double height =
        (forced_height_ > 0.0) ? forced_height_
        : (canvas_rect_valid_ ? canvas_height_
                              : static_cast<double>(avail.y));
    ```

  - Viewport width is `width - coords_.piano_key_width()`.
  - Viewport height is `height - (scrollbar track) - (CC lane height, if visible)`.

- Debug inside `draw()` (currently still enabled):
  - **Widget‑level origin line** (cyan-ish) at `canvas_min.x` (where `renderer_.render`’s `InvisibleButton` starts).
  - **Widget‑level grid start line** (red) at `canvas_min.x + coords_.piano_key_width()`.

**Important:**  
The widget still uses `ImGui::InvisibleButton` inside `PianoRollRenderer::render()` and `GetItemRectMin/Max()` for the canvas origin. We moved the ImGui cursor before calling `renderer_.render`, so the internal item rect now matches the host‑supplied rect. That’s why the widget‑level debug lines are inside the panel card and move correctly with it.

## 4. Scrollbar & CC lane behaviour (C++ vs Python)

File: `piano_roll_cpp_port/include/piano_roll/custom_scrollbar.hpp`  
File: `piano_roll_cpp_port/src/custom_scrollbar.cpp`

- C++ `CustomScrollbar` is now structurally aligned with the Python version:
  - Inherits from `DraggableRectangle`.
  - Track and thumb are in screen coordinates.
  - Horizontal scrollbar supports:
    - Edge resize (left/right) → Bitwig‑style zoom.
    - Body drag (centre) → scroll only.
    - Page up/down on clicks in the track outside the thumb.
    - Double‑click on thumb (forwarded via `on_double_click` callback).

- We introduced an internal `BodyDragState` for the thumb centre in C++ to cleanly separate body drag from edge resize:

  ```cpp
  struct BodyDragState { bool active; double offset_x; };
  ```

  - On thumb body click: set `BodyDragState.active = true` and record offset.
  - On mouse move: when active, compute new `bounds.left` by clamping `(mouse_x - offset_x)` within `track_pos_` and `track_pos_ + track_size_px_.first - thumb_width`, then call `handle_bounds_changed_internal(bounds)` to update `scroll_position_` and `on_scroll_update`.
  - On mouse up: end body drag and call `on_drag_end`.

- Edge resize retains the Python semantics:
  - `interaction_state = ResizingLeft/ResizingRight`.
  - Manual thumb geometry is stored in `manual_thumb_pos_` / `manual_thumb_size_`.
  - `on_edge_resize(edge, delta_x)` is called to drive zoom/scaling.

- Visual feedback:
  - Track: dark background (`(0.14, 0.14, 0.14)`).
  - Thumb: base grey, light grey on hover, darker grey while dragging.
  - Edge highlights: yellow bands when edges are hovered or being resized.

**State:**

- Scrollbar behaviour is now stable and non‑erratic; centre drags pan, edges resize, and hover/drag visuals match expectations from the Python version.
- Grid selection rectangles no longer render over the scrollbar or CC lane:
  - Viewport height excludes scrollbar+CC lane.
  - Selection overlay is drawn immediately after notes (before scrollbar/CC lane).

## 5. Panel alignment & piano keys

This is the part that is still slightly off and needs careful treatment by the next agent.

**What works:**

- Input routing:
  - Inspector/Browser clicks no longer activate the piano roll.
  - Piano roll responds only when clicking inside its panel card.
- Debug lines:
  - Panel‑level cyan/orange lines (`editor_piano_roll.cpp`) live inside the piano roll card and move with the panel.
  - Widget‑level cyan/red lines (`PianoRollWidget::draw()`) now move with the panel and stay inside the card.
  - When the panel is moved left/right, the widget’s own origin and grid start follow it correctly.
- Piano keys:
  - At one point during this session, the keys were visible in the correct place (left strip inside the piano roll card) and the grid started at the red line.

**What regressed:**

- After fixing the squashed top controls by moving the cursor inside `draw()`, the piano-key strip again appears visually “under” the Inspector when the piano-roll panel is on the right (default layout).
  - The keys are still there; they become fully visible if the vertical splitter is dragged all the way left (Inspector hidden).
  - This strongly suggests a subtle interaction between:
    - Where the widget moves the cursor internally.
    - How the panel’s clip rect and z‑order interact with the `InvisibleButton` canvas.

**Key insight:**

- The Python piano roll assumed a full-screen canvas at (0,0). In the C++ port:
  - We’ve introduced an explicit canvas rect and moved the cursor to its origin.
  - But we **also** rely on `InvisibleButton` + `GetItemRectMin/Max()` internally in the renderer and overlays.
  - When we changed cursor positioning to fix the top controls, we reintroduced a mismatch between the “panel’s idea” of the canvas start and the widget’s.

## 6. Recommended next steps

For the next agent, here’s a concrete plan to finish this cleanly:

1. **Stop moving the cursor inside `PianoRollWidget` entirely.**
   - Let the panel (`editor_piano_roll.cpp`) own ImGui cursor positioning.
   - Inside the widget, treat `canvas_origin_x_/y_` + `canvas_width_/height_` purely as a coordinate system; do not call `ImGui::SetCursorScreenPos` from within `draw()` or `renderer`.

2. **Make the renderer/overlays respect the explicit canvas rect.**
   - In `PianoRollRenderer::render`:
     - Drop reliance on `GetWindowDrawList()` + `InvisibleButton` for origin.
     - Use `canvas_origin_x_/y_` and `canvas_width_/height_` from the widget to:
       - Compute `ImVec2 origin(canvas_origin_x_, canvas_origin_y_)`.
       - Determine widget size (for `ItemSize`/`ItemAdd` if you still want an ImGui item).
   - In `handle_pointer_events` and `RenderSelectionOverlay`:
     - Replace `GetItemRectMin/Max()` with explicit `canvas_origin_x_/y_` and `canvas_width_/height_`:
       - Canvas bounds: `[canvas_origin_x_, canvas_origin_x_ + canvas_width_)` × `[canvas_origin_y_, canvas_origin_y_ + canvas_height_)`.
       - Local coords: `local_x = mouse.x - canvas_origin_x_`, `local_y = mouse.y - canvas_origin_y_`.

3. **Use a single source of truth for screen↔local mapping.**
   - Ideally, add helper functions in `PianoRollWidget`:

     ```cpp
     std::pair<float,float> screen_to_local(float sx, float sy) const;
     std::pair<float,float> local_to_screen(float lx, float ly) const;
     ```

   - Then use those in:
     - `handle_pointer_events`
     - `RenderSelectionOverlay`
     - Key strip drawing
     - Grid/scrollbar/CC lane positioning

4. **Once that’s in place, remove the debug lines.**
   - Panel-level cyan/orange in `editor_piano_roll.cpp`.
   - Widget-level cyan/red in `PianoRollWidget::draw()`.

5. **Verify visually in both layouts.**
   - Piano-roll panel on the left:
     - Keys at left edge of card.
     - Grid at red line.
   - Piano-roll panel on the right (default):
     - Keys still at left edge of card (right of splitter), never under Inspector.
     - Scrollbar/CC lane visible and responsive as before.

## 7. Quick summary for the next instance

- The C++ piano roll is integrated and functional inside `daw_widgets`, with:
  - Note editing, grid, scrollbar, CC lane, and top controls all present.
  - Scrollbar and CC behaviour close to the Python version.
  - Panel-aware canvas rect via `set_canvas_rect` / `draw_in_rect`.
- The big remaining UX issue is:
  - The piano-key strip can still appear visually under the Inspector when the piano-roll panel is on the right.
  - This is due to a residual mismatch between the panel’s content rect and the widget’s internal canvas origin/clip, not a model/interaction bug.
- The safe path forward is:
  - Let the panel own cursor positioning.
  - Make the widget honor the explicit canvas rect everywhere (renderer, pointer, overlays) without calling `SetCursorScreenPos` internally.
  - Then remove debug lines and verify in both panel layouts.

Once that’s done, the C++ piano roll should behave and align like the original Python version but within the DAW’s panel system.  

## 8. Original Python piano roll reference

- Repository path: `/home/claude_code/dpg_piano_roll`
  - Main demo entry point: `examples/enhanced_piano_roll.py`
    - This creates a DearPyGui window, instantiates `EnhancedPianoRoll` (a thin wrapper over `UnifiedPianoRoll`), and wires up transport controls (play/stop/loop/tempo display) plus a demo clip.
  - Core implementation: `piano_roll/unified_piano_roll_main.py` and related modules under `piano_roll/`:
    - `UnifiedPianoRoll` class combines:
      - Rendering mixin (`PianoRollRenderingMixin`).
      - Interaction mixin (`PianoRollInteractionMixin`).
      - `CoordinateSystem`, `NoteManager`, `GridSnapSystem`, `CustomScrollbar`, `PointerTool`, `InteractionSystem`, and playback markers.
    - The Python version assumes a full-screen single canvas and manages all scroll/zoom/selection behaviour within that canvas.
  - Single-canvas architecture and scrollbar design are documented in:
    - `SINGLE_CANVAS_PLAN.md` / `SINGLE_CANVAS_PLAN_V2.md`
    - `docs/CUSTOM_SCROLLBAR_SOLUTION.md`
    - `tests/unit/single_canvas/test_custom_scrollbar.py`

The C++ port in `piano_roll_cpp_port` mirrors the Python data/coord/render/interaction layers but must work inside the DAW’s panel system, so the main deltas are around canvas origin, clipping, and panel-aware layout (the behavioural semantics of notes/grid/snap/scroll are intended to match the Python version). 
