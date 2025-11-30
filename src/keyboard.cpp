#include "piano_roll/keyboard.hpp"

namespace piano_roll {

KeyboardController::KeyboardController(NoteManager& notes)
    : notes_(&notes) {}

bool KeyboardController::on_key_press(Key key, const ModifierKeys& mods) {
    if (!notes_) {
        return false;
    }

    // Ctrl+A: select all
    if (mods.ctrl && key == Key::A) {
        handle_select_all();
        return true;
    }

    // Delete / Backspace: delete selected
    if (key == Key::Delete || key == Key::Backspace) {
        handle_delete();
        return true;
    }

    // Ctrl+C: copy
    if (mods.ctrl && key == Key::C) {
        handle_copy();
        return true;
    }

    // Ctrl+V: paste
    if (mods.ctrl && key == Key::V) {
        handle_paste();
        return true;
    }

    // Ctrl+Z: undo
    if (mods.ctrl && key == Key::Z) {
        return notes_->undo();
    }

    // Ctrl+Y: redo
    if (mods.ctrl && key == Key::Y) {
        return notes_->redo();
    }

    // Arrow keys: move selected notes (snap-aware in time, semitone/octave in pitch).
    if (key == Key::Up || key == Key::Down ||
        key == Key::Left || key == Key::Right) {
        Tick delta_tick{0};
        int delta_key{0};
        if (key == Key::Up) {
            delta_key = mods.shift ? 12 : 1;
        } else if (key == Key::Down) {
            delta_key = mods.shift ? -12 : -1;
        } else if (key == Key::Left || key == Key::Right) {
            if (!snap_) {
                return false;
            }
            int tpb = snap_->ticks_per_beat();
            // Fine unit: 1/128 note (1920/128 ticks per whole note).
            Tick fine =
                static_cast<Tick>(4 * tpb / 128);
            Tick base = snap_->snap_division().ticks;
            if (coords_ && snap_->snap_mode() == SnapMode::Adaptive) {
                double ppb = coords_->pixels_per_beat();
                const SnapDivision& div =
                    snap_->adaptive_division(ppb,
                                              /*for_grid=*/false);
                base = div.ticks;
            }
            Tick step = mods.shift ? fine : base;
            if (key == Key::Left) {
                step = -step;
            }
            delta_tick = step;
        }

        // For pitch moves, ensure the whole group can move without exceeding
        // MIDI key limits (0..127), mirroring the Python behaviour.
        const auto& all = notes_->notes();
        if (delta_key != 0) {
            int min_key = 127;
            int max_key = 0;
            bool any = false;
            for (const Note& n : all) {
                if (!n.selected) {
                    continue;
                }
                any = true;
                if (n.key < min_key) min_key = n.key;
                if (n.key > max_key) max_key = n.key;
            }
            if (!any) {
                return false;
            }
            if (max_key + delta_key > 127 ||
                min_key + delta_key < 0) {
                return false;
            }
        }

        // For time moves, avoid pushing group start before tick 0 to prevent
        // relative spacing distortion from per-note clamping.
        if (delta_tick != 0) {
            Tick min_tick = 0;
            bool any = false;
            for (const Note& n : all) {
                if (!n.selected) {
                    continue;
                }
                if (!any) {
                    min_tick = n.tick;
                    any = true;
                } else if (n.tick < min_tick) {
                    min_tick = n.tick;
                }
            }
            if (!any) {
                return false;
            }
            if (min_tick + delta_tick < 0) {
                return false;
            }
        }

        bool moved_any = false;
        std::vector<NoteId> ids = notes_->selected_ids();
        if (!ids.empty() && (delta_tick != 0 || delta_key != 0)) {
            notes_->snapshot_for_undo();
            for (NoteId id : ids) {
                if (notes_->move_note(id,
                                      delta_tick,
                                      delta_key,
                                      /*record_undo=*/false,
                                      /*allow_overlap=*/false)) {
                    moved_any = true;
                }
            }
        }
        return moved_any;
    }

    return false;
}

void KeyboardController::handle_delete() {
    if (!notes_) {
        return;
    }

    // Collect IDs of selected notes first to avoid modifying while iterating.
    std::vector<NoteId> to_delete;
    for (const Note& n : notes_->notes()) {
        if (n.selected) {
            to_delete.push_back(n.id);
        }
    }

    if (!to_delete.empty()) {
        notes_->snapshot_for_undo();
        for (NoteId id : to_delete) {
            notes_->remove_note(id, /*record_undo=*/false);
        }
    }
}

void KeyboardController::handle_select_all() {
    if (!notes_) {
        return;
    }
    notes_->select_all();
}

void KeyboardController::handle_copy() {
    if (!notes_) {
        return;
    }

    clipboard_.clear();
    for (const Note& n : notes_->notes()) {
        if (n.selected) {
            clipboard_.push_back(n);
        }
    }
}

void KeyboardController::handle_paste() {
    if (!notes_ || clipboard_.empty()) {
        return;
    }

    // Simple behaviour: paste copies at their original tick positions.
    // More advanced behaviour (e.g. paste at playhead) can be implemented
    // at a higher layer by adjusting ticks before creating notes.
    notes_->snapshot_for_undo();
    for (const Note& src : clipboard_) {
        notes_->create_note(src.tick,
                            src.duration,
                            src.key,
                            src.velocity,
                            src.channel,
                            /*selected=*/true,
                            /*record_undo=*/false,
                            /*allow_overlap=*/false);
    }
}

}  // namespace piano_roll
