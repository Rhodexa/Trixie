// piano_roll_space.h
// Persistent state for the Piano Roll editor (mirrors Blender's Space* structs).
// Survives across redraws. Does not own geometry — that lives in ARegion::winrct.

#pragma once

#include "camera.h"
#include "note.h"
#include "note_hit.h"
#include <optional>

struct SpacePianoRoll {
    Camera camera;
    float  strip_width        = 68.0f;
    bool   camera_initialized = false;

    // Playback observation — written by render.cpp each frame before draw.
    Tick cursor_tick = 0;
    bool is_playing  = false;

    // Interaction state
    bool                mmb_held          = false;
    bool                rmb_held          = false;
    bool                lmb_placing       = false;
    std::optional<Note> pending_note;

    bool     lmb_dragging_note = false;
    int      drag_track        = -1;
    int      drag_note_idx     = -1;
    NotePart drag_part         = NotePart::BODY;
    Note     drag_original     = {};
    Tick     drag_tick_offset  = 0;
};
