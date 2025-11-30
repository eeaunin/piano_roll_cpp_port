#pragma once

#include <cstdint>

namespace piano_roll {

// Basic type aliases used across the piano roll core.
using Tick = std::int64_t;
using Duration = std::int64_t;
using MidiKey = int;
using Velocity = int;
using Channel = int;

// Internal identifier for notes managed by NoteManager.
// 0 is reserved as an invalid ID.
using NoteId = std::uint64_t;

}  // namespace piano_roll

