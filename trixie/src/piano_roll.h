// piano_roll.h
// Draws the piano roll — pitch lanes, grid lines, and notes — using NanoVG.
// Reads Song and Camera but never writes to them.

#pragma once

#include "song.h"
#include "panel.h"

#include <optional>

struct NVGcontext; // forward declare to avoid pulling nanovg.h into every file

void draw_piano_roll(NVGcontext* nvg, const Song& song, const Panel& panel);

// Draws a single note as a translucent white ghost overlay (in-flight drag preview).
void piano_roll_draw_ghost(NVGcontext* nvg, const Song& song, const Panel& panel, const Note& note);

// Draws the playback cursor line at the given tick position.
void piano_roll_draw_cursor(NVGcontext* nvg, const Song& song, const Panel& panel, Tick cursor_tick);

// --- hit testing ---

// Which part of a note was hit. Determines drag behaviour.
enum class NotePart { HEAD, BODY, TAIL };

struct NoteHit { int track; int note_idx; NotePart part; };

// Returns the first note whose pixel rect (plus handle zones) contains (mx, my).
std::optional<NoteHit> piano_roll_hit_test(const Song& song, const Panel& panel, float mx, float my);

// Converts (mx, my) to a snapped Note ready for insertion.
// Returns nullopt if the click is on the piano strip or maps outside 0–127.
std::optional<Note> piano_roll_make_note(const Song& song, const Panel& panel,
                                         float mx, float my,
                                         Tick snap_ticks, Tick dur_ticks, int velocity);

// --- coordinate helpers (used by drag logic in the render thread) ---

// Converts pixel x to a tick. Pass snap_ticks=0 for raw (unsnapped) result.
Tick piano_roll_x_to_tick(const Song& song, const Panel& panel, float mx, Tick snap_ticks);

// Converts pixel y to a pitch (0–127, clamped).
int  piano_roll_y_to_pitch(const Panel& panel, float my);
