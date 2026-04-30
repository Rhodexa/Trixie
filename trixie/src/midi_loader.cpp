// midi_loader.cpp
// Reads a MIDI file using the midifile library and converts it to our Song format.
// Preserves the file's original ppq so tick values stay exact.

#include "midi_loader.h"
#include "MidiFile.h"

#include <cstdio>

Song load_midi(const char* path) {
    smf::MidiFile mf;
    if (!mf.read(path)) {
        fprintf(stderr, "trixie: could not read MIDI file: %s\n", path);
        return {};
    }

    mf.doTimeAnalysis(); // converts delta-times to absolute tick positions
    mf.linkNotePairs();  // connects each note-on to its corresponding note-off

    Song song;
    song.ppq = mf.getTPQ();

    // Extract tempo from the first tempo meta-event found (any track).
    for (int t = 0; t < mf.getTrackCount(); t++) {
        for (int e = 0; e < mf[t].getSize(); e++) {
            smf::MidiEvent& ev = mf[t][e];
            if (ev.isTempo()) {
                int us = ev.getTempoMicroseconds();
                if (us > 0) song.bpm = 60'000'000.0f / (float)us;
                goto tempo_done; // first one wins; tempo maps are future work
            }
        }
    }
    tempo_done:

    for (int t = 0; t < mf.getTrackCount(); t++) {
        Track track;

        for (int e = 0; e < mf[t].getSize(); e++) {
            smf::MidiEvent& ev = mf[t][e];

            if (!ev.isNoteOn()) continue; // isNoteOn() returns false for vel=0

            Note note;
            note.start    = ev.tick;
            note.pitch    = ev.getKeyNumber();
            note.velocity = ev.getVelocity();
            note.channel  = ev.getChannel();

            // Use the linked note-off tick for duration; fall back to a 16th note.
            if (ev.isLinked()) {
                note.duration = ev.getLinkedEvent()->tick - ev.tick;
            } else {
                note.duration = song.ppq / 4;
            }

            if (note.duration <= 0) note.duration = song.ppq / 4;

            track.notes.push_back(note);
        }

        if (!track.notes.empty()) {
            song.tracks.push_back(std::move(track));
        }
    }

    fprintf(stdout, "trixie: loaded '%s'  ppq=%d  tracks=%d\n",
            path, song.ppq, (int)song.tracks.size());

    return song;
}
