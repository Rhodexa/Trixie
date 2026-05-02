// toolbar.h
// Top toolbar strip: transport position display and future controls.

#pragma once

#include "box.h"
#include "song.h"
#include "note.h"
#include "theme.h"

struct NVGcontext;

// Draws the toolbar into box b. cursor_tick is the current playback position.
void draw_toolbar(NVGcontext* nvg, Box b, const Song& song, Tick cursor_tick, bool is_playing);
