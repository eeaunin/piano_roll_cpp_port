# piano_roll_cpp_port Plan

Goal:
- Build a standalone C++20 + Dear ImGui piano roll that reproduces the core UX of `../dpg_piano_roll` without depending on the main DAW repos.

Initial Milestones:
- M1: Core data + coordinates
  - Note representation (pitch, start, duration, velocity, selection).
  - Coordinate system for ticks↔pixels, zoom, and scroll.
- M2: Basic rendering
  - Piano keys, grid, notes, and playhead rendering in ImGui.
- M3: Core interactions
  - Mouse-based note creation, move, resize, delete; basic selection box.
- M4: Configuration and presets
  - Simple config struct for dimensions and colours; presets for “compact” vs “spacious” layouts.

Constraints:
- Keep dependencies minimal (ImGui + std); no engine/audio integration.
- Use the behavioural notes in `../dpg_piano_roll/bitwig_piano_roll_ui_behaviours.txt` as guidance rather than as a strict spec.
- Ensure the code can compile as a small demo app without external DAW context.

See `docs/PLAN.md` for a more detailed architecture and implementation-status plan for the C++ port.

