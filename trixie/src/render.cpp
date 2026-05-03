// render.cpp
// Render thread: owns GL context, runs NanoVG, dispatches to the Piano Roll editor.

#include "render.h"
#include "piano_roll.h"
#include "journal.h"

#include <glad/gl.h>
#include <GLFW/glfw3.h>

#define NANOVG_GLFW
#define NANOVG_GL3_IMPLEMENTATION
#include "nanovg_gl.h"
#include "nanovg.h"

#include <algorithm>
#include <cstdio>

void render_thread_run(Window& window, std::atomic<bool>& running, Song& song,
                       InputQueue& input_queue, PlaybackEngine& engine) {
    glfwMakeContextCurrent(window.handle);

    if (!gladLoadGL(glfwGetProcAddress)) {
        fprintf(stderr, "trixie: gladLoadGL failed\n");
        return;
    }

    glfwSwapInterval(1);

    NVGcontext* nvg = nvgCreateGL3(NVG_ANTIALIAS | NVG_STENCIL_STROKES);
    if (!nvg) {
        fprintf(stderr, "trixie: nvgCreateGL3 failed\n");
        return;
    }

    if (nvgCreateFont(nvg, "ui", TRIXIE_ASSETS_DIR "/fonts/JetBrainsMono/fonts/ttf/JetBrainsMono-Regular.ttf") < 0)
        fprintf(stderr, "trixie: failed to load UI font\n");

    SpaceType*     space_type = piano_roll_space_type();
    SpacePianoRoll space;
    ARegion        regions[PIANO_ROLL_REGION_COUNT];
    Journal        journal;

    while (running.load()) {
        int fb_width, fb_height;
        glfwGetFramebufferSize(window.handle, &fb_width, &fb_height);

        int win_width, win_height;
        glfwGetWindowSize(window.handle, &win_width, &win_height);
        float pixel_ratio = (float)fb_width / (float)win_width;

        // --- layout (recomputed every frame, free) ---
        Box screen = { 0.0f, 0.0f, (float)win_width, (float)win_height };
        piano_roll_compute_layout(space, screen, regions);

        // Wire runtime_type pointers each frame (cheap pointer assignments)
        for (int i = 0; i < PIANO_ROLL_REGION_COUNT; i++)
            regions[i].runtime_type = &space_type->region_types[i];

        // Viewport init/sync — init once, then sync screen rect every frame.
        {
            const Box& wbox = regions[(int)RegionType::Window].winrct;
            if (!space.viewport_initialized) {
                space.viewport = {
                    0.0f, 0.0f, 100.0f, 128.0f,   
                    0.0f, 0.0f, wbox.w, wbox.h,
                    0.0f, 0.0f, 100.0f, 128.0f
                };
                space.viewport_initialized = true;
            } else {
                vp_update(space.viewport);                
            }
        }

        // Sync playback state into space data before draw
        space.cursor_tick = engine.cursor_tick();
        space.is_playing  = engine.is_playing();

        // --- event dispatch ---
        ARegion& win = regions[(int)RegionType::Window];

        for (const auto& event : input_queue.drain()) {
            std::visit([&](auto&& e) {
                using T = std::decay_t<decltype(e)>;

                if constexpr (std::is_same_v<T, KeyEvent>) {
                    // Keyboard events are global — not region-bound
                    if (e.action == GLFW_PRESS || e.action == GLFW_REPEAT) {
                        bool ctrl = e.mods & INPUT_MOD_CONTROL;
                        bool shft = e.mods & INPUT_MOD_SHIFT;
                        if (ctrl && e.key == GLFW_KEY_Z)
                            shft ? journal.redo() : journal.undo();
                        else if (e.key == GLFW_KEY_SPACE)
                            engine.toggle();
                    }
                } else if constexpr (std::is_same_v<T, ScrollEvent>) {
                    // Scroll only active when cursor is below the header
                    if (e.my >= PIANO_ROLL_HEADER_H && win.runtime_type->handle_event)
                        win.runtime_type->handle_event(win, space, song, journal, event);
                } else {
                    // Mouse button / move → Window region (only interactive region for now)
                    if (win.runtime_type->handle_event)
                        win.runtime_type->handle_event(win, space, song, journal, event);
                }
            }, event);
        }

        // --- draw ---
        glViewport(0, 0, fb_width, fb_height);
        glClearColor(0.012f, 0.027f, 0.051f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);

        nvgBeginFrame(nvg, (float)win_width, (float)win_height, pixel_ratio);

        for (int i = 0; i < PIANO_ROLL_REGION_COUNT; i++) {
            auto* rt = regions[i].runtime_type;
            if (rt->draw)
                rt->draw(nvg, regions[i], space, song);
        }


        // Overlays drawn on top of the Window region
        piano_roll_draw_canvas_crosshair(nvg, win, space);

        if (space.pending_note)
            piano_roll_draw_ghost(nvg, win, space, song, *space.pending_note);
        if (space.is_playing)
            piano_roll_draw_playback_cursor(nvg, win, space, song, space.cursor_tick);

        nvgEndFrame(nvg);
        glfwSwapBuffers(window.handle);
    }

    nvgDeleteGL3(nvg);
    glfwMakeContextCurrent(nullptr);
}
