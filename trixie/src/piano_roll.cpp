// piano_roll.cpp
// Renders the piano roll: lane backgrounds, grid lines, notes, piano strip.
// All internal draw helpers work in their own local coordinate space.
// draw_piano_roll sets up sub-boxes (strip / canvas) and applies scissor+translate
// so each helper is completely unaware of where it sits on screen.

#include "piano_roll.h"
#include "nanovg.h"

#include <cmath>
#include <algorithm>
#include <cstdint>

// true = note is not in scale (draw darker lane)
static bool SCALE_LUT[12] = {
    false, true,  false, true,  false,
    false, true,  false, true,  false, true, false
};

static constexpr float TRACK_COLORS[][4] = {
    { 1.000f, 0.839f, 0.584f, 0.92f }, // warm gold
    { 0.671f, 0.800f, 0.620f, 0.92f }, // sage green
    { 0.608f, 0.800f, 0.882f, 0.92f }, // muted sky
    { 0.867f, 0.659f, 0.482f, 0.92f }, // muted terracotta
    { 0.780f, 0.722f, 0.878f, 0.92f }, // soft lavender
};
static constexpr int TRACK_COLOR_COUNT = (int)(sizeof(TRACK_COLORS) / sizeof(TRACK_COLORS[0]));

// --- coordinate helpers (canvas-local) ---
// Camera world units: x = beats, y = semitones.
// screen = world * zoom - scroll

static float beat_to_x(float beat, const Camera& cam) {
    return beat * cam.zoom_x - cam.scroll_x;
}

static float pitch_to_y(int pitch, const Camera& cam) {
    return (127 - pitch) * cam.zoom_y - cam.scroll_y;
}

// --- canvas draw passes ---
// All helpers receive canvas dimensions (no strip offset) and draw at local origin.

static void draw_pitch_lanes(NVGcontext* nvg, const Camera& cam,
                              int canvas_w, int canvas_h) {
    int first_pitch = 127 - (int)floorf(cam.scroll_y / cam.zoom_y);
    int last_pitch  = 127 - (int)floorf((cam.scroll_y + (float)canvas_h) / cam.zoom_y) - 1;
    first_pitch = std::min(first_pitch, 127);
    last_pitch  = std::max(last_pitch,  0);

    for (int pitch = last_pitch; pitch <= first_pitch; pitch++) {
        float y = pitch_to_y(pitch, cam);
        nvgBeginPath(nvg);
        nvgRect(nvg, 0.0f, y, (float)canvas_w, cam.zoom_y);
        nvgFillColor(nvg, SCALE_LUT[pitch % 12]
            ? nvgRGBf(0.055f, 0.063f, 0.080f)
            : nvgRGBf(0.082f, 0.094f, 0.122f));
        nvgFill(nvg);
    }
}

static void draw_grid_lines(NVGcontext* nvg, const Camera& cam,
                             int canvas_w, int canvas_h) {
    float first_beat = cam.scroll_x / cam.zoom_x;
    float last_beat  = (cam.scroll_x + (float)canvas_w) / cam.zoom_x;

    for (float beat = floorf(first_beat); beat <= last_beat; beat += 1.0f) {
        float x      = beat_to_x(beat, cam);
        bool  is_bar = ((int)beat % 4 == 0);

        nvgBeginPath(nvg);
        nvgMoveTo(nvg, x, 0.0f);
        nvgLineTo(nvg, x, (float)canvas_h);
        nvgStrokeColor(nvg, is_bar
            ? nvgRGBAf(0.0f, 0.0f, 0.0f, 0.35f)
            : nvgRGBAf(0.0f, 0.0f, 0.0f, 0.15f));
        nvgStrokeWidth(nvg, 1.0f);
        nvgStroke(nvg);
    }
}

static void draw_notes(NVGcontext* nvg, const Song& song, const Camera& cam,
                        int canvas_w, int canvas_h) {
    for (int t = 0; t < (int)song.tracks.size(); t++) {
        const float* c = TRACK_COLORS[t % TRACK_COLOR_COUNT];

        for (const Note& note : song.tracks[t].notes) {
            float beats_start = (float)note.start    / (float)song.ppq;
            float beats_dur   = (float)note.duration / (float)song.ppq;

            float x = beat_to_x(beats_start, cam);
            float y = pitch_to_y(note.pitch, cam);
            float w = std::max(beats_dur * cam.zoom_x, 2.0f);
            float h = cam.zoom_y;

            if (x + w < 0.0f)           continue;
            if (x     > (float)canvas_w) continue;
            if (y + h < 0.0f)           continue;
            if (y     > (float)canvas_h) continue;

            float inset  = 1.5f;
            float nw     = std::max(w - inset * 2.0f, 1.0f);
            float nh     = h - inset * 2.0f;
            float radius = std::min(2.0f, std::min(nw, nh) * 0.5f);

            nvgBeginPath(nvg);
            nvgRoundedRect(nvg, x + inset, y + inset, nw, nh, radius);
            nvgFillColor(nvg, nvgRGBAf(c[0], c[1], c[2], c[3]));
            nvgFill(nvg);
            nvgStrokeColor(nvg, nvgRGBAf(0.0f, 0.0f, 0.0f, 0.45f));
            nvgStrokeWidth(nvg, 1.0f);
            nvgStroke(nvg);
        }
    }
}

// --- piano strip ---

struct KeyDef { float y, h; bool black; };

static constexpr KeyDef KEY_DEFS[12] = {
    { 0.000f, 0.143f, false }, // C
    { 0.083f, 0.082f, true  }, // C#
    { 0.143f, 0.143f, false }, // D
    { 0.250f, 0.082f, true  }, // D#
    { 0.286f, 0.143f, false }, // E
    { 0.429f, 0.143f, false }, // F
    { 0.500f, 0.082f, true  }, // F#
    { 0.571f, 0.143f, false }, // G
    { 0.667f, 0.082f, true  }, // G#
    { 0.714f, 0.143f, false }, // A
    { 0.833f, 0.082f, true  }, // A#
    { 0.857f, 0.143f, false }, // B
};

// Draws one octave into [0,width] × [0,height] local space.
// Caller applies a y-flip transform so C lands at band bottom,
// matching the piano roll's pitch ordering (high pitch = low y).
static void draw_octave(NVGcontext* nvg, float width, float height, uint16_t on_keys = 0) {
    for (int i = 0; i < 12; i++) {
        const KeyDef& k = KEY_DEFS[i];
        if (k.black) continue;
        nvgBeginPath(nvg);
        nvgRect(nvg, 0.0f, k.y * height, width, k.h * height);
        if      (on_keys & (1 << i)) nvgFillColor(nvg, nvgRGBf(0.0f,  0.737f, 0.859f));
        else if (i == 0 || i == 5)   nvgFillColor(nvg, nvgRGBf(0.85f, 0.85f,  0.85f ));
        else                         nvgFillColor(nvg, nvgRGBf(0.96f, 0.95f,  0.93f ));
        nvgFill(nvg);
        nvgStrokeColor(nvg, nvgRGBAf(0.0f, 0.0f, 0.0f, 0.1f));
        nvgStrokeWidth(nvg, 1.0f);
        nvgStroke(nvg);
    }
    for (int i = 0; i < 12; i++) {
        const KeyDef& k = KEY_DEFS[i];
        if (!k.black) continue;
        nvgBeginPath(nvg);
        nvgRect(nvg, 0.0f, k.y * height, width * 0.66f, k.h * height);
        if (on_keys & (1 << i)) nvgFillColor(nvg, nvgRGBf(0.0f,  0.737f, 0.859f));
        else                    nvgFillColor(nvg, nvgRGBf(0.10f, 0.09f,  0.09f ));
        nvgFill(nvg);
    }
}

// Draws the piano strip into the current local coordinate space (origin = strip top-left).
// strip_w = strip_width, strip_h = view height.
static void draw_piano_strip(NVGcontext* nvg, const Camera& cam,
                              float strip_w, float strip_h) {
    for (int oct = 0; oct <= 10; oct++) {
        int   pitch_lo = oct * 12;
        int   pitch_hi = std::min(pitch_lo + 11, 127);
        float y_bottom = (127 - pitch_lo) * cam.zoom_y + cam.zoom_y - cam.scroll_y;
        float band_h   = (float)(pitch_hi - pitch_lo + 1) * cam.zoom_y;
        float y_top    = y_bottom - band_h;

        if (y_bottom < 0.0f || y_top > strip_h) continue;

        nvgSave(nvg);
        nvgTranslate(nvg, 0.0f, y_bottom);
        nvgScale(nvg, 1.0f, -1.0f);
        draw_octave(nvg, strip_w, band_h, 0);
        nvgRestore(nvg);
    }

    // Right-edge separator
    nvgBeginPath(nvg);
    nvgMoveTo(nvg, strip_w, 0.0f);
    nvgLineTo(nvg, strip_w, strip_h);
    nvgStrokeColor(nvg, nvgRGBAf(0.0f, 0.0f, 0.0f, 1.0f));
    nvgStrokeWidth(nvg, 1.0f);
    nvgStroke(nvg);
}

// --- public entry points ---

void draw_piano_roll(NVGcontext* nvg, const Song& song, const PianoRollView& view) {
    const Camera& cam = view.camera;
    Box b             = view.box;

    // Piano strip: left slice of the view
    nvgSave(nvg);
    nvgScissor(nvg, b.x, b.y, view.strip_width, b.h);
    nvgTranslate(nvg, b.x, b.y);
    draw_piano_strip(nvg, cam, view.strip_width, b.h);
    nvgRestore(nvg);

    // Canvas: everything to the right of the strip
    float cx = b.x + view.strip_width;
    float cw = b.w - view.strip_width;

    nvgSave(nvg);
    nvgScissor(nvg, cx, b.y, cw, b.h);
    nvgTranslate(nvg, cx, b.y);
    draw_pitch_lanes(nvg, cam, (int)cw, (int)b.h);
    draw_grid_lines(nvg, cam, (int)cw, (int)b.h);
    draw_notes(nvg, song, cam, (int)cw, (int)b.h);
    nvgRestore(nvg);
}

void piano_roll_draw_ghost(NVGcontext* nvg, const Song& song,
                           const PianoRollView& view, const Note& note) {
    const Camera& cam = view.camera;
    float beats_start = (float)note.start    / (float)song.ppq;
    float beats_dur   = (float)note.duration / (float)song.ppq;
    float x = beat_to_x(beats_start, cam);
    float y = pitch_to_y(note.pitch,  cam);
    float w = std::max(beats_dur * cam.zoom_x, 2.0f);
    float h = cam.zoom_y;

    float inset  = 1.5f;
    float nw     = std::max(w - inset * 2.0f, 1.0f);
    float nh     = h - inset * 2.0f;
    float radius = std::min(2.0f, std::min(nw, nh) * 0.5f);

    float cx = view.box.x + view.strip_width;
    float cw = view.box.w - view.strip_width;

    nvgSave(nvg);
    nvgScissor(nvg, cx, view.box.y, cw, view.box.h);
    nvgTranslate(nvg, cx, view.box.y);
    nvgBeginPath(nvg);
    nvgRoundedRect(nvg, x + inset, y + inset, nw, nh, radius);
    nvgFillColor(nvg,   nvgRGBAf(1.0f, 1.0f, 1.0f, 0.22f));
    nvgFill(nvg);
    nvgStrokeColor(nvg, nvgRGBAf(1.0f, 1.0f, 1.0f, 0.75f));
    nvgStrokeWidth(nvg, 1.5f);
    nvgStroke(nvg);
    nvgRestore(nvg);
}

void piano_roll_draw_cursor(NVGcontext* nvg, const Song& song,
                            const PianoRollView& view, Tick cursor_tick) {
    const Camera& cam = view.camera;
    float x = beat_to_x((float)cursor_tick / (float)song.ppq, cam);
    float cw = view.box.w - view.strip_width;
    if (x < 0.0f || x > cw) return;

    float cx = view.box.x + view.strip_width;

    nvgSave(nvg);
    nvgScissor(nvg, cx, view.box.y, cw, view.box.h);
    nvgTranslate(nvg, cx, view.box.y);
    nvgBeginPath(nvg);
    nvgMoveTo(nvg, x, 0.0f);
    nvgLineTo(nvg, x, view.box.h);
    nvgStrokeColor(nvg, nvgRGBAf(1.0f, 0.78f, 0.18f, 0.9f));
    nvgStrokeWidth(nvg, 1.5f);
    nvgStroke(nvg);
    nvgRestore(nvg);
}

// --- hit testing / coordinate inversion ---
// All public functions receive canvas-local coords (strip already excluded by caller).

static constexpr float HANDLE_WIDTH = 8.0f;

std::optional<NoteHit> piano_roll_hit_test(const Song& song,
                                           const PianoRollView& view,
                                           float cx, float cy) {
    const Camera& cam = view.camera;
    for (int t = 0; t < (int)song.tracks.size(); t++) {
        const auto& notes = song.tracks[t].notes;
        for (int i = 0; i < (int)notes.size(); i++) {
            const Note& n = notes[i];
            float x0 = beat_to_x((float)n.start                / (float)song.ppq, cam);
            float x1 = beat_to_x((float)(n.start + n.duration) / (float)song.ppq, cam);
            float y0 = pitch_to_y(n.pitch, cam);
            float y1 = y0 + cam.zoom_y;
            if (cx < x0 || cx >= x1 || cy < y0 || cy >= y1) continue;

            float w = x1 - x0;
            NotePart part;
            if      (w < HANDLE_WIDTH * 2.0f) part = NotePart::BODY;
            else if (cx < x0 + HANDLE_WIDTH)  part = NotePart::HEAD;
            else if (cx >= x1 - HANDLE_WIDTH) part = NotePart::TAIL;
            else                              part = NotePart::BODY;

            return NoteHit{ t, i, part };
        }
    }
    return std::nullopt;
}

Tick piano_roll_x_to_tick(const Song& song, const PianoRollView& view,
                           float cx, Tick snap_ticks) {
    const Camera& cam = view.camera;
    float beat = (cx + cam.scroll_x) / cam.zoom_x;
    Tick raw = (Tick)(beat * (float)song.ppq);
    if (raw < 0) raw = 0;
    return snap_to_nearest(raw, snap_ticks);
}

int piano_roll_y_to_pitch(const PianoRollView& view, float cy) {
    const Camera& cam = view.camera;
    int pitch = 127 - (int)floorf((cy + cam.scroll_y) / cam.zoom_y);
    return std::clamp(pitch, 0, 127);
}

std::optional<Note> piano_roll_make_note(const Song& song,
                                         const PianoRollView& view,
                                         float cx, float cy,
                                         Tick snap_ticks, Tick dur_ticks, int velocity) {
    if (cx < 0.0f) return std::nullopt; // on the strip
    const Camera& cam = view.camera;
    int pitch = 127 - (int)floorf((cy + cam.scroll_y) / cam.zoom_y);
    if (pitch < 0 || pitch > 127) return std::nullopt;
    float beat    = (cx + cam.scroll_x) / cam.zoom_x;
    Tick raw_tick = (Tick)(beat * (float)song.ppq);
    return Note{ snap_to_nearest(raw_tick, snap_ticks), dur_ticks, pitch, velocity, 0 };
}
