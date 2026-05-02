#pragma once

struct Viewport {
    float left, top;   // world-space left/top edges
    float right, bottom;   // world-space right/bottom edges
    float screen_l, screen_t; // screen-space (canvas-local) left/top
    float screen_r, screen_b; // screen-space right/bottom
};

// World → screen
inline float vp_to_screen_x(const Viewport& vp, float wx) {
    float t = (wx - vp.left) / (vp.right - vp.left);
    return vp.screen_l + t * (vp.screen_r - vp.screen_l);
}

// Handles y-flip naturally: when top > bottom, higher world_y maps to lower screen_y.
inline float vp_to_screen_y(const Viewport& vp, float wy) {
    float t = (wy - vp.top) / (vp.bottom - vp.top);
    return vp.screen_t + t * (vp.screen_b - vp.screen_t);
}

// Screen → world
inline float vp_to_world_x(const Viewport& vp, float sx) {
    float t = (sx - vp.screen_l) / (vp.screen_r - vp.screen_l);
    return vp.left + t * (vp.right - vp.left);
}

inline float vp_to_world_y(const Viewport& vp, float sy) {
    float t = (sy - vp.screen_t) / (vp.screen_b - vp.screen_t);
    return vp.top + t * (vp.bottom - vp.top);
}

// some getters
inline float vp_get_width(const Viewport& vp){
    return vp.right - vp.left;
}

inline float vp_get_height(const Viewport& vp){
    return vp.bottom - vp.top;
}

// Zoom: pixels per world unit. Positive even for y-flip (top > bottom).
inline float vp_zoom_x(const Viewport& vp) {
    return (vp.screen_r - vp.screen_l) / (vp.right - vp.left);
}

inline float vp_zoom_y(const Viewport& vp) {
    return (vp.screen_b - vp.screen_t) / (vp.top - vp.bottom);
}

/*
    Viewport controllers:
*/
// move viewport to a new place in X, Y
inline void vp_go_to(Viewport& vp, float x, float y){
    float width = vp_get_width(vp);
    float height = vp_get_height(vp);
    vp.left = x;
    vp.top = y;
    vp.right = vp.left + width;
    vp.bottom = vp.top + height;
}

// Zoom x/y by factor while keeping world point wx/wy fixed at its current screen position.
inline void vp_zoom_at_x(Viewport& vp, float wx, float factor) {
    vp.left = wx + (vp.left - wx) / factor;
    vp.right = wx + (vp.right - wx) / factor;
}

inline void vp_zoom_at_y(Viewport& vp, float wy, float factor) {
    vp.top = wy + (vp.top - wy) / factor;
    vp.bottom = wy + (vp.bottom - wy) / factor;
}
