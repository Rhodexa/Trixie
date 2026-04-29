// note.h
// A single MIDI note. The atom of all musical data in Trixie.
// Timing is in ticks — integers, not floats. See song.h for ppq.
// This struct knows nothing about pixels, screens, or rendering.

#pragma once

#include <cstdint>

using Tick = int64_t;

struct Note {
    Tick start;    // tick position from the beginning of the pattern
    Tick duration; // length in ticks
    int  pitch;    // MIDI note number, 0–127 (middle C = 60)
    int  velocity; // 0–127
    int  channel;  // MIDI channel, 0–15
};
