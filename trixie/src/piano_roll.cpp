// piano_roll.cpp
// Renders the piano roll: lane backgrounds, grid lines, notes, piano strip.
// Drawing order matters: lanes first, grid on top, notes on top of that,
// piano strip last so it always sits over the scroll content.

#include "piano_roll.h"
#include "nanovg.h"

#include <cmath>
#include <algorithm>

static constexpr float PIANO_STRIP_WIDTH = 60.0f;

// Which semitones within an octave are black keys (index 0 = C).
// This is a LUT — later it can be swapped out for alternate tuning layouts.
// Director's note: No. at least, not like that... 
// There are two LUTs: This is the one that draws the keyboard.
// The second LUT defaults to a copy of this one and it is meant to show the _scale_ on the lanes
static constexpr bool IS_BLACK_KEY[12] = {
    false, true,  false, true,  false,
    false, true,  false, true,  false, true, false
};

// true means this note is "not used"
bool SCALE_LUT[12] = {
    false, true,  false, true,  false,
    false, true,  false, true,  false, true, false
};

// Per-track note colors. Cycles if there are more tracks than entries.
// Slightly translucent (alpha < 1) so stacked notes stay visible.
static constexpr float TRACK_COLORS[][4] = {
    { 1.000f, 0.839f, 0.584f, 0.92f }, // #FFD18C — warm gold
    { 0.671f, 0.800f, 0.620f, 0.92f }, // #A1C495 — sage green
    { 0.608f, 0.800f, 0.882f, 0.92f }, // rgba(0.6, 0.8, 0.88, 0.92) muted sky
    { 0.867f, 0.659f, 0.482f, 0.92f }, // muted terracotta
    { 0.780f, 0.722f, 0.878f, 0.92f }, // soft lavender
};

static constexpr int TRACK_COLOR_COUNT = 5;

// --- coordinate helpers ---

static float beat_to_x(float beat, const Camera& cam) {
    return PIANO_STRIP_WIDTH + beat * cam.pixels_per_beat - cam.scroll_x;
}

static float pitch_to_y(int pitch, const Camera& cam) {
    return (127 - pitch) * cam.lane_height - cam.scroll_y;
}

// --- drawing passes ---

// Technically, the piano roll itself is a "div".
// DR(Director's note lol): If i'm not mistaken, it technically has its own "viewport"
// the keyboard strip maintains its width, however, its vertical scale matches the vertical scale of the camera's vertical zoom
// You can pan across the lanes, but the piano shoudln't move. This means PIANO_STRIP_WIDTH has nothing to do here from now own, other than setting up a new viewport space _offset_ by that amount
static void draw_pitch_lanes(NVGcontext* nvg, const Camera& cam,
                              int view_width, int view_height) {
    int first_pitch = 127 - (int)floorf(cam.scroll_y / cam.lane_height);
    int last_pitch  = 127 - (int)floorf((cam.scroll_y + view_height) / cam.lane_height) - 1;
    first_pitch = std::min(first_pitch, 127);
    last_pitch  = std::max(last_pitch,  0);

    for (int pitch = last_pitch; pitch <= first_pitch; pitch++) {
        float y = pitch_to_y(pitch, cam);
        bool  black = SCALE_LUT[pitch % 12]; // here it is

        nvgBeginPath(nvg);
        nvgRect(nvg, PIANO_STRIP_WIDTH, y,
                (float)view_width - PIANO_STRIP_WIDTH, cam.lane_height);
        nvgFillColor(nvg, black
            ? nvgRGBf(0.055f, 0.063f, 0.080f)  // black key: noticeably darker
            : nvgRGBf(0.082f, 0.094f, 0.122f)); // white key: #15181f
        nvgFill(nvg);
    }
}

static void draw_grid_lines(NVGcontext* nvg, const Camera& cam,
                             int view_width, int view_height) {
    float roll_width    = (float)view_width - PIANO_STRIP_WIDTH; // DR: same roll viewport note thing
    float first_beat    = cam.scroll_x / cam.pixels_per_beat;
    float last_beat     = (cam.scroll_x + roll_width) / cam.pixels_per_beat;

    for (float beat = floorf(first_beat); beat <= last_beat; beat += 1.0f) {
        float x      = beat_to_x(beat, cam);
        bool  is_bar = ((int)beat % 4 == 0);

        nvgBeginPath(nvg);
        nvgMoveTo(nvg, x, 0.0f);
        nvgLineTo(nvg, x, (float)view_height);
        nvgStrokeColor(nvg, is_bar
            ? nvgRGBAf(0.0f, 0.0f, 0.0f, 0.35f)
            : nvgRGBAf(0.0f, 0.0f, 0.0f, 0.15f));
        nvgStrokeWidth(nvg, 1.0f);
        nvgStroke(nvg);
    }
}

static void draw_notes(NVGcontext* nvg, const Song& song, const Camera& cam,
                        int view_width, int view_height) {
    for (int t = 0; t < (int)song.tracks.size(); t++) {
        const float* c = TRACK_COLORS[t % TRACK_COLOR_COUNT];

        for (const Note& note : song.tracks[t].notes) {
            float beats_start = (float)note.start    / (float)song.ppq;
            float beats_dur   = (float)note.duration / (float)song.ppq;

            float x = beat_to_x(beats_start, cam);
            float y = pitch_to_y(note.pitch, cam);
            float w = std::max(beats_dur * cam.pixels_per_beat, 2.0f); // min 2px
            float h = cam.lane_height;

            // Skip notes entirely outside the viewport
            if (x + w < PIANO_STRIP_WIDTH) continue;
            if (x     > (float)view_width)  continue;
            if (y + h < 0.0f)               continue;
            if (y     > (float)view_height)  continue;

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

/*
    DR: This works fine, low noise... however — and this is just a thought — maybe the piano strip should be _constructed_ using sprites? tiles? texture smaples? Not only that might be easier to tune an cheaper (I don't know that) but people _love_ to edit skins, don't they? lmao
*/
static void draw_piano_strip(NVGcontext* nvg, const Camera& cam, int view_height) {
    // Background
    nvgBeginPath(nvg);
    nvgRect(nvg, 0.0f, 0.0f, PIANO_STRIP_WIDTH, (float)view_height);
    nvgFillColor(nvg, nvgRGBf(0.96f, 0.95f, 0.93f)); // ivory white keys
    nvgFill(nvg);

    // Black key blocks
    int first_pitch = 127 - (int)floorf(cam.scroll_y / cam.lane_height);
    int last_pitch  = 127 - (int)floorf((cam.scroll_y + view_height) / cam.lane_height) - 1;
    first_pitch = std::min(first_pitch, 127);
    last_pitch  = std::max(last_pitch,  0);

    for (int pitch = last_pitch; pitch <= first_pitch; pitch++) {
        if (!IS_BLACK_KEY[pitch % 12]) continue;
        float y = pitch_to_y(pitch, cam);

        nvgBeginPath(nvg);
        nvgRect(nvg, 0.0f, y + 1.5f, PIANO_STRIP_WIDTH * 0.65f, cam.lane_height - 3.0f);
        nvgFillColor(nvg, nvgRGBf(0.10f, 0.09f, 0.09f)); // near-black ebony
        nvgFill(nvg);
    }

    // Right-edge separator line
    nvgBeginPath(nvg);
    nvgMoveTo(nvg, PIANO_STRIP_WIDTH, 0.0f);
    nvgLineTo(nvg, PIANO_STRIP_WIDTH, (float)view_height);
    nvgStrokeColor(nvg, nvgRGBAf(0.0f, 0.0f, 0.0f, 0.5f));
    nvgStrokeWidth(nvg, 2.0f);
    nvgStroke(nvg);
}

// --- ghost note overlay ---

void piano_roll_draw_ghost(NVGcontext* nvg, const Song& song, const Panel& panel, const Note& note) {
    const Camera& cam    = panel.camera;
    float beats_start    = (float)note.start    / (float)song.ppq;
    float beats_dur      = (float)note.duration / (float)song.ppq;
    float x = beat_to_x(beats_start, cam);
    float y = pitch_to_y(note.pitch,  cam);
    float w = std::max(beats_dur * cam.pixels_per_beat, 2.0f);
    float h = cam.lane_height;

    float inset  = 1.5f;
    float nw     = std::max(w - inset * 2.0f, 1.0f);
    float nh     = h - inset * 2.0f;
    float radius = std::min(2.0f, std::min(nw, nh) * 0.5f);

    nvgBeginPath(nvg);
    nvgRoundedRect(nvg, x + inset, y + inset, nw, nh, radius);
    nvgFillColor(nvg,   nvgRGBAf(1.0f, 1.0f, 1.0f, 0.22f));
    nvgFill(nvg);
    nvgStrokeColor(nvg, nvgRGBAf(1.0f, 1.0f, 1.0f, 0.75f));
    nvgStrokeWidth(nvg, 1.5f);
    nvgStroke(nvg);
}

// --- playback cursor ---

void piano_roll_draw_cursor(NVGcontext* nvg, const Song& song, const Panel& panel, Tick cursor_tick) {
    const Camera& cam = panel.camera;
    float x = beat_to_x((float)cursor_tick / song.ppq, cam);
    if (x < PIANO_STRIP_WIDTH || x > panel.w) return;

    nvgBeginPath(nvg);
    nvgMoveTo(nvg, x, 0.0f);
    nvgLineTo(nvg, x, panel.h);
    nvgStrokeColor(nvg, nvgRGBAf(1.0f, 0.78f, 0.18f, 0.9f)); // warm amber
    nvgStrokeWidth(nvg, 1.5f);
    nvgStroke(nvg);
}

// --- hit testing / coordinate inversion ---

// Width of the head/tail drag handles in pixels.
// Notes narrower than 2× this are treated as body-only. DN: This is problematic, because it means tiny notes cannot be resized. I belive it makes sense to have head and tail _handles_ extend _beyond_ the body size. This would allow better handling, and narrow notes to be resized (which is nice for dragging.)

static constexpr float HANDLE_WIDTH = 8.0f;

std::optional<NoteHit> piano_roll_hit_test(const Song& song, const Panel& panel, float mx, float my) {
    const Camera& cam = panel.camera;
    for (int t = 0; t < (int)song.tracks.size(); t++) {
        const auto& notes = song.tracks[t].notes;
        for (int i = 0; i < (int)notes.size(); i++) {
            const Note& n = notes[i];
            float x0 = beat_to_x((float)n.start                  / song.ppq, cam);
            float x1 = beat_to_x((float)(n.start + n.duration)   / song.ppq, cam);
            float y0 = pitch_to_y(n.pitch, cam);
            float y1 = y0 + cam.lane_height;
            if (mx < x0 || mx >= x1 || my < y0 || my >= y1) continue;

            float w = x1 - x0;
            NotePart part;
            if (w < HANDLE_WIDTH * 2.0f) {
                part = NotePart::BODY;
            } else if (mx < x0 + HANDLE_WIDTH) {
                part = NotePart::HEAD;
            } else if (mx >= x1 - HANDLE_WIDTH) {
                part = NotePart::TAIL;
            } else {
                part = NotePart::BODY;
            }
            return NoteHit{ t, i, part };
        }
    }
    return std::nullopt;
}

Tick piano_roll_x_to_tick(const Song& song, const Panel& panel, float mx, Tick snap_ticks) {
    const Camera& cam = panel.camera;
    float beat = (mx - PIANO_STRIP_WIDTH + cam.scroll_x) / cam.pixels_per_beat;
    Tick raw = (Tick)(beat * song.ppq);
    if (raw < 0) raw = 0;
    return snap_to_nearest(raw, snap_ticks);
}

int piano_roll_y_to_pitch(const Panel& panel, float my) {
    const Camera& cam = panel.camera;
    int pitch = 127 - (int)floorf((my + cam.scroll_y) / cam.lane_height);
    return std::clamp(pitch, 0, 127);
}

std::optional<Note> piano_roll_make_note(const Song& song, const Panel& panel,
                                         float mx, float my,
                                         Tick snap_ticks, Tick dur_ticks, int velocity) {
    if (mx < PIANO_STRIP_WIDTH) return std::nullopt;
    const Camera& cam = panel.camera;
    int pitch = 127 - (int)floorf((my + cam.scroll_y) / cam.lane_height);
    if (pitch < 0 || pitch > 127) return std::nullopt;
    float beat    = (mx - PIANO_STRIP_WIDTH + cam.scroll_x) / cam.pixels_per_beat;
    Tick raw_tick = (Tick)(beat * song.ppq);
    return Note{ snap_to_nearest(raw_tick, snap_ticks), dur_ticks, pitch, velocity, 0 };
}

// --- public entry point ---

void draw_piano_roll(NVGcontext* nvg, const Song& song, const Panel& panel) {
    int view_width  = (int)panel.w;
    int view_height = (int)panel.h;
    const Camera& camera = panel.camera;
    draw_pitch_lanes(nvg, camera, view_width, view_height);
    draw_grid_lines(nvg, camera, view_width, view_height);
    draw_notes(nvg, song, camera, view_width, view_height);
    draw_piano_strip(nvg, camera, view_height); // drawn last so it overlays the scroll
}
