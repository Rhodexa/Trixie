// piano_roll.h
// Piano roll widget: lane backgrounds, grid lines, notes, piano strip.
// All public functions work in canvas-local coordinates (strip already excluded).

#pragma once

#include "song.h"
#include "piano_roll_view.h"
#include <optional>

struct NVGcontext;

void draw_piano_roll(NVGcontext* nvg, const Song& song, const PianoRollView& view);

void piano_roll_draw_ghost(NVGcontext* nvg, const Song& song,
                           const PianoRollView& view, const Note& note);

void piano_roll_draw_cursor(NVGcontext* nvg, const Song& song,
                            const PianoRollView& view, Tick cursor_tick);

// --- hit testing and coordinate inversion ---
// cx, cy are canvas-local (origin at strip right edge, view top-left).

std::optional<NoteHit> piano_roll_hit_test(const Song& song,
                                           const PianoRollView& view,
                                           float cx, float cy);

std::optional<Note> piano_roll_make_note(const Song& song,
                                         const PianoRollView& view,
                                         float cx, float cy,
                                         Tick snap_ticks, Tick dur_ticks, int velocity);

Tick piano_roll_x_to_tick(const Song& song, const PianoRollView& view,
                           float cx, Tick snap_ticks);

int  piano_roll_y_to_pitch(const PianoRollView& view, float cy);
