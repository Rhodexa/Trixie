// box.h
// A Box is a rectangular region of screen space owned by one widget.
// Layout is computed by splitting boxes top-down every frame — no heap,
// no dirty flags, no runtime tree. Just arithmetic.

#pragma once

struct Box {
    float x, y, w, h;
};

// Each split carves a slice off `b` in-place and returns the carved piece.
// Caller holds the remainder in `b` after the call.

inline Box box_split_top(Box* b, float size) {
    if (size > b->h) size = b->h;
    Box out = { b->x, b->y, b->w, size };
    b->y += size;
    b->h -= size;
    return out;
}

inline Box box_split_bottom(Box* b, float size) {
    if (size > b->h) size = b->h;
    b->h -= size;
    return { b->x, b->y + b->h, b->w, size };
}

inline Box box_split_left(Box* b, float size) {
    if (size > b->w) size = b->w;
    Box out = { b->x, b->y, size, b->h };
    b->x += size;
    b->w -= size;
    return out;
}

inline Box box_split_right(Box* b, float size) {
    if (size > b->w) size = b->w;
    b->w -= size;
    return { b->x + b->w, b->y, size, b->h };
}

inline bool box_contains(Box b, float mx, float my) {
    return mx >= b.x && mx < b.x + b.w &&
           my >= b.y && my < b.y + b.h;
}
