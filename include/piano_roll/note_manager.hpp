#pragma once

#include "piano_roll/note.hpp"

#include <cstddef>
#include <optional>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace piano_roll {

// Central manager for notes, providing CRUD operations,
// simple spatial queries, and selection tracking.
class NoteManager {
public:
    NoteManager() = default;

    // Access to the underlying note collection.
    const std::vector<Note>& notes() const noexcept { return notes_; }
    std::vector<Note>& notes() noexcept { return notes_; }

    // Create a new note and add it to the collection.
    // Returns the assigned NoteId, or 0 if the note would overlap and
    // overlaps are not allowed.
    NoteId create_note(Tick tick,
                       Duration duration,
                       MidiKey key,
                       Velocity velocity = 100,
                       Channel channel = 0,
                       bool selected = false,
                       bool record_undo = true,
                       bool allow_overlap = false);

    // Remove a note by ID. Returns true if a note was removed.
    bool remove_note(NoteId id, bool record_undo = true);

    // Move a note by the specified deltas.
    // Returns true if the move was applied.
    bool move_note(NoteId id,
                   Tick delta_tick,
                   int key_delta,
                   bool record_undo = true,
                   bool allow_overlap = false);

    // Resize a note to a new duration.
    // Returns true if the resize was applied.
    bool resize_note(NoteId id,
                     Duration new_duration,
                     bool record_undo = true,
                     bool allow_overlap = false);

    // Check if a note would overlap any existing note on the same key.
    bool would_overlap(const Note& probe,
                       std::optional<NoteId> exclude_id = std::nullopt) const;

    // Query helpers.
    Note* find_by_id(NoteId id) noexcept;
    const Note* find_by_id(NoteId id) const noexcept;

    Note* note_at(Tick tick, MidiKey key) noexcept;
    const Note* note_at(Tick tick, MidiKey key) const noexcept;

    std::vector<Note*> notes_in_range(Tick start_tick,
                                      Tick end_tick,
                                      MidiKey min_key,
                                      MidiKey max_key) noexcept;
    std::vector<const Note*> notes_in_range(Tick start_tick,
                                            Tick end_tick,
                                            MidiKey min_key,
                                            MidiKey max_key) const noexcept;

    // Selection operations (by NoteId).
    void select(NoteId id, bool add_to_selection = false);
    void deselect(NoteId id);
    void clear_selection();
    void select_all();
    bool is_selected(NoteId id) const;
    std::vector<NoteId> selected_ids() const;

    // Clear all notes and state.
    void clear();

    // Undo / redo support (snapshot-based).
    void set_max_undo_levels(std::size_t levels) { max_undo_levels_ = levels; }
    bool undo();
    bool redo();

    // Explicitly capture the current note/selection state for undo. This is
    // useful for grouping multi-step edits (e.g. drags/resizes) into a single
    // undo step when the caller wants to drive edits without per-call
    // record_undo flags.
    void snapshot_for_undo();

private:
    std::vector<Note> notes_;
    std::unordered_map<NoteId, std::size_t> id_to_index_;
    std::unordered_map<MidiKey, std::vector<std::size_t>> spatial_index_;
    std::unordered_set<NoteId> selected_note_ids_;

    std::vector<std::vector<Note>> undo_stack_;
    std::vector<std::vector<Note>> redo_stack_;
    std::size_t max_undo_levels_{100};
    NoteId next_id_{1};

    void rebuild_indexes();
    void rebuild_selection_from_notes();
    void push_undo_state();

    std::size_t allocate_index_for_new_note();
    NoteId allocate_id();
};

}  // namespace piano_roll
