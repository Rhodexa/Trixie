#pragma once
#include <cstdio>

// DN to _director_: naming is a bit ugly... this could be fixed later
// DN commentary: Gosh, I feel like I'm building a videogame here...
struct Viewport {
    float left, top, right, bottom; // world-space
    float screen_l, screen_t, screen_r, screen_b; // screen-space
    float bound_l, bound_t, bound_r, bound_b; // future bounds to check against for scrolling. DN: I guess a value of 0 should be considered infinite?
};
/*      Some setters     */
inline void vp_setWorldSpace(Viewport& vp, float left, float top, float right, float bottom) {
    vp.left = left;
    vp.top = top;
    vp.right = right;
    vp.bottom = bottom;
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
    float t = (wx - vp.left) / (vp.right - vp.left);
    return vp.screen_l + t * (vp.screen_r - vp.screen_l);
}

inline float vp_to_screen_y(const Viewport& vp, float wy) {
    // Handles y-flip naturally: when top > bottom, higher world_y maps to lower screen_y.
    float t = (wy - vp.top) / (vp.bottom - vp.top);
    return vp.screen_t + t * (vp.screen_b - vp.screen_t);
}

// World ← Screen
inline float vp_to_world_x(const Viewport& vp, float sx) {
    float t = (sx - vp.screen_l) / (vp.screen_r - vp.screen_l);
    return vp.left + t * (vp.right - vp.left);
}

inline float vp_to_world_y(const Viewport& vp, float sy) {
    float t = (sy - vp.screen_t) / (vp.screen_b - vp.screen_t);
    return vp.top + t * (vp.bottom - vp.top);
}

/*   Some getters    */
inline float vp_get_width(const Viewport& vp){
    return vp.right - vp.left;
}

inline float vp_get_height(const Viewport& vp){
    return vp.bottom - vp.top;
}

inline float vp_zoom_x(const Viewport& vp) {
    return (vp.screen_r - vp.screen_l) / (vp.right - vp.left);
}

inline float vp_zoom_y(const Viewport& vp) {
    return (vp.screen_b - vp.screen_t) / (vp.top - vp.bottom);
}

/*    Viewport controllers:     */
// move viewport to a new place in X, Y
inline void vp_go_to(Viewport& vp, float x, float y){
    float width = vp_get_width(vp);
    float height = vp_get_height(vp);
    vp.left = x;
    vp.top = y;
    vp.right = vp.left + width;
    vp.bottom = vp.top + height;
}

// Scroll by 'n' pixels in screen space (takes viewport "zoom" into account)
// DN: This whole horizontal/vertical duplicates are driving me crazy. We should've simplified that by making a viewport a collection of axes
// Oh gosh this thing would sooo benefit from more abstractions. This is currently highly error-prone
// right now it assumes bound_l < bound_r, else it just won't bound check. This means viewports with negative determinants will skip bound checks with the current implementation
// On a different note, perhaps this could use layers: target values vs bounded values. That way, if you reach a bound, it will start stretching and zooming, but returning inside the "safe zone" will restore zoom
// neat little viewport detail i guess...elastic viewports. Right now they will be solid
// This is turning into a proper game engine, huh? complete with hit boxes lol
inline void vp_scroll_x_by(Viewport& vp, float delta) {
    // Wait for axis abstraction instead
}

inline void vp_scroll_y_by(Viewport& vp, float delta) {
    float span = vp.bottom - vp.top;
    float factor = (vp.bottom - vp.top) / (vp.screen_b - vp.screen_t);
    float new_start = vp.top  - delta * factor;
    float new_end   = new_start + span;
    fprintf(stdout, "trixie: bounds: %d\n", (int)vp.top);
    if(vp.bound_t < vp.bound_b) {
        if     (new_start < vp.bound_t) {
            new_start = vp.bound_t;
            new_end   = new_start + span;
            fprintf(stdout, "trixie: viewport: bounds! negative — "); // you can tell its problematic when you need to add debug code lol
        }
        else if(new_end   > vp.bound_b) {
            new_end   = vp.bound_b;
            new_start = new_end   - span;
            fprintf(stdout, "trixie: viewport: bounds! positive — ");
        }
    }
    vp.top = new_start;
    vp.bottom = new_end;
}

inline void vp_scroll_by(Viewport& vp, float dx, float dy) {
    vp_scroll_x_by(vp, dx);
    vp_scroll_y_by(vp, dy);
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
