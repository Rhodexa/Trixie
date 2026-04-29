// panel.h
// A Panel is a rectangular region of the screen with its own Camera.
// Every interactive surface in Trixie is a Panel — the piano roll canvas,
// the piano strip, the mixer rack, the pattern editor, etc.
// The input system routes events to whichever panel the mouse is currently in.

#pragma once

#include "camera.h"

struct Panel {
    float  x = 0, y = 0;  // top-left corner in window pixels
    float  w = 0, h = 0;  // size in window pixels
    Camera camera;
};

// Returns true if the point (mx, my) is inside the panel's screen rect.
inline bool panel_contains(const Panel& panel, float mx, float my) {
    return mx >= panel.x && mx < panel.x + panel.w
        && my >= panel.y && my < panel.y + panel.h;
}
