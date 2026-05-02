#pragma once
#include "nanovg.h"

struct Theme {
    NVGcolor bg_surface;
    NVGcolor bg_surface_transparent;
    NVGcolor bg_surface_text;
};

inline const Theme& theme() {
    static const Theme t = {
        nvgRGBAf(0.157f, 0.169f, 0.200f, 1.0f),
        nvgRGBAf(0.157f, 0.169f, 0.200f, 0.0f),
        nvgRGBAf(0.831f, 0.886f, 1.000f, 1.0f)
    };
    return t;
}