#pragma once
#include <cstdio>

/*   New viewport implementations   */
// this new system while just a teensy bit less efficient, is much more expandable, debuggable and less error prone. Hopefully
struct ViewportAxis {
    float start, end;
};

struct ViewportSpace {
    ViewportAxis x, y;
};

struct Viewport {
    ViewportSpace
        world,  // real space
        screen, // view space
        bounds, // where is the viewport allowed to wander to
        target  // used for bound check. world is bound_check(target) so to speak
    ;
};

/*      Some setters     */
inline void vp_set_space(ViewportSpace& vps, float left,  float top, float right,  float bottom) {
    vps.x.start = left;
    vps.x.end = right;
    vps.y.start = top;
    vps.y.end = bottom;
}

// World → Screen
// hmmm... yeah... this now has a different set of problems...
// how do we translate a specific axis to another within the Viewport ref? We have an order-of-operations issue here.
// Ideally we would like to call vp_to_screen(vp, p) and specify _which_ axis...
// unless we call axis to axis
inline float vp_to_screen_x(const Viewport& vp, float p) {
    float t = (p - vp.world.x.start) / (vp.world.x.end - vp.world.x.start);
    return vp.screen.x.start + t * (vp.screen.x.end - vp.screen.x.start);
}

// if we did axis-to-axis
// thenwe would end up something like this:
inline float vp_map(const ViewportAxis& source, const ViewportAxis& target, float p) {
    float t = (p - source.start) / (source.end - source.start);
    return target.start + t * (target.end - target.start);
}

// this does, however, make mapping harder
// it'd look something like
// float x = vp_map(vp.world.x, vp.screen.x, p);
// which is elegant, but a mouthfull

// then vp_to_screen_x() would look like
inline float vp_to_screen_x(const Viewport& vp, float p) {
    return vp_map(vp.world.x, vp.screen.x, p);
}
// now... vp_to_screen_x(vp, p) has 21 characters
// and vp_map(vp.world.x, vp.screen.x, p); has 35. Is it worth it? IDK! lol
// we could also try vp_map(vp.gbl.x, vp.scr.x, p); -> 30 PFFT. Ok, it _is_ worth it from an LLM economy POV but not humanly

// However scroll now looks like this
inline void vp_scroll_by(ViewportAxis& axis, float delta){
    float span = axis.end - axis.start;
    float factor = () /// yeah.... now im realising the order is wrong
    float new_start = axis.start + delta;
}

// However now bound checks looks like this:
inline float vp_update_world_space(Viewport& vp){
    // hmm... this aint working lol
    // there's too many dependencies...
    // perhaps the ordering is wrong and i shouldn't think about these as axes.
}

// It should be

struct ViewportSpan {
    float start, end;
};

struct ViewportAxis {
    ViewportSpan // name to be tweaked
        world,
        screen,
        target,
        bound
    ;
};

struct Viewport {
    ViewportAxis x, y;
};

// Now mapping looks closer to:

inline float vp_map(ViewportAxis& axis, float p) {
    float t = (p - axis.world.start) / (axis.world.end - axis.world.start);
    return axis.screen.start + t * (axis.screen.end - axis.screen.start);
}

// which is significantly nicer, a map now is just:
Viewport vp;
float p;
float x = vp_map(vp.x, p);

// scroll now looks like:
inline void vp_scroll_by(ViewportAxis& axis, float delta){
    auto& world = axis.world;
    auto& screen = axis.screen;
    auto& bound = axis.bound;

    float span = (world.end - world.start);
    float factor = span / (screen.end - screen.start);
    float new_start = world.start + delta * factor;
    float new_end = new_start + span;

    if(new_end > bound.end) {
        new_end = bound.end;
        new_start = new_end - span;
    }
    else if(new_start < bound.start) {
        new_start = bound.start;
        new_end = new_start + span;
    }

    world.start = new_start;
    world.end = new_end;
}

// Now, bound-cheking could also be performed by the target -> world layer we prepared earlier
// This means we could play the game-engine game and have a behaviour fucntion call to vp (vp_update(), vp_run(), whatever)
// which could keep the VP bounded, processes the collision logic and handles scroll smoothing if applicable. It could also keep track of side properties like width and height

inline void vp_update(ViewportAxis& axis) {
    axis.world.start = std::max(axis.target.start, axis.bound.start);
    axis.world.end = std::min(axis.target.end, axis.bound.end);
    //plus some extra features
}

// now scroll_by becomes
inline void vp_scroll_by(ViewportAxis& axis, float delta){
    auto& target = axis.target;
    auto& screen = axis.screen;
    auto& bound = axis.bound;

    float span = (target.end - target.start);
    float factor = span / (screen.end - screen.start);
    target.start += delta * factor;
    target.end   += delta * factor;

    vp_update(axis);
}

// Now we don't really rigidly crash, but instead the zoom starts to grow as we get more and more trapped into an edge.
// I think i prefer crashing though... 



















/* Older system, for now saved... while we experiment with the newer system */
// DN to _director_: naming is a bit ugly... this could be fixed later
// DN commentary: Gosh, I feel like I'm building a videogame here...

struct Viewport {
    float left, top, right, bottom; // world-space
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
inline void vp_scroll_x_by(Viewport& vp, float delta) {
    // Wait for axis abstraction instead
}

inline void vp_scroll_y_by(Viewport& vp, float delta) {
    float span = vp.world_b - vp.world_t;
    float factor = (vp.world_b - vp.world_t) / (vp.screen_b - vp.screen_t);
    float new_start = vp.world_t  - delta * factor;
    float new_end   = new_start + span;
    fprintf(stdout, "trixie: bounds: %d\n", (int)vp.world_t);
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
    vp.world_t = new_start;
    vp.world_b = new_end;
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
