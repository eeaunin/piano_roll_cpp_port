#pragma once

#include "piano_roll/cc_lane.hpp"
#include "piano_roll/note_manager.hpp"
#include "piano_roll/types.hpp"

#include <iosfwd>
#include <vector>

namespace piano_roll {

// Serialize notes and CC lanes to a simple text format:
//
//   PPR1
//   N <tick> <duration> <key> <velocity> <channel>
//   C <cc_number> <tick> <value>
//
// One event per line. Lines starting with any other character are ignored.
//
// IDs are not preserved; NoteManager will assign new IDs when deserializing.
void serialize_notes_and_cc(const NoteManager& notes,
                            const std::vector<ControlLane>& lanes,
                            std::ostream& out);

// Deserialize notes and CC lanes from the text format described above.
// Existing notes and lanes in the destination containers are cleared.
// Unknown line types are ignored.
void deserialize_notes_and_cc(NoteManager& notes,
                              std::vector<ControlLane>& lanes,
                              std::istream& in);

}  // namespace piano_roll

