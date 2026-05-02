// toolbar.cpp

#include "toolbar.h"
#include "nanovg.h"

#include <cstdio>

void draw_toolbar(NVGcontext* nvg, Box b, const Song& song, Tick cursor_tick, bool is_playing) {
    nvgSave(nvg);
    nvgScissor(nvg, b.x, b.y, b.w, b.h);
    nvgTranslate(nvg, b.x, b.y);

    // Background
    nvgBeginPath(nvg);
    nvgRect(nvg, 0.0f, 0.0f, b.w, b.h);
    nvgFillColor(nvg, nvgRGBf(0.055f, 0.063f, 0.080f));
    nvgFill(nvg);

    // Bottom separator line
    nvgBeginPath(nvg);
    nvgMoveTo(nvg, 0.0f, b.h - 1.0f);
    nvgLineTo(nvg, b.w,  b.h - 1.0f);
    nvgStrokeColor(nvg, nvgRGBAf(0.0f, 0.0f, 0.0f, 0.6f));
    nvgStrokeWidth(nvg, 1.0f);
    nvgStroke(nvg);

    // Position: BAR : BEAT (1-indexed, 4/4 assumed)
    int bar  = (int)(cursor_tick / (song.ppq * 4)) + 1;
    int beat = (int)(cursor_tick /  song.ppq) % 4  + 1;
    char pos[32];
    snprintf(pos, sizeof(pos), "%03d : %d", bar, beat);

    float cy = b.h * 0.5f;

    nvgFontFace(nvg, "ui");
    nvgFontSize(nvg, 24.0f);
    nvgTextAlign(nvg, NVG_ALIGN_LEFT | NVG_ALIGN_MIDDLE);
    nvgFillColor(nvg, nvgRGBf(0.75f, 0.80f, 0.88f)); // cool blue-white
    nvgText(nvg, 12.0f, cy, pos, nullptr);

    // BPM
    char bpm[32];
    snprintf(bpm, sizeof(bpm), "%.1f BPM", song.bpm);
    nvgFillColor(nvg, nvgRGBf(0.45f, 0.50f, 0.58f)); // dimmer
    nvgText(nvg, 110.0f, cy, bpm, nullptr);

    // Play indicator dot
    if (is_playing) {
        nvgBeginPath(nvg);
        nvgCircle(nvg, b.w - 16.0f, cy, 4.5f);
        nvgFillColor(nvg, nvgRGBAf(0.28f, 0.90f, 0.45f, 1.0f)); // green
        nvgFill(nvg);
    }

    nvgRestore(nvg);
}
