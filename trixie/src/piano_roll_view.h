// piano_roll_view.h
// All state owned by a PianoRoll widget instance: layout box, camera, and interaction.

#pragma once

#include "box.h"
#include "camera.h"
#include "note_hit.h"
#include "song.h"
#include <optional>

struct PianoRollView {
    Box    box;
    Camera camera;
    float  strip_width        = 68.0f;

    bool   camera_initialized = false;
    bool   mmb_held           = false;
    bool   rmb_held           = false;

    bool                lmb_placing  = false;
    std::optional<Note> pending_note;

    bool     lmb_dragging_note = false;
    int      drag_track        = -1;
    int      drag_note_idx     = -1;
    NotePart drag_part         = NotePart::BODY;
    Note     drag_original     = {};
    Tick     drag_tick_offset  = 0;
};
