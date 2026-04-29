// note_commands.h
// Journal commands for placing and removing notes on the piano roll.

#pragma once

#include "journal.h"
#include "song.h"

struct AddNoteCommand : Command {
    Song& song;
    int   track;
    Note  note;
    int   inserted_idx = -1; // captured in execute(), used by undo() to erase the right slot

    AddNoteCommand(Song& s, int t, const Note& n, std::string desc)
        : song(s), track(t), note(n) { description = std::move(desc); }

    void execute() override {
        auto& notes  = song.tracks[track].notes;
        inserted_idx = (int)notes.size();
        notes.push_back(note);
    }

    void undo() override {
        auto& notes = song.tracks[track].notes;
        if (inserted_idx >= 0 && inserted_idx < (int)notes.size())
            notes.erase(notes.begin() + inserted_idx);
    }
};

struct RemoveNoteCommand : Command {
    Song& song;
    int   track;
    int   note_idx;
    Note  removed_note; // captured in execute() so undo() can restore exact note

    RemoveNoteCommand(Song& s, int t, int idx, std::string desc)
        : song(s), track(t), note_idx(idx) { description = std::move(desc); }

    void execute() override {
        auto& notes  = song.tracks[track].notes;
        removed_note = notes[note_idx];
        notes.erase(notes.begin() + note_idx);
    }

    void undo() override {
        auto& notes = song.tracks[track].notes;
        notes.insert(notes.begin() + note_idx, removed_note);
    }
};
