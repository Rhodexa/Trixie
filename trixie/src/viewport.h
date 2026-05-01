// Replacing current camera system with this new Viewport idea. This should handle visibility more elegantly
// This is a quick-draft concept, and requires proper Cplusplusification. 

struct Viewport {
    float world_x, world_y;
    float world_w, world_h;
    float screen_x, screen_y;
    float screen_w, screen_h;
};

// transform world-space coordinates to screen-space coordinates. Theses put graphics in the right places on the screen
float vp_to_screen_x(const Viewport& vp, float wx) {
    float t = (wx - vp.world_x) / vp.world_w;
    return vp.screen_x + t * vp.screen_w;
}

float vp_to_screen_y(const Viewport& vp, float wy) {
    float t = (wy - vp.world_y) / vp.world_h;
    return vp.screen_y + t * vp.screen_h;
}

// These can take the screen space coordinates of the viewport itself and transform the coordinates back into world-space for things like cursor hits.
// Note that theses are _viewport-space_ and thus cursor location still needs to take nesting into account, This is interesting because it means anythin nested views should include a viewort or a least a viewport translator.
float vp_to_world_x(const Viewport& vp, float sx) {
    float t = (sx - vp.screen_x) / vp.screen_w;
    return vp.world_x + t * vp.world_w;
}

float vp_to_world_y(const Viewport& vp, float sy) {
    float t = (sy - vp.screen_y) / vp.screen_h;
    return vp.world_y + t * vp.world_h;
}


