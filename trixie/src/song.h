// song.h
// A Song is a ppq value plus a list of tracks.
// A Track is a name plus a list of notes.
// No rendering, no playback — pure data.

#pragma once

#include "note.h"

#include <string>
#include <vector>

struct Track {
    std::string       name;
    std::vector<Note> notes;
};

struct Song {
    int                ppq = 480; // ticks per quarter note, taken from the MIDI file
    std::vector<Track> tracks;
};
