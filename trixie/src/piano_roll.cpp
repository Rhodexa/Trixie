// piano_roll.cpp
// Piano Roll editor — SpaceType registration, region layout, draw/event callbacks.
// Internal draw helpers work in region-local coordinate space (origin = region top-left).

#include "piano_roll.h"
#include "piano_roll_ops.h"
#include "toolbar.h"
#include "nanovg.h"

#include <cmath>
#include <cstdio>
#include <algorithm>
#include <cstdint>

// ============================================================
// Palette / lookup tables
// ============================================================

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

// ============================================================
// Coordinate helpers (canvas-local)
// World units: x = beats, y = semitones (0-127).
// screen = world * zoom - scroll
// ============================================================
// DN: See ths is where the new viewport would come in handy.

static float beat_to_x(float beat, const Camera& cam) {
    return beat * cam.zoom_x - cam.scroll_x;
}

static float pitch_to_y(int pitch, const Camera& cam) {
    return (127 - pitch) * cam.zoom_y - cam.scroll_y;
}

// ============================================================
// Internal draw helpers — work in local origin coords after nvgTranslate
// ============================================================

static void draw_pitch_lanes(NVGcontext* nvg, const Camera& cam, int w, int h) {
    int first_pitch = 127 - (int)floorf(cam.scroll_y / cam.zoom_y);
    int last_pitch  = 127 - (int)floorf((cam.scroll_y + (float)h) / cam.zoom_y) - 1;
    first_pitch = std::min(first_pitch, 127);
    last_pitch  = std::max(last_pitch,  0);

    for (int pitch = last_pitch; pitch <= first_pitch; pitch++) {
        float y = pitch_to_y(pitch, cam);
        nvgBeginPath(nvg);
        nvgRect(nvg, 0.0f, y, (float)w, cam.zoom_y);
        nvgFillColor(nvg, SCALE_LUT[pitch % 12]
            ? nvgRGBf(0.055f, 0.063f, 0.080f)
            : nvgRGBf(0.082f, 0.094f, 0.122f));
        nvgFill(nvg);
    }
}

static void draw_grid_lines(NVGcontext* nvg, const Camera& cam, int w, int h) {
    float first_beat = cam.scroll_x / cam.zoom_x;
    float last_beat  = (cam.scroll_x + (float)w) / cam.zoom_x;

    for (float beat = floorf(first_beat); beat <= last_beat; beat += 1.0f) {
        float x      = beat_to_x(beat, cam);
        bool  is_bar = ((int)beat % 4 == 0);

        nvgBeginPath(nvg);
        nvgMoveTo(nvg, x, 0.0f);
        nvgLineTo(nvg, x, (float)h);
        nvgStrokeColor(nvg, is_bar
            ? nvgRGBAf(0.0f, 0.0f, 0.0f, 0.35f)
            : nvgRGBAf(0.0f, 0.0f, 0.0f, 0.15f));
        nvgStrokeWidth(nvg, 1.0f);
        nvgStroke(nvg);
    }
}

static void draw_notes(NVGcontext* nvg, const Song& song, const Camera& cam, int w, int h) {
    for (int t = 0; t < (int)song.tracks.size(); t++) {
        const float* c = TRACK_COLORS[t % TRACK_COLOR_COUNT];

        for (const Note& note : song.tracks[t].notes) {
            float beats_start = (float)note.start    / (float)song.ppq;
            float beats_dur   = (float)note.duration / (float)song.ppq;

            float x  = beat_to_x(beats_start, cam);
            float y  = pitch_to_y(note.pitch, cam);
            float nw = std::max(beats_dur * cam.zoom_x, 2.0f);
            float nh = cam.zoom_y;

            if (x + nw < 0.0f || x > (float)w) continue;
            if (y + nh < 0.0f || y > (float)h) continue;

            float inset  = 1.5f;
            float rw     = std::max(nw - inset * 2.0f, 1.0f);
            float rh     = nh - inset * 2.0f;
            float radius = std::min(2.0f, std::min(rw, rh) * 0.5f);

            nvgBeginPath(nvg);
            nvgRoundedRect(nvg, x + inset, y + inset, rw, rh, radius);
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
    { 1 - 0.0000f, -0.1389f, false },      // C
    { 1 - 0.0833f, -0.0833f, true  },      //     C#
    { 1 - 0.1389f, -0.1389f, false },      // D
    { 1 - 0.2500f, -0.0833f, true  },      //     D#
    { 1 - 0.2777f, -0.1389f, false },      // E
    { 1 - 0.4166f, -0.1458f, false },      // F
    { 1 - 0.5000f, -0.0833f, true  },      //     F#
    { 1 - 0.5625f, -0.1458f, false },      // G
    { 1 - 0.6667f, -0.0833f, true  },      //     G#
    { 1 - 0.7083f, -0.1458f, false },      // A
    { 1 - 0.8333f, -0.0833f, true  },      //     A#
    { 1 - 0.8542f, -0.1458f, false },      // B
};

static void draw_octave(NVGcontext* nvg, float x, float y, float width, float height, uint16_t on_keys = 0) {
    for (int i = 0; i < 12; i++) {
        const KeyDef& k = KEY_DEFS[i];
        if (k.black) continue;
        nvgBeginPath(nvg);
        nvgRect(nvg, x, y + k.y * height, width, k.h * height);
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
        nvgRect(nvg, x, y + k.y * height, width * 0.66f, k.h * height);
        if (on_keys & (1 << i)) nvgFillColor(nvg, nvgRGBf(0.0f,  0.737f, 0.859f));
        else                    nvgFillColor(nvg, nvgRGBf(0.10f, 0.09f,  0.09f ));
        nvgFill(nvg);
    }
}

static void draw_piano_strip_impl(NVGcontext* nvg, const Camera& cam, float strip_w, float strip_h) {
    nvgSave(nvg);
    for (int oct = 0; oct < 11; oct++) {
        draw_octave(nvg, 0, ((128 - 12 - oct * 12) * cam.zoom_y) - cam.scroll_y, strip_w, 12.0f * cam.zoom_y, 0);
    }
    nvgRestore(nvg);
}

// ============================================================
// Forward declarations of region callbacks (defined below)
// ============================================================

static void piano_roll_header_draw   (NVGcontext*, ARegion&, const SpacePianoRoll&, const Song&);
static void piano_roll_timeruler_draw(NVGcontext*, ARegion&, const SpacePianoRoll&, const Song&);
static void piano_roll_channels_draw (NVGcontext*, ARegion&, const SpacePianoRoll&, const Song&);
static void piano_roll_window_draw   (NVGcontext*, ARegion&, const SpacePianoRoll&, const Song&);
static void piano_roll_ui_draw       (NVGcontext*, ARegion&, const SpacePianoRoll&, const Song&);
static void piano_roll_scrollbar_draw(NVGcontext*, ARegion&, const SpacePianoRoll&, const Song&);

static bool piano_roll_window_handle_event(ARegion&, SpacePianoRoll&, Song&, Journal&, const InputEvent&);

// ============================================================
// ARegionType table + SpaceType registration
// ============================================================

static ARegionType piano_roll_region_types[PIANO_ROLL_REGION_COUNT] = {
    { RegionType::Header,    "Header",    piano_roll_header_draw,    nullptr },
    { RegionType::TimeRuler, "TimeRuler", piano_roll_timeruler_draw, nullptr },
    { RegionType::Channels,  "Channels",  piano_roll_channels_draw,  nullptr },
    { RegionType::Window,    "Window",    piano_roll_window_draw,    piano_roll_window_handle_event },
    { RegionType::UI,        "UI",        piano_roll_ui_draw,        nullptr },
    { RegionType::Scrollbar, "Scrollbar", piano_roll_scrollbar_draw, nullptr },
};

SpaceType* piano_roll_space_type() {
    static SpaceType st = {
        "Piano Roll",
        piano_roll_region_types,
        PIANO_ROLL_REGION_COUNT,
    };
    return &st;
}

// ============================================================
// Layout
// ============================================================

void piano_roll_compute_layout(const SpacePianoRoll& space, Box screen,
                                ARegion regions[PIANO_ROLL_REGION_COUNT]) {
    regions[(int)RegionType::Header]    = { RegionType::Header,    box_split_top   (&screen, PIANO_ROLL_HEADER_H),    nullptr };
    regions[(int)RegionType::TimeRuler] = { RegionType::TimeRuler, box_split_top   (&screen, PIANO_ROLL_TIMERULER_H), nullptr };
    regions[(int)RegionType::UI]        = { RegionType::UI,        box_split_bottom(&screen, PIANO_ROLL_UI_H),        nullptr };
    regions[(int)RegionType::Scrollbar] = { RegionType::Scrollbar, box_split_right (&screen, PIANO_ROLL_SCROLLBAR_W), nullptr };
    regions[(int)RegionType::Channels]  = { RegionType::Channels,  box_split_left  (&screen, space.strip_width),      nullptr };
    regions[(int)RegionType::Window]    = { RegionType::Window,    screen,                                             nullptr };
}

// ============================================================
// Region draw callbacks
// Each: nvgSave → scissor + translate to region-local origin → draw → nvgRestore
// ============================================================

static void piano_roll_header_draw(NVGcontext* nvg, ARegion& region,
                                    const SpacePianoRoll& space, const Song& song) {
    draw_toolbar(nvg, region.winrct, song, space.cursor_tick, space.is_playing);
}

static void piano_roll_timeruler_draw(NVGcontext* nvg, ARegion& region,
                                       const SpacePianoRoll& space, const Song& song) {
    const Box&    b   = region.winrct;
    const Camera& cam = space.camera;

    nvgSave(nvg);
    nvgScissor(nvg, b.x, b.y, b.w, b.h);
    nvgTranslate(nvg, b.x, b.y);

    // Background
    nvgBeginPath(nvg);
    nvgRect(nvg, 0.0f, 0.0f, b.w, b.h);
    nvgFillColor(nvg, nvgRGBf(0.055f, 0.063f, 0.080f));
    nvgFill(nvg);

    // Bottom separator
    nvgBeginPath(nvg);
    nvgMoveTo(nvg, 0.0f, b.h - 1.0f);
    nvgLineTo(nvg, b.w,  b.h - 1.0f);
    nvgStrokeColor(nvg, nvgRGBAf(0.0f, 0.0f, 0.0f, 0.6f));
    nvgStrokeWidth(nvg, 1.0f);
    nvgStroke(nvg);

    // Beat markers — offset by strip_width to align with canvas
    float rx         = space.strip_width;
    float canvas_w   = b.w - rx;
    float first_beat = cam.scroll_x / cam.zoom_x;
    float last_beat  = (cam.scroll_x + canvas_w) / cam.zoom_x;

    nvgFontFace(nvg, "ui");
    nvgFontSize(nvg, 10.0f);

    for (float beat = floorf(first_beat); beat <= last_beat; beat += 1.0f) {
        float x      = rx + beat * cam.zoom_x - cam.scroll_x;
        bool  is_bar = ((int)beat % 4 == 0);

        float tick_h = is_bar ? b.h * 0.55f : b.h * 0.28f;
        nvgBeginPath(nvg);
        nvgMoveTo(nvg, x, b.h - tick_h);
        nvgLineTo(nvg, x, b.h);
        nvgStrokeColor(nvg, nvgRGBAf(0.5f, 0.55f, 0.65f, is_bar ? 0.7f : 0.35f));
        nvgStrokeWidth(nvg, 1.0f);
        nvgStroke(nvg);

        if (is_bar) {
            int  bar = (int)beat / 4 + 1;
            char label[16];
            snprintf(label, sizeof(label), "%d", bar);
            nvgTextAlign(nvg, NVG_ALIGN_LEFT | NVG_ALIGN_TOP);
            nvgFillColor(nvg, nvgRGBf(0.50f, 0.56f, 0.68f));
            nvgText(nvg, x + 3.0f, 3.0f, label, nullptr);
        }
    }

    nvgRestore(nvg);
}

static void piano_roll_channels_draw(NVGcontext* nvg, ARegion& region, const SpacePianoRoll& space, const Song& /*song*/) {
    const Box& b = region.winrct;
    nvgSave(nvg);
    nvgScissor(nvg, b.x, b.y, b.w, b.h);
    nvgTranslate(nvg, b.x, b.y);
    draw_piano_strip_impl(nvg, space.camera, b.w, b.h);
    nvgRestore(nvg);
}

static void piano_roll_window_draw(NVGcontext* nvg, ARegion& region, const SpacePianoRoll& space, const Song& song) {
    const Box& b = region.winrct;
    nvgSave(nvg);
    nvgScissor(nvg, b.x, b.y, b.w, b.h);
    nvgTranslate(nvg, b.x, b.y);
    draw_pitch_lanes(nvg, space.camera, (int)b.w, (int)b.h);
    draw_grid_lines (nvg, space.camera, (int)b.w, (int)b.h);
    draw_notes      (nvg, song, space.camera, (int)b.w, (int)b.h);
    nvgRestore(nvg);
}

static void piano_roll_ui_draw(NVGcontext* nvg, ARegion& region,
                                const SpacePianoRoll& /*space*/, const Song& /*song*/) {
    const Box& b = region.winrct;
    nvgSave(nvg);
    nvgScissor(nvg, b.x, b.y, b.w, b.h);
    nvgTranslate(nvg, b.x, b.y);

    nvgBeginPath(nvg);
    nvgRect(nvg, 0.0f, 0.0f, b.w, b.h);
    nvgFillColor(nvg, nvgRGBf(0.048f, 0.055f, 0.072f));
    nvgFill(nvg);

    nvgBeginPath(nvg);
    nvgMoveTo(nvg, 0.0f, 0.5f);
    nvgLineTo(nvg, b.w,  0.5f);
    nvgStrokeColor(nvg, nvgRGBAf(0.0f, 0.0f, 0.0f, 0.6f));
    nvgStrokeWidth(nvg, 1.0f);
    nvgStroke(nvg);

    nvgRestore(nvg);
}

static void piano_roll_scrollbar_draw(NVGcontext* nvg, ARegion& region,
                                       const SpacePianoRoll& /*space*/, const Song& /*song*/) {
    const Box& b = region.winrct;
    nvgSave(nvg);
    nvgScissor(nvg, b.x, b.y, b.w, b.h);
    nvgTranslate(nvg, b.x, b.y);

    nvgBeginPath(nvg);
    nvgRect(nvg, 0.0f, 0.0f, b.w, b.h);
    nvgFillColor(nvg, nvgRGBf(0.048f, 0.055f, 0.072f));
    nvgFill(nvg);

    nvgRestore(nvg);
}

// ============================================================
// Window region event handler
// ============================================================

static bool piano_roll_window_handle_event(ARegion& region, SpacePianoRoll& space,
                                            Song& song, Journal& journal,
                                            const InputEvent& raw) {
    wmEvent     ev  = wm_event_from_input(raw);
    wmOpContext ctx { region, space, song, journal };
    return wm_handle_event(ctx, ev, piano_roll_keymap(), space.active_modal_op);
}

// ============================================================
// Overlay draws (called from render.cpp after region loop)
// ============================================================

void piano_roll_draw_ghost(NVGcontext* nvg, ARegion& window,
                            const SpacePianoRoll& space, const Song& song, const Note& note) {
    const Box&    b   = window.winrct;
    const Camera& cam = space.camera;

    float beats_start = (float)note.start    / (float)song.ppq;
    float beats_dur   = (float)note.duration / (float)song.ppq;
    float x  = beat_to_x(beats_start, cam);
    float y  = pitch_to_y(note.pitch,  cam);
    float w  = std::max(beats_dur * cam.zoom_x, 2.0f);
    float h  = cam.zoom_y;

    float inset  = 1.5f;
    float nw     = std::max(w - inset * 2.0f, 1.0f);
    float nh     = h - inset * 2.0f;
    float radius = std::min(2.0f, std::min(nw, nh) * 0.5f);

    nvgSave(nvg);
    nvgScissor(nvg, b.x, b.y, b.w, b.h);
    nvgTranslate(nvg, b.x, b.y);
    nvgBeginPath(nvg);
    nvgRoundedRect(nvg, x + inset, y + inset, nw, nh, radius);
    nvgFillColor(nvg,   nvgRGBAf(1.0f, 1.0f, 1.0f, 0.22f));
    nvgFill(nvg);
    nvgStrokeColor(nvg, nvgRGBAf(1.0f, 1.0f, 1.0f, 0.75f));
    nvgStrokeWidth(nvg, 1.5f);
    nvgStroke(nvg);
    nvgRestore(nvg);
}

void piano_roll_draw_cursor(NVGcontext* nvg, ARegion& window,
                             const SpacePianoRoll& space, const Song& song, Tick cursor_tick) {
    const Box&    b   = window.winrct;
    const Camera& cam = space.camera;
    float x = beat_to_x((float)cursor_tick / (float)song.ppq, cam);
    if (x < 0.0f || x > b.w) return;

    nvgSave(nvg);
    nvgScissor(nvg, b.x, b.y, b.w, b.h);
    nvgTranslate(nvg, b.x, b.y);
    nvgBeginPath(nvg);
    nvgMoveTo(nvg, x, 0.0f);
    nvgLineTo(nvg, x, b.h);
    nvgStrokeColor(nvg, nvgRGBAf(1.0f, 0.78f, 0.18f, 0.9f));
    nvgStrokeWidth(nvg, 1.5f);
    nvgStroke(nvg);
    nvgRestore(nvg);
}

// ============================================================
// Coordinate utilities
// ============================================================

static constexpr float HANDLE_WIDTH = 8.0f;

std::optional<NoteHit> piano_roll_hit_test(const Song& song, const SpacePianoRoll& space,
                                            float cx, float cy) {
    const Camera& cam = space.camera;
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

Tick piano_roll_x_to_tick(const Song& song, const SpacePianoRoll& space,
                            float cx, Tick snap_ticks) {
    const Camera& cam = space.camera;
    float beat = (cx + cam.scroll_x) / cam.zoom_x;
    Tick raw   = (Tick)(beat * (float)song.ppq);
    if (raw < 0) raw = 0;
    return snap_to_nearest(raw, snap_ticks);
}

int piano_roll_y_to_pitch(const SpacePianoRoll& space, float cy) {
    const Camera& cam = space.camera;
    int pitch = 127 - (int)floorf((cy + cam.scroll_y) / cam.zoom_y);
    return std::clamp(pitch, 0, 127);
}

std::optional<Note> piano_roll_make_note(const Song& song, const SpacePianoRoll& space,
                                          float cx, float cy,
                                          Tick snap_ticks, Tick dur_ticks, int velocity) {
    if (cx < 0.0f) return std::nullopt;
    const Camera& cam = space.camera;
    int pitch = 127 - (int)floorf((cy + cam.scroll_y) / cam.zoom_y);
    if (pitch < 0 || pitch > 127) return std::nullopt;
    float beat    = (cx + cam.scroll_x) / cam.zoom_x;
    Tick raw_tick = (Tick)(beat * (float)song.ppq);
    return Note{ snap_to_nearest(raw_tick, snap_ticks), dur_ticks, pitch, velocity, 0 };
}
