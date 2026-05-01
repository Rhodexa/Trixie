// camera.h
// A 2D viewport transform: scroll and zoom. Domain-agnostic.
// World-to-screen: screen_pos = world_pos * zoom - scroll
// At zoom = 1.0, one world unit = one pixel.

#pragma once

struct Camera {
    float scroll_x =  0.0f;
    float scroll_y =  0.0f;
    float zoom_x   = 20.0f; // pixels per world unit (x-axis)
    float zoom_y   = 20.0f; // pixels per world unit (y-axis)
};
