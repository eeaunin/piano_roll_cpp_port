#include "piano_roll/interaction.hpp"

#include <algorithm>

namespace piano_roll {

PointerTool::PointerTool(NoteManager& notes,
                         CoordinateSystem& coords,
                         GridSnapSystem* snap_system)
    : notes_(&notes),
      coords_(&coords),
      snap_(snap_system) {
    default_note_duration_ =
        static_cast<Duration>(coords.ticks_per_beat());
}

void PointerTool::selection_rectangle_world(double& x1,
                                            double& y1,
                                            double& x2,
                                            double& y2) const noexcept {
    if (!rect_active_) {
        x1 = y1 = x2 = y2 = 0.0;
        return;
    }
    x1 = std::min(rect_start_world_x_, rect_end_world_x_);
    x2 = std::max(rect_start_world_x_, rect_end_world_x_);
    y1 = std::min(rect_start_world_y_, rect_end_world_y_);
    y2 = std::max(rect_start_world_y_, rect_end_world_y_);
}

Tick PointerTool::apply_snap(Tick raw_tick,
                             const ModifierKeys& mods) const {
    if (!snap_) {
        return raw_tick;
    }
    // Shift disables snapping (matches Python magnetic_snap_tick).
    if (mods.shift) {
        return raw_tick;
    }

    double ppb = coords_ ? coords_->pixels_per_beat() : 0.0;
    auto [snapped, snapped_flag] =
        snap_->magnetic_snap(raw_tick, ppb);
    (void)snapped_flag;
    return snapped;
}

void PointerTool::begin_rectangle_selection(double world_x,
                                            double world_y,
                                            const ModifierKeys& mods) {
    action_ = Action::RectangleSelection;
    hover_ = {};
    rect_active_ = true;
    rect_start_world_x_ = world_x;
    rect_start_world_y_ = world_y;
    rect_end_world_x_ = world_x;
    rect_end_world_y_ = world_y;

    initial_selection_.clear();
    for (const Note& n : notes_->notes()) {
        if (n.selected) {
            initial_selection_.push_back(n.id);
        }
    }
}

void PointerTool::update_rectangle_selection(const ModifierKeys& mods) {
    if (!rect_active_) {
        return;
    }

    double x1 = std::min(rect_start_world_x_, rect_end_world_x_);
    double x2 = std::max(rect_start_world_x_, rect_end_world_x_);
    double y1 = std::min(rect_start_world_y_, rect_end_world_y_);
    double y2 = std::max(rect_start_world_y_, rect_end_world_y_);

    // Build a set of note IDs inside the rectangle.
    std::vector<NoteId> in_rect;
    for (const Note& note : notes_->notes()) {
        double note_x1 = coords_->tick_to_world(note.tick);
        double note_x2 = coords_->tick_to_world(note.end_tick());
        double note_y1 = coords_->key_to_world_y(note.key);
        double note_y2 = note_y1 + coords_->key_height();

        bool overlaps =
            (note_x1 < x2 && note_x2 > x1 &&
             note_y1 < y2 && note_y2 > y1);
        if (overlaps) {
            in_rect.push_back(note.id);
        }
    }

    // Apply selection: emulate Python selection_rect_mode semantics:
    // Alt = subtract; Ctrl = add; Shift = toggle; otherwise replace.
    if (mods.alt) {
        std::unordered_set<NoteId> base(initial_selection_.begin(),
                                        initial_selection_.end());
        notes_->clear_selection();
        for (NoteId id : initial_selection_) {
            notes_->select(id, true);
        }
        for (NoteId id : in_rect) {
            if (base.find(id) != base.end()) {
                notes_->deselect(id);
            }
        }
    } else if (mods.ctrl) {
        notes_->clear_selection();
        for (NoteId id : initial_selection_) {
            notes_->select(id, true);
        }
        for (NoteId id : in_rect) {
            notes_->select(id, true);
        }
    } else if (mods.shift) {
        std::unordered_set<NoteId> base(initial_selection_.begin(),
                                         initial_selection_.end());
        // Start with initial selection.
        notes_->clear_selection();
        for (NoteId id : initial_selection_) {
            notes_->select(id, true);
        }
        for (NoteId id : in_rect) {
            if (base.find(id) != base.end()) {
                notes_->deselect(id);
            } else {
                notes_->select(id, true);
            }
        }
    } else {
        notes_->clear_selection();
        for (NoteId id : in_rect) {
            notes_->select(id, true);
        }
    }
}

bool PointerTool::hovered_note_world(double& x1,
                                     double& y1,
                                     double& x2,
                                     double& y2,
                                     HoverEdge& edge) const noexcept {
    if (!hover_.has_note || !notes_ || !coords_) {
        return false;
    }
    const Note* note = notes_->find_by_id(hover_.note_id);
    if (!note) {
        return false;
    }

    double world_x1 = coords_->tick_to_world(note->tick);
    double world_x2 = coords_->tick_to_world(note->end_tick());
    double world_y1 = coords_->key_to_world_y(note->key);
    double world_y2 = world_y1 + coords_->key_height();

    x1 = world_x1;
    x2 = world_x2;
    y1 = world_y1;
    y2 = world_y2;
    edge = hover_.edge;
    return true;
}

void PointerTool::on_mouse_down(MouseButton button,
                                double screen_x,
                                double screen_y,
                                const ModifierKeys& mods) {
    if (button != MouseButton::Left || !notes_ || !coords_) {
        return;
    }

    pending_click_ = true;
    click_start_screen_x_ = screen_x;
    click_start_screen_y_ = screen_y;

    // Convert to world space and derive tick/key.
    auto [world_x, world_y] =
        coords_->screen_to_world(screen_x, screen_y);

    Tick tick = coords_->world_to_tick(world_x);
    MidiKey key = coords_->world_y_to_key(world_y);

    // Check if clicked on an existing note.
    Note* note = notes_->note_at(tick, key);
    if (note) {
        active_note_id_ = note->id;
        initial_tick_ = note->tick;
        initial_duration_ = note->duration;
        initial_key_ = note->key;

        double note_x1 = coords_->tick_to_world(note->tick);
        double note_x2 = coords_->tick_to_world(note->end_tick());
        double note_y1 = coords_->key_to_world_y(note->key);

        drag_offset_world_x_ = world_x - note_x1;
        drag_offset_world_y_ = world_y - note_y1;

        // Selection behaviour: click selects note, Ctrl/Shift add to selection.
        bool already_selected = note->selected;
        if (!already_selected) {
            if (!(mods.ctrl || mods.shift)) {
                notes_->clear_selection();
            }
            notes_->select(note->id, true);
        } else if (mods.ctrl && !enable_ctrl_drag_duplicate_) {
            // Ctrl-click on an already selected note: treat as potential
            // toggle-on-release if this remains a click, matching the Python
            // pending_toggle_on_release behaviour.
            pending_toggle_on_release_ = true;
        }

        // Ctrl+drag duplication: duplicate current selection before starting drag.
        is_duplicating_ = false;
        drag_original_selection_.clear();
        if (enable_ctrl_drag_duplicate_ && mods.ctrl) {
            drag_original_selection_ = notes_->selected_ids();
            if (!drag_original_selection_.empty()) {
                std::vector<NoteId> new_ids;
                new_ids.reserve(drag_original_selection_.size());
                for (NoteId id : drag_original_selection_) {
                    const Note* src = notes_->find_by_id(id);
                    if (!src) continue;
                    NoteId new_id =
                        notes_->create_note(src->tick,
                                            src->duration,
                                            src->key,
                                            src->velocity,
                                            src->channel,
                                            /*selected=*/true,
                                            /*record_undo=*/false,
                                            /*allow_overlap=*/false);
                    if (new_id != 0) {
                        new_ids.push_back(new_id);
                    }
                }
                if (!new_ids.empty()) {
                    notes_->clear_selection();
                    for (NoteId nid : new_ids) {
                        notes_->select(nid, true);
                    }
                    active_note_id_ = new_ids.front();
                    is_duplicating_ = true;
                }
            }
        }

        Note* drag_note = notes_->find_by_id(active_note_id_);
        if (!drag_note) {
            action_ = Action::None;
            return;
        }

        note_x1 = coords_->tick_to_world(drag_note->tick);
        note_x2 = coords_->tick_to_world(drag_note->end_tick());

        // Decide between drag and resize based on proximity to note edges.
        double dx_left = std::abs(world_x - note_x1);
        double dx_right = std::abs(world_x - note_x2);

        if (dx_left <= edge_threshold_world_) {
            action_ = Action::ResizingLeft;
        } else if (dx_right <= edge_threshold_world_) {
            action_ = Action::ResizingRight;
        } else {
            action_ = Action::DraggingNote;
        }
        rect_active_ = false;
        hover_ = {};
        return;
    }

    // Clicked in empty space: start rectangle selection.
    begin_rectangle_selection(world_x, world_y, mods);
}

void PointerTool::on_mouse_move(double screen_x,
                                double screen_y,
                                const ModifierKeys& mods) {
    if (!notes_ || !coords_) {
        return;
    }

    auto [world_x, world_y] =
        coords_->screen_to_world(screen_x, screen_y);

    if (action_ == Action::None) {
        if (pending_click_) {
            double dx = std::abs(screen_x - click_start_screen_x_);
            double dy = std::abs(screen_y - click_start_screen_y_);
            if (dx <= drag_threshold_pixels_ &&
                dy <= drag_threshold_pixels_) {
                // Still within click slop: only update hover, do not
                // start a drag/resize yet.
                HoverState new_hover{};
                Tick tick = coords_->world_to_tick(world_x);
                MidiKey key = coords_->world_y_to_key(world_y);
                Note* note = notes_->note_at(tick, key);
                if (note) {
                    new_hover.has_note = true;
                    new_hover.note_id = note->id;

                    double note_x1 = coords_->tick_to_world(note->tick);
                    double note_x2 = coords_->tick_to_world(note->end_tick());
                    double dx_left = std::abs(world_x - note_x1);
                    double dx_right = std::abs(world_x - note_x2);

                    if (dx_left <= edge_threshold_world_) {
                        new_hover.edge = HoverEdge::Left;
                    } else if (dx_right <= edge_threshold_world_) {
                        new_hover.edge = HoverEdge::Right;
                    } else {
                        new_hover.edge = HoverEdge::Body;
                    }
                }
                hover_ = new_hover;
                return;
            }
            // Threshold exceeded: treat as movement and clear pending flag.
            pending_click_ = false;
        }

        HoverState new_hover{};
        Tick tick = coords_->world_to_tick(world_x);
        MidiKey key = coords_->world_y_to_key(world_y);
        Note* note = notes_->note_at(tick, key);
        if (note) {
            new_hover.has_note = true;
            new_hover.note_id = note->id;

            double note_x1 = coords_->tick_to_world(note->tick);
            double note_x2 = coords_->tick_to_world(note->end_tick());
            double dx_left = std::abs(world_x - note_x1);
            double dx_right = std::abs(world_x - note_x2);

            if (dx_left <= edge_threshold_world_) {
                new_hover.edge = HoverEdge::Left;
            } else if (dx_right <= edge_threshold_world_) {
                new_hover.edge = HoverEdge::Right;
            } else {
                new_hover.edge = HoverEdge::Body;
            }
        }
        hover_ = new_hover;
        return;
    }

    switch (action_) {
    case Action::DraggingNote: {
        Note* anchor = notes_->find_by_id(active_note_id_);
        if (!anchor) {
            return;
        }

        // Compute new start position for the anchor using stored offsets.
        double new_world_x = world_x - drag_offset_world_x_;
        double new_world_y = world_y - drag_offset_world_y_;

        Tick new_tick = coords_->world_to_tick(new_world_x);
        MidiKey new_key = coords_->world_y_to_key(new_world_y);
        new_tick = apply_snap(new_tick, mods);

        // Compute deltas relative to current position of the anchor.
        Tick delta_tick = new_tick - anchor->tick;
        int delta_key = new_key - anchor->key;

        if (delta_tick == 0 && delta_key == 0) {
            break;
        }

        // Move all selected notes by the same delta, matching the Python
        // behaviour where dragging a selected note moves the whole group.
        std::vector<NoteId> ids = notes_->selected_ids();
        if (ids.empty()) {
            ids.push_back(active_note_id_);
        }
        for (NoteId id : ids) {
            notes_->move_note(id,
                              delta_tick,
                              delta_key,
                              /*record_undo=*/false,
                              /*allow_overlap=*/false);
        }
        break;
    }
    case Action::ResizingLeft:
    case Action::ResizingRight: {
        Note* note = notes_->find_by_id(active_note_id_);
        if (!note) {
            return;
        }

        double note_x1 = coords_->tick_to_world(initial_tick_);
        double note_x2 = coords_->tick_to_world(initial_tick_ + initial_duration_);

        double new_world_x_left =
            (action_ == Action::ResizingLeft) ? world_x : note_x1;
        double new_world_x_right =
            (action_ == Action::ResizingRight) ? world_x : note_x2;

        Tick new_tick_left =
            coords_->world_to_tick(new_world_x_left);
        Tick new_tick_right =
            coords_->world_to_tick(new_world_x_right);

        new_tick_left = apply_snap(new_tick_left, mods);
        new_tick_right = apply_snap(new_tick_right, mods);

        // Enforce a small minimum note length, mirroring the Python
        // MIN_NOTE_LENGTH_TICKS behaviour.
        const Duration MIN_NOTE_LENGTH_TICKS = 10;
        if (action_ == Action::ResizingLeft) {
            Tick max_left =
                initial_tick_ + initial_duration_ -
                MIN_NOTE_LENGTH_TICKS;
            if (new_tick_left > max_left) {
                new_tick_left = max_left;
            }
        } else {
            Tick min_right =
                initial_tick_ + MIN_NOTE_LENGTH_TICKS;
            if (new_tick_right < min_right) {
                new_tick_right = min_right;
            }
        }

        if (new_tick_right <= new_tick_left) {
            return;
        }

        // Update note tick/duration directly through NoteManager.
        Tick delta_tick = new_tick_left - note->tick;
        Duration new_duration = new_tick_right - new_tick_left;

        notes_->move_note(active_note_id_,
                          delta_tick,
                          0,
                          /*record_undo=*/false,
                          /*allow_overlap=*/false);
        notes_->resize_note(active_note_id_,
                            new_duration,
                            /*record_undo=*/false,
                            /*allow_overlap=*/false);

        // Remember the last resized length for subsequent note creation.
        default_note_duration_ = new_duration;
        break;
    }
    case Action::RectangleSelection: {
        rect_end_world_x_ = world_x;
        rect_end_world_y_ = world_y;
        update_rectangle_selection(mods);
        break;
    }
    case Action::None:
    default:
        break;
    }
}

void PointerTool::on_mouse_up(MouseButton button,
                              double screen_x,
                              double screen_y,
                              const ModifierKeys& mods) {
    if (button != MouseButton::Left) {
        return;
    }

    // If we were rectangle-selecting and ended with a zero-area rectangle
    // (no real drag), treat this as a click in empty space. In that case,
    // clear the selection for plain clicks (no modifiers), matching the
    // typical piano-roll behaviour and the Python implementation.
    if (action_ == Action::RectangleSelection &&
        notes_ && coords_ &&
        !mods.ctrl && !mods.shift && !mods.alt) {
        double x1 = std::min(rect_start_world_x_, rect_end_world_x_);
        double x2 = std::max(rect_start_world_x_, rect_end_world_x_);
        double y1 = std::min(rect_start_world_y_, rect_end_world_y_);
        double y2 = std::max(rect_start_world_y_, rect_end_world_y_);
        if (x1 == x2 && y1 == y2) {
            notes_->clear_selection();
        }
    }

    // If we had a pending toggle request and this ended as a click (no drag
    // action active), perform Ctrl-click toggle semantics.
    if (pending_toggle_on_release_ &&
        !is_duplicating_ &&
        action_ == Action::None && notes_ && coords_) {
        auto [world_x, world_y] =
            coords_->screen_to_world(screen_x, screen_y);
        Tick tick = coords_->world_to_tick(world_x);
        MidiKey key = coords_->world_y_to_key(world_y);
        Note* note = notes_->note_at(tick, key);
        if (note && mods.ctrl) {
            bool selected = note->selected;
            if (selected) {
                notes_->deselect(note->id);
            } else {
                notes_->select(note->id, true);
            }
        }
    }

    // Drag/resize currently operate without recording undo history. Undo
    // wiring can be added later in a DAW-specific command layer.
    action_ = Action::None;
    active_note_id_ = 0;
    rect_active_ = false;
    is_duplicating_ = false;
    pending_click_ = false;
    pending_toggle_on_release_ = false;
}

void PointerTool::on_double_click(MouseButton button,
                                  double screen_x,
                                  double screen_y,
                                  const ModifierKeys& mods) {
    if (button != MouseButton::Left || !notes_ || !coords_) {
        return;
    }

    auto [world_x, world_y] =
        coords_->screen_to_world(screen_x, screen_y);

    Tick tick = coords_->world_to_tick(world_x);
    MidiKey key = coords_->world_y_to_key(world_y);

    // If clicking an existing note: delete it.
    Note* note = notes_->note_at(tick, key);
    if (note) {
        notes_->remove_note(note->id, /*record_undo=*/false);
        return;
    }

    // Otherwise: create a new note at this cell.
    Tick snapped_tick = apply_snap(tick, mods);
    if (snapped_tick < 0) {
        snapped_tick = 0;
    }
    if (key < 0) {
        key = 0;
    } else if (key > 127) {
        key = 127;
    }

    notes_->create_note(snapped_tick,
                        default_note_duration_,
                        key,
                        /*velocity=*/100,
                        /*channel=*/0,
                        /*selected=*/true,
                        /*record_undo=*/false,
                        /*allow_overlap=*/false);
}

}  // namespace piano_roll
