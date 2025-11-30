#include "piano_roll/note_manager.hpp"

#include <algorithm>

namespace piano_roll {

NoteId NoteManager::create_note(Tick tick,
                                Duration duration,
                                MidiKey key,
                                Velocity velocity,
                                Channel channel,
                                bool selected,
                                bool record_undo,
                                bool allow_overlap) {
    Note new_note{tick, duration, key, velocity, channel, selected};
    new_note.id = allocate_id();

    if (!allow_overlap && would_overlap(new_note)) {
        // Do not consume the ID; caller can decide what to do.
        return 0;
    }

    if (record_undo) {
        push_undo_state();
    }

    std::size_t index = allocate_index_for_new_note();
    notes_[index] = new_note;

    // Update indexes for the new note only.
    id_to_index_[new_note.id] = index;
    spatial_index_[new_note.key].push_back(index);

    if (selected) {
        selected_note_ids_.insert(new_note.id);
    }

    return new_note.id;
}

bool NoteManager::remove_note(NoteId id, bool record_undo) {
    auto id_it = id_to_index_.find(id);
    if (id_it == id_to_index_.end()) {
        return false;
    }

    if (record_undo) {
        push_undo_state();
    }

    std::size_t index_to_remove = id_it->second;
    if (index_to_remove >= notes_.size()) {
        return false;
    }

    notes_.erase(notes_.begin() + static_cast<std::ptrdiff_t>(index_to_remove));

    // Rebuild indexes and selection after removal.
    rebuild_indexes();
    rebuild_selection_from_notes();

    return true;
}

bool NoteManager::move_note(NoteId id,
                            Tick delta_tick,
                            int key_delta,
                            bool record_undo,
                            bool allow_overlap) {
    Note* note = find_by_id(id);
    if (note == nullptr) {
        return false;
    }

    Note original = *note;
    note->move_by(delta_tick, key_delta);

    if (!allow_overlap && would_overlap(*note, id)) {
        *note = original;
        return false;
    }

    if (record_undo) {
        push_undo_state();
    }

    rebuild_indexes();
    return true;
}

bool NoteManager::resize_note(NoteId id,
                              Duration new_duration,
                              bool record_undo,
                              bool allow_overlap) {
    Note* note = find_by_id(id);
    if (note == nullptr) {
        return false;
    }

    if (new_duration <= 0) {
        return false;
    }

    Note original = *note;
    note->resize_to(new_duration);

    if (!allow_overlap && would_overlap(*note, id)) {
        *note = original;
        return false;
    }

    if (record_undo) {
        push_undo_state();
    }

    rebuild_indexes();
    return true;
}

bool NoteManager::would_overlap(const Note& probe,
                                std::optional<NoteId> exclude_id) const {
    auto index_it = spatial_index_.find(probe.key);
    if (index_it == spatial_index_.end()) {
        return false;
    }

    const std::vector<std::size_t>& indices_for_key = index_it->second;
    for (std::size_t note_index : indices_for_key) {
        if (note_index >= notes_.size()) {
            continue;
        }
        const Note& existing = notes_[note_index];
        if (exclude_id.has_value() && existing.id == *exclude_id) {
            continue;
        }
        if (probe.overlaps(existing)) {
            return true;
        }
    }

    return false;
}

Note* NoteManager::find_by_id(NoteId id) noexcept {
    auto id_it = id_to_index_.find(id);
    if (id_it == id_to_index_.end()) {
        return nullptr;
    }
    std::size_t index = id_it->second;
    if (index >= notes_.size()) {
        return nullptr;
    }
    return &notes_[index];
}

const Note* NoteManager::find_by_id(NoteId id) const noexcept {
    auto id_it = id_to_index_.find(id);
    if (id_it == id_to_index_.end()) {
        return nullptr;
    }
    std::size_t index = id_it->second;
    if (index >= notes_.size()) {
        return nullptr;
    }
    return &notes_[index];
}

Note* NoteManager::note_at(Tick tick, MidiKey key) noexcept {
    auto index_it = spatial_index_.find(key);
    if (index_it == spatial_index_.end()) {
        return nullptr;
    }

    const std::vector<std::size_t>& indices_for_key = index_it->second;
    for (std::size_t note_index : indices_for_key) {
        if (note_index >= notes_.size()) {
            continue;
        }
        Note& note = notes_[note_index];
        if (note.contains_tick(tick)) {
            return &note;
        }
    }
    return nullptr;
}

const Note* NoteManager::note_at(Tick tick, MidiKey key) const noexcept {
    auto index_it = spatial_index_.find(key);
    if (index_it == spatial_index_.end()) {
        return nullptr;
    }

    const std::vector<std::size_t>& indices_for_key = index_it->second;
    for (std::size_t note_index : indices_for_key) {
        if (note_index >= notes_.size()) {
            continue;
        }
        const Note& note = notes_[note_index];
        if (note.contains_tick(tick)) {
            return &note;
        }
    }
    return nullptr;
}

std::vector<Note*> NoteManager::notes_in_range(Tick start_tick,
                                               Tick end_tick,
                                               MidiKey min_key,
                                               MidiKey max_key) noexcept {
    std::vector<Note*> result;
    if (start_tick >= end_tick || min_key > max_key) {
        return result;
    }

    for (MidiKey key = min_key; key <= max_key; ++key) {
        auto index_it = spatial_index_.find(key);
        if (index_it == spatial_index_.end()) {
            continue;
        }

        const std::vector<std::size_t>& indices_for_key = index_it->second;
        for (std::size_t note_index : indices_for_key) {
            if (note_index >= notes_.size()) {
                continue;
            }
            Note& note = notes_[note_index];
            if (note.tick < end_tick && note.end_tick() > start_tick) {
                result.push_back(&note);
            }
        }
    }

    return result;
}

std::vector<const Note*> NoteManager::notes_in_range(Tick start_tick,
                                                     Tick end_tick,
                                                     MidiKey min_key,
                                                     MidiKey max_key) const noexcept {
    std::vector<const Note*> result;
    if (start_tick >= end_tick || min_key > max_key) {
        return result;
    }

    for (MidiKey key = min_key; key <= max_key; ++key) {
        auto index_it = spatial_index_.find(key);
        if (index_it == spatial_index_.end()) {
            continue;
        }

        const std::vector<std::size_t>& indices_for_key = index_it->second;
        for (std::size_t note_index : indices_for_key) {
            if (note_index >= notes_.size()) {
                continue;
            }
            const Note& note = notes_[note_index];
            if (note.tick < end_tick && note.end_tick() > start_tick) {
                result.push_back(&note);
            }
        }
    }

    return result;
}

void NoteManager::select(NoteId id, bool add_to_selection) {
    Note* note = find_by_id(id);
    if (note == nullptr) {
        return;
    }

    if (!add_to_selection) {
        clear_selection();
    }

    note->selected = true;
    selected_note_ids_.insert(id);
}

void NoteManager::deselect(NoteId id) {
    Note* note = find_by_id(id);
    if (note == nullptr) {
        return;
    }
    note->selected = false;
    selected_note_ids_.erase(id);
}

void NoteManager::clear_selection() {
    for (Note& note : notes_) {
        note.selected = false;
    }
    selected_note_ids_.clear();
}

void NoteManager::select_all() {
    selected_note_ids_.clear();
    for (Note& note : notes_) {
        note.selected = true;
        selected_note_ids_.insert(note.id);
    }
}

bool NoteManager::is_selected(NoteId id) const {
    return selected_note_ids_.find(id) != selected_note_ids_.end();
}

std::vector<NoteId> NoteManager::selected_ids() const {
    std::vector<NoteId> result;
    result.reserve(selected_note_ids_.size());
    for (NoteId id : selected_note_ids_) {
        result.push_back(id);
    }
    return result;
}

void NoteManager::clear() {
    notes_.clear();
    id_to_index_.clear();
    spatial_index_.clear();
    selected_note_ids_.clear();
    undo_stack_.clear();
    redo_stack_.clear();
}

bool NoteManager::undo() {
    if (undo_stack_.empty()) {
        return false;
    }

    // Save current state to redo stack.
    redo_stack_.push_back(notes_);

    // Restore previous state.
    notes_ = undo_stack_.back();
    undo_stack_.pop_back();

    rebuild_indexes();
    rebuild_selection_from_notes();

    return true;
}

bool NoteManager::redo() {
    if (redo_stack_.empty()) {
        return false;
    }

    // Save current state to undo stack.
    undo_stack_.push_back(notes_);

    // Restore next state.
    notes_ = redo_stack_.back();
    redo_stack_.pop_back();

    rebuild_indexes();
    rebuild_selection_from_notes();

    return true;
}

void NoteManager::rebuild_indexes() {
    id_to_index_.clear();
    spatial_index_.clear();

    for (std::size_t index = 0; index < notes_.size(); ++index) {
        Note& note = notes_[index];
        id_to_index_[note.id] = index;
        spatial_index_[note.key].push_back(index);
    }
}

void NoteManager::rebuild_selection_from_notes() {
    selected_note_ids_.clear();
    for (const Note& note : notes_) {
        if (note.selected) {
            selected_note_ids_.insert(note.id);
        }
    }
}

void NoteManager::push_undo_state() {
    undo_stack_.push_back(notes_);
    if (undo_stack_.size() > max_undo_levels_) {
        undo_stack_.erase(undo_stack_.begin());
    }
    redo_stack_.clear();
}

std::size_t NoteManager::allocate_index_for_new_note() {
    std::size_t index = notes_.size();
    notes_.push_back(Note{});
    return index;
}

NoteId NoteManager::allocate_id() {
    // Simple monotonically increasing ID generator.
    NoteId assigned_id = next_id_;
    ++next_id_;
    return assigned_id;
}

}  // namespace piano_roll

