// render.cpp
// Render thread: owns GL context, runs NanoVG, draws the piano roll.

#include "render.h"
#include "piano_roll.h"
#include "panel.h"
#include "input_queue.h"
#include "journal.h"
#include "note_commands.h"

#include <glad/gl.h>
#include <GLFW/glfw3.h>

#define NANOVG_GLFW
#define NANOVG_GL3_IMPLEMENTATION
#include "nanovg_gl.h"
#include "nanovg.h"

#include <algorithm>
#include <optional>
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

    Panel   piano_roll_panel;
    Journal journal;

    bool mmb_held    = false;
    bool rmb_held    = false;
    bool lmb_placing = false;
    std::optional<Note> pending_note;

    bool camera_initialized = false;

    while (running.load()) {
        int fb_width, fb_height;
        glfwGetFramebufferSize(window.handle, &fb_width, &fb_height);

        int win_width, win_height;
        glfwGetWindowSize(window.handle, &win_width, &win_height);
        float pixel_ratio = (float)fb_width / (float)win_width;

        piano_roll_panel.x = 0.0f;
        piano_roll_panel.y = 0.0f;
        piano_roll_panel.w = (float)win_width;
        piano_roll_panel.h = (float)win_height;

        if (!camera_initialized) {
            float center_y = (127 - 60) * piano_roll_panel.camera.lane_height - (float)win_height * 0.5f;
            piano_roll_panel.camera.scroll_y = std::max(0.0f, center_y);
            camera_initialized = true;
        }

        Tick snap_ticks = song.ppq; // 1 quarter note

        for (const auto& event : input_queue.drain()) {
            std::visit([&](auto&& e) {
                using T = std::decay_t<decltype(e)>;

                if constexpr (std::is_same_v<T, ScrollEvent>) {
                    if (panel_contains(piano_roll_panel, e.mx, e.my)) {
                        if (e.mods & INPUT_MOD_CONTROL)
                            piano_roll_panel.camera.scroll_x = std::max(0.0f, piano_roll_panel.camera.scroll_x - e.dy * 60.0f);
                        else
                            piano_roll_panel.camera.scroll_y = std::max(0.0f, piano_roll_panel.camera.scroll_y - e.dy * 30.0f);
                    }

                } else if constexpr (std::is_same_v<T, MouseButtonEvent>) {
                    if (e.button == GLFW_MOUSE_BUTTON_MIDDLE) {
                        mmb_held = e.pressed;

                    } else if (e.button == GLFW_MOUSE_BUTTON_LEFT) {
                        if (e.pressed) {
                            auto hit = piano_roll_hit_test(song, piano_roll_panel, e.x, e.y);
                            if (!hit && !song.tracks.empty()) {
                                auto note = piano_roll_make_note(song, piano_roll_panel,
                                                                 e.x, e.y, snap_ticks, snap_ticks, 100);
                                if (note) {
                                    pending_note = note;
                                    lmb_placing  = true;
                                }
                            }
                        } else {
                            if (lmb_placing && pending_note)
                                journal.commit(std::make_unique<AddNoteCommand>(song, 0, *pending_note, "Add note"));
                            lmb_placing  = false;
                            pending_note = std::nullopt;
                        }

                    } else if (e.button == GLFW_MOUSE_BUTTON_RIGHT) {
                        rmb_held = e.pressed;
                        if (e.pressed) {
                            auto hit = piano_roll_hit_test(song, piano_roll_panel, e.x, e.y);
                            if (hit)
                                journal.commit(std::make_unique<RemoveNoteCommand>(song, hit->track, hit->note_idx, "Remove note"));
                        }
                    }

                } else if constexpr (std::is_same_v<T, MouseMoveEvent>) {
                    if (mmb_held) {
                        piano_roll_panel.camera.scroll_x = std::max(0.0f, piano_roll_panel.camera.scroll_x - e.dx);
                        piano_roll_panel.camera.scroll_y = std::max(0.0f, piano_roll_panel.camera.scroll_y - e.dy);
                    }
                    if (lmb_placing) {
                        auto note = piano_roll_make_note(song, piano_roll_panel,
                                                         e.x, e.y, snap_ticks, snap_ticks, 100);
                        if (note) pending_note = note;
                    }
                    if (rmb_held) {
                        auto hit = piano_roll_hit_test(song, piano_roll_panel, e.x, e.y);
                        if (hit)
                            journal.commit(std::make_unique<RemoveNoteCommand>(song, hit->track, hit->note_idx, "Remove note"));
                    }

                } else if constexpr (std::is_same_v<T, KeyEvent>) {
                    if (e.action == GLFW_PRESS || e.action == GLFW_REPEAT) {
                        bool ctrl = e.mods & INPUT_MOD_CONTROL;
                        bool shft = e.mods & INPUT_MOD_SHIFT;
                        if (ctrl && e.key == GLFW_KEY_Z)
                            shft ? journal.redo() : journal.undo();
                        else if (e.key == GLFW_KEY_SPACE)
                            engine.toggle();
                    }
                }
            }, event);
        }

        glViewport(0, 0, fb_width, fb_height);
        glClearColor(0.012f, 0.027f, 0.051f, 1.0f); // #03070D
        glClear(GL_COLOR_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);

        nvgBeginFrame(nvg, (float)win_width, (float)win_height, pixel_ratio);
        draw_piano_roll(nvg, song, piano_roll_panel);
        if (lmb_placing && pending_note)
            piano_roll_draw_ghost(nvg, song, piano_roll_panel, *pending_note);
        if (engine.is_playing())
            piano_roll_draw_cursor(nvg, song, piano_roll_panel, engine.cursor_tick());
        nvgEndFrame(nvg);

        glfwSwapBuffers(window.handle);
    }

    nvgDeleteGL3(nvg);
    glfwMakeContextCurrent(nullptr);
}
