// note.h
// A single MIDI note. The atom of all musical data in Trixie.
// Timing is in ticks — integers, not floats. See song.h for ppq.
// This struct knows nothing about pixels, screens, or rendering.

#pragma once

#include <cstdint>

using Tick = int64_t;

// Round value to the nearest multiple of step.
// step <= 0 → value returned unchanged (no snap).
inline Tick snap_to_nearest(Tick value, Tick step) {
    if (step <= 0) return value;
    Tick half = step / 2;
    if (value >= 0)
        return ((value + half) / step) * step;
    else
        return ((value - half) / step) * step;
}

struct Note {
    Tick start;    // tick position from the beginning of the pattern
    Tick duration; // length in ticks
    int  pitch;    // MIDI note number, 0–127 (middle C = 60)
    int  velocity; // 0–127
    int  channel;  // MIDI channel, 0–15
};
