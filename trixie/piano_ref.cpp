// Normalized Y offsets and heights for each key within one octave, top = C (index 11) to B (index 0).
// Positions are in [0, 1] relative to octave height.
// Black key positions reflect real piano geometry — they are NOT evenly spaced.
struct KeyDef {
    float y;      // top of key within octave, normalized [0,1]
    float h;      // height of key, normalized [0,1]
    bool  black;
};

static constexpr KeyDef KEY_DEFS[12] = {
    { 0.000f, 0.110f, false }, // C
    { 0.065f, 0.075f, true  }, // C#
    { 0.110f, 0.110f, false }, // D
    { 0.195f, 0.075f, true  }, // D#
    { 0.220f, 0.110f, false }, // E
    { 0.333f, 0.110f, false }, // F  <-- E/F boundary
    { 0.390f, 0.075f, true  }, // F#
    { 0.443f, 0.110f, false }, // G
    { 0.527f, 0.075f, true  }, // G#
    { 0.557f, 0.110f, false }, // A
    { 0.650f, 0.075f, true  }, // A#
    { 0.667f, 0.110f, false }, // B
};

// Draws one octave of piano keys, top-down, origin at top-left of the octave's strip region.
// width  = PIANO_STRIP_WIDTH (pixels)
// height = octave_height in pixels (lane_height * 12)
// on_keys = bitmask, bit 0 = C, bit 11 = B
static void draw_octave(NVGcontext* nvg, float width, float height, uint16_t on_keys = 0) {
    // White keys first (background layer)
    for (int i = 0; i < 12; i++) {
        const KeyDef& k = KEY_DEFS[i];
        if (k.black) continue;

        float y = k.y * height;
        float h = k.h * height;

        nvgBeginPath(nvg);
        nvgRect(nvg, 0.0f, y, width, h);

        if (on_keys & (1 << i))
            nvgFillColor(nvg, nvgRGBf(0.643f, 0.990f, 0.938f)); // held: blueish tint
        else if (i == 0)
            nvgFillColor(nvg, nvgRGBf(0.93f, 0.92f, 0.90f));    // C: slightly darker ivory
        else
            nvgFillColor(nvg, nvgRGBf(0.96f, 0.95f, 0.93f));    // ivory

        nvgFill(nvg);
    }

    // Black keys on top
    for (int i = 0; i < 12; i++) {
        const KeyDef& k = KEY_DEFS[i];
        if (!k.black) continue;

        float y = k.y * height;
        float h = k.h * height;

        nvgBeginPath(nvg);
        nvgRect(nvg, 0.0f, y, width * 0.63f, h);

        if (on_keys & (1 << i))
            nvgFillColor(nvg, nvgRGBf(0.0415f, 0.830f, 0.712f)); // held: blueish tint
        else
            nvgFillColor(nvg, nvgRGBf(0.10f, 0.09f, 0.09f));     // ebony

        nvgFill(nvg);
    }

    // E/F separator line (subtle, marks the octave boundary mid-point)
    float ef_y = KEY_DEFS[5].y * height; // F is index 5
    nvgBeginPath(nvg);
    nvgMoveTo(nvg, 0.0f,  ef_y);
    nvgLineTo(nvg, width, ef_y);
    nvgStrokeColor(nvg, nvgRGBAf(0.0f, 0.0f, 0.0f, 0.25f));
    nvgStrokeWidth(nvg, 1.0f);
    nvgStroke(nvg);
}