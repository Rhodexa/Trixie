// piano_roll_space.h
// Persistent state for the Piano Roll editor (mirrors Blender's Space* structs).
// Survives across redraws. Does not own geometry — that lives in ARegion::winrct.

#pragma once

#include "camera.h"
#include "note.h"
#include "wm.h"
#include <optional>

struct SpacePianoRoll {
    Camera camera;
    float  strip_width        = 68.0f;
    bool   camera_initialized = false;

    // Playback observation — written by render.cpp each frame before draw.
    Tick cursor_tick = 0;
    bool is_playing  = false;

    // Pending note preview (written by NOTE_ADD_OT while placing, cleared on release/cancel).
    std::optional<Note>       pending_note;

    // Active modal operator — owned by the wm dispatch loop.
    std::optional<wmOperator> active_modal_op;
};
