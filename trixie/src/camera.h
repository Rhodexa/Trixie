// camera.h
// The piano roll viewport state.
// Converts between tick/pitch space (musical) and pixel space (screen).
// Lives in the render thread — not shared with audio or the journal.

#pragma once

struct Camera {
    float scroll_x        =    0.0f; // pixels scrolled from the left edge
    float scroll_y        =    0.0f; // pixels scrolled from the top edge
    float pixels_per_beat =   80.0f; // horizontal zoom
    float lane_height     =   20.0f; // height of one semitone row, in pixels
};
