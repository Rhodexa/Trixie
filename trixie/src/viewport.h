#pragma once
#include <cstdio>

// DN: I just did a lot of sloppy global find-and-replace, i hope i didn't destroy vendored libraries lol

struct Viewport {
    float world_l, world_t, world_r, world_b; // world-space
    float screen_l, screen_t, screen_r, screen_b; // screen-space
    float bound_l, bound_t, bound_r, bound_b; // future bounds to check against for scrolling. DN: I guess a value of 0 should be considered infinite?
};

/*      Some setters     */
inline void vp_setWorldSpace(Viewport& vp, float left, float top, float right, float bottom) {
    vp.world_l = left;
    vp.world_t = top;
    vp.world_r = right;
    vp.world_b = bottom;
}

inline void vp_setScreenSpace(Viewport& vp, float left, float top, float right, float bottom) {
    vp.screen_l = left;
    vp.screen_t = top;
    vp.screen_r = right;
    vp.screen_b = bottom;
}

inline void vp_setBoundsSpace(Viewport& vp, float left, float top, float right, float bottom) {
    vp.bound_l = left;
    vp.bound_t = top;
    vp.bound_r = right;
    vp.bound_b = bottom;
}

/*    Transformers:     */
// World → Screen
inline float vp_to_screen_x(const Viewport& vp, float wx) {
    float t = (wx - vp.world_l) / (vp.world_r - vp.world_l);
    return vp.screen_l + t * (vp.screen_r - vp.screen_l);
}

inline float vp_to_screen_y(const Viewport& vp, float wy) {
    // Handles y-flip naturally: when top > bottom, higher world_y maps to lower screen_y.
    float t = (wy - vp.world_t) / (vp.world_b - vp.world_t);
    return vp.screen_t + t * (vp.screen_b - vp.screen_t);
}

// World ← Screen
inline float vp_to_world_x(const Viewport& vp, float sx) {
    float t = (sx - vp.screen_l) / (vp.screen_r - vp.screen_l);
    return vp.world_l + t * (vp.world_r - vp.world_l);
}

inline float vp_to_world_y(const Viewport& vp, float sy) {
    float t = (sy - vp.screen_t) / (vp.screen_b - vp.screen_t);
    return vp.world_t + t * (vp.world_b - vp.world_t);
}

/*   Some getters    */
inline float vp_get_width(const Viewport& vp){
    return vp.world_r - vp.world_l;
}

inline float vp_get_height(const Viewport& vp){
    return vp.world_b - vp.world_t;
}

inline float vp_zoom_x(const Viewport& vp) {
    return (vp.screen_r - vp.screen_l) / (vp.world_r - vp.world_l);
}

inline float vp_zoom_y(const Viewport& vp) {
    return (vp.screen_b - vp.screen_t) / (vp.world_t - vp.world_b);
}

/*    Viewport controllers:     */
// move viewport to a new place in X, Y
inline void vp_go_to(Viewport& vp, float x, float y){
    float width = vp_get_width(vp);
    float height = vp_get_height(vp);
    vp.world_l = x;
    vp.world_t = y;
    vp.world_r = vp.world_l + width;
    vp.world_b = vp.world_t + height;
}

// Scroll by 'n' pixels in screen space (takes viewport "zoom" into account)
// DN: This whole horizontal/vertical duplicates are driving me crazy. We should've simplified that by making a viewport a collection of axes
// Oh gosh this thing would sooo benefit from more abstractions. This is currently highly error-prone
// right now it assumes bound_l < bound_r, else it just won't bound check. This means viewports with negative determinants will skip bound checks with the current implementation
// On a different note, perhaps this could use layers: target values vs bounded values. That way, if you reach a bound, it will start stretching and zooming, but returning inside the "safe zone" will restore zoom
// neat little viewport detail i guess...elastic viewports. Right now they will be solid
// This is turning into a proper game engine, huh? complete with hit boxes lol

// look at this hack work... ugh... we must fix this soon
// This is what happens when you come from embedded shared memory single core enviroments. I bet desktop devs would totally cringe
inline void vp_scroll_axis_by(float& s_start, float& s_end, float& d_start, float& d_end, float& b_start, float& b_end, float delta) {
    float span = s_end - s_start;
    float factor = (s_end - s_start) / (d_end - d_start);
    float new_start = s_start  - delta * factor;
    float new_end   = new_start + span;
    if(b_start != b_end) {
        if (new_start < b_start) {
            new_start = b_start;
            new_end   = new_start + span;
        }
        else if (new_end > b_end) {
            new_end   = b_end;
            new_start = new_end   - span;
        }
    }
    s_start = new_start;
    s_end = new_end;
}

inline void vp_scroll_x_by(Viewport& vp, float delta) {
    vp_scroll_axis_by(vp.world_l, vp.world_r, vp.screen_l, vp.screen_r, vp.bound_l, vp.bound_r, delta);
}

inline void vp_scroll_y_by(Viewport& vp, float delta) {
    vp_scroll_axis_by(vp.world_t, vp.world_b, vp.screen_t, vp.screen_b, vp.bound_t, vp.bound_b, delta);
}

inline void vp_scroll_by(Viewport& vp, float dx, float dy) {
    vp_scroll_x_by(vp, dx);
    vp_scroll_y_by(vp, dy);
}


// Zoom x/y by factor while keeping world point wx/wy fixed at its current screen position.
inline void vp_zoom_at_x(Viewport& vp, float wx, float factor) {
    vp.world_l = wx + (vp.world_l - wx) / factor;
    vp.world_r = wx + (vp.world_r - wx) / factor;
}

inline void vp_zoom_at_y(Viewport& vp, float wy, float factor) {
    vp.world_t = wy + (vp.world_t - wy) / factor;
    vp.world_b = wy + (vp.world_b - wy) / factor;
    
}

inline void vp_update(Viewport& vp) {
    float zoom_x = vp_zoom_x(vp);
    float zoom_y = vp_zoom_y(vp);
    vp.screen_r  = wbox.w;
    vp.screen_b  = wbox.h;
    vp.world_r   = vp.world_l + wbox.w / zoom_x;
    vp.world_b   = vp.world_t + wbox.h / zoom_y;
}