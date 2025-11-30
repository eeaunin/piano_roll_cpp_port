#pragma once

#include "piano_roll/note_manager.hpp"
#include "piano_roll/grid_snap.hpp"
#include "piano_roll/interaction.hpp"
#include "piano_roll/coordinate_system.hpp"

#include <vector>

namespace piano_roll {

// Logical keys used by the keyboard controller. The host is responsible for
// mapping framework-specific key codes (e.g. ImGui, GLFW) to these values.
enum class Key {
    Delete,
    Backspace,
    A,
    C,
    V,
    Z,
    Y,
    Up,
    Down,
    Left,
    Right,
};

// Simple keyboard controller that applies a minimal set of shortcuts to
// a NoteManager:
// - Ctrl+A: select all notes.
// - Delete / Backspace: delete selected notes.
// - Ctrl+C: copy selected notes into an internal clipboard.
// - Ctrl+V: paste clipboard (at original positions for now).
// - Ctrl+Z: undo.
// - Ctrl+Y: redo.
class KeyboardController {
public:
    explicit KeyboardController(NoteManager& notes);

    // Handle a key press event. Returns true if the event was consumed.
    bool on_key_press(Key key, const ModifierKeys& mods);

    // Clipboard helpers.
    bool has_clipboard() const noexcept {
        return !clipboard_.empty();
    }

    // Paste the current clipboard so that the earliest note in the clipboard
    // starts at target_tick. Returns true if any notes were created. This is
    // intended for higher-level callers (e.g. PianoRollWidget) that want
    // "paste at playhead" or similar behaviours.
    bool paste_at_tick(Tick target_tick);

    void set_snap_system(GridSnapSystem* snap) noexcept {
        snap_ = snap;
    }

    void set_coordinate_system(CoordinateSystem* coords) noexcept {
        coords_ = coords;
    }

private:
    NoteManager* notes_{nullptr};
    GridSnapSystem* snap_{nullptr};
    CoordinateSystem* coords_{nullptr};

    // Clipboard stores copies of notes with absolute tick positions for now.
    std::vector<Note> clipboard_;

    void handle_delete();
    void handle_select_all();
    void handle_copy();
    void handle_paste();
};

}  // namespace piano_roll
