// midi_loader.h
// Loads a MIDI file from disk and returns a populated Song.
// Handles Type 0 and Type 1 MIDI files.
// Returns an empty Song on failure (check song.tracks.empty()).

#pragma once

#include "song.h"

Song load_midi(const char* path);
