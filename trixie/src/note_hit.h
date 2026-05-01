// note_hit.h
// Hit-test result types for the piano roll canvas.

#pragma once

enum class NotePart { HEAD, BODY, TAIL };

struct NoteHit {
    int      track;
    int      note_idx;
    NotePart part;
};
