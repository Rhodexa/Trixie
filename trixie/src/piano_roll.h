// piano_roll.h
// Piano Roll editor: SpaceType registration, layout, draw callbacks, and coordinate utilities.
// Naming mirrors Blender: piano_roll_<region>_draw / handle_event.

#pragma once

#include "editor.h"
#include "note_hit.h"
#include "piano_roll_space.h"
#include "song.h"
#include <optional>
#include "theme.h"

struct NVGcontext;

// --- region sizes (mirrors Blender's HEADERY etc.) ---
constexpr float PIANO_ROLL_HEADER_H    = 48.0f;
constexpr float PIANO_ROLL_TIMERULER_H = 24.0f;
constexpr float PIANO_ROLL_UI_H        = 80.0f;
constexpr float PIANO_ROLL_SCROLLBAR_W = 12.0f;
constexpr int   PIANO_ROLL_REGION_COUNT = 6;

// --- SpaceType registration ---
// Returns the static descriptor for the Piano Roll editor. Call once at startup.
SpaceType* piano_roll_space_type();

// --- layout ---
// Fills regions[PIANO_ROLL_REGION_COUNT] with winrct boxes carved from `screen`.
// Indexed by (int)RegionType: 0=Header, 1=TimeRuler, 2=Channels, 3=Window, 4=UI, 5=Scrollbar.
void piano_roll_compute_layout(const SpacePianoRoll& space, Box screen, ARegion regions[PIANO_ROLL_REGION_COUNT]);

// --- overlay draws (called from render.cpp after the region draw loop) ---
void piano_roll_draw_ghost(NVGcontext*, ARegion& window, const SpacePianoRoll&, const Song&, const Note&);                            
void piano_roll_draw_playback_cursor(NVGcontext*, ARegion& window, const SpacePianoRoll&, const Song&, Tick cursor_tick);
void piano_roll_draw_canvas_crosshair(NVGcontext*, ARegion& window, const SpacePianoRoll&);

// --- coordinate utilities (canvas-local coords; cx/cy relative to Window region origin) ---
std::optional<NoteHit> piano_roll_hit_test(const Song&, const SpacePianoRoll&, float cx, float cy);
std::optional<Note>    piano_roll_make_note(const Song&, const SpacePianoRoll&, float cx, float cy, Tick snap_ticks, Tick dur_ticks, int velocity);
Tick piano_roll_x_to_tick(const Song&, const SpacePianoRoll&, float cx, Tick snap_ticks);
int  piano_roll_y_to_pitch(const SpacePianoRoll&, float cy);
