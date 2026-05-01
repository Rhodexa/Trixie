// render.cpp
// Render thread: owns GL context, runs NanoVG, draws the UI.

#include "render.h"
#include "piano_roll.h"
#include "piano_roll_view.h"
#include "input_queue.h"
#include "journal.h"
#include "note_commands.h"

#include <glad/gl.h>
#include <GLFW/glfw3.h>

#define NANOVG_GLFW
#define NANOVG_GL3_IMPLEMENTATION
#include "nanovg_gl.h"
#include "nanovg.h"

#include "box.h"
#include "toolbar.h"

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

    if (nvgCreateFont(nvg, "ui", TRIXIE_ASSETS_DIR "/fonts/JetBrainsMono/fonts/ttf/JetBrainsMono-Regular.ttf") < 0)
        fprintf(stderr, "trixie: failed to load UI font\n");

    PianoRollView view;
    Journal       journal;

    while (running.load()) {
        int fb_width, fb_height;
        glfwGetFramebufferSize(window.handle, &fb_width, &fb_height);

        int win_width, win_height;
        glfwGetWindowSize(window.handle, &win_width, &win_height);
        float pixel_ratio = (float)fb_width / (float)win_width;

        // --- layout (recomputed every frame, free) ---
        Box screen      = { 0.0f, 0.0f, (float)win_width, (float)win_height };
        Box toolbar_box = box_split_top(&screen, 32.0f);
        view.box        = screen;

        if (!view.camera_initialized) {
            float center_y = (127 - 60) * view.camera.zoom_y - view.box.h * 0.5f;
            view.camera.scroll_y      = std::max(0.0f, center_y);
            view.camera_initialized   = true;
        }

        Tick snap_ticks = song.ppq;

        for (const auto& event : input_queue.drain()) {
            std::visit([&](auto&& e) {
                using T = std::decay_t<decltype(e)>;

                if constexpr (std::is_same_v<T, ScrollEvent>) {
                    if (box_contains(view.box, e.mx, e.my)) {
                        bool ctrl  = e.mods & INPUT_MOD_CONTROL;
                        bool shift = e.mods & INPUT_MOD_SHIFT;

                        if (ctrl && shift) {
                            // Horizontal zoom toward cursor
                            float screen_cx = e.mx - view.box.x - view.strip_width;
                            float world_cx  = (screen_cx + view.camera.scroll_x) / view.camera.zoom_x;
                            float factor    = (e.dy > 0.0f) ? 1.15f : (1.0f / 1.15f);
                            view.camera.zoom_x   = std::clamp(view.camera.zoom_x * factor, 10.0f, 1000.0f);
                            view.camera.scroll_x = std::max(0.0f, world_cx * view.camera.zoom_x - screen_cx);

                        } else if (shift) {
                            // Vertical zoom toward cursor
                            float screen_cy = e.my - view.box.y;
                            float world_cy  = (screen_cy + view.camera.scroll_y) / view.camera.zoom_y;
                            float factor    = (e.dy > 0.0f) ? 1.15f : (1.0f / 1.15f);
                            view.camera.zoom_y   = std::clamp(view.camera.zoom_y * factor, 4.0f, 80.0f);
                            view.camera.scroll_y = std::max(0.0f, world_cy * view.camera.zoom_y - screen_cy);

                        } else if (ctrl) {
                            view.camera.scroll_x = std::max(0.0f, view.camera.scroll_x - e.dy * 60.0f);
                        } else {
                            view.camera.scroll_y = std::max(0.0f, view.camera.scroll_y - e.dy * 30.0f);
                        }
                    }

                } else if constexpr (std::is_same_v<T, MouseButtonEvent>) {
                    // Canvas-local coords: origin at strip right edge, view top-left.
                    float cx = e.x - view.box.x - view.strip_width;
                    float cy = e.y - view.box.y;

                    if (e.button == GLFW_MOUSE_BUTTON_MIDDLE) {
                        view.mmb_held = e.pressed;

                    } else if (e.button == GLFW_MOUSE_BUTTON_LEFT) {
                        if (e.pressed) {
                            auto hit = piano_roll_hit_test(song, view, cx, cy);
                            if (hit) {
                                view.lmb_dragging_note = true;
                                view.drag_track        = hit->track;
                                view.drag_note_idx     = hit->note_idx;
                                view.drag_part         = hit->part;
                                view.drag_original     = song.tracks[hit->track].notes[hit->note_idx];
                                Tick cursor_tick       = piano_roll_x_to_tick(song, view, cx, 0);
                                view.drag_tick_offset  = cursor_tick - view.drag_original.start;
                            } else if (!song.tracks.empty()) {
                                auto note = piano_roll_make_note(song, view, cx, cy,
                                                                 snap_ticks, snap_ticks, 100);
                                if (note) { view.pending_note = note; view.lmb_placing = true; }
                            }
                        } else {
                            if (view.lmb_dragging_note) {
                                Note& cur = song.tracks[view.drag_track].notes[view.drag_note_idx];
                                if (cur.start    != view.drag_original.start ||
                                    cur.pitch    != view.drag_original.pitch  ||
                                    cur.duration != view.drag_original.duration) {
                                    Note final = cur;
                                    journal.commit(std::make_unique<EditNoteCommand>(
                                        song, view.drag_track, view.drag_note_idx,
                                        view.drag_original, final,
                                        view.drag_part == NotePart::BODY ? "Move note" :
                                        view.drag_part == NotePart::TAIL ? "Resize note tail" :
                                                                           "Resize note head"));
                                }
                                view.lmb_dragging_note = false;
                            }
                            if (view.lmb_placing && view.pending_note)
                                journal.commit(std::make_unique<AddNoteCommand>(song, 0, *view.pending_note, "Add note"));
                            view.lmb_placing  = false;
                            view.pending_note = std::nullopt;
                        }

                    } else if (e.button == GLFW_MOUSE_BUTTON_RIGHT) {
                        view.rmb_held = e.pressed;
                        if (e.pressed && !view.lmb_dragging_note) {
                            auto hit = piano_roll_hit_test(song, view, cx, cy);
                            if (hit)
                                journal.commit(std::make_unique<RemoveNoteCommand>(
                                    song, hit->track, hit->note_idx, "Remove note"));
                        }
                    }

                } else if constexpr (std::is_same_v<T, MouseMoveEvent>) {
                    float cx = e.x - view.box.x - view.strip_width;
                    float cy = e.y - view.box.y;

                    if (view.mmb_held) {
                        view.camera.scroll_x = std::max(0.0f, view.camera.scroll_x - e.dx);
                        view.camera.scroll_y = std::max(0.0f, view.camera.scroll_y - e.dy);
                    }

                    if (view.lmb_placing) {
                        auto note = piano_roll_make_note(song, view, cx, cy,
                                                         snap_ticks, snap_ticks, 100);
                        if (note) view.pending_note = note;
                    }

                    if (view.lmb_dragging_note) {
                        Note& note         = song.tracks[view.drag_track].notes[view.drag_note_idx];
                        Tick  original_end = view.drag_original.start + view.drag_original.duration;

                        if (view.drag_part == NotePart::BODY) {
                            Tick raw   = piano_roll_x_to_tick(song, view, cx, 0) - view.drag_tick_offset;
                            note.start = std::max(0LL, snap_to_nearest(raw, snap_ticks));
                            note.pitch = piano_roll_y_to_pitch(view, cy);

                        } else if (view.drag_part == NotePart::TAIL) {
                            /*
                                Rhode: ok, let me understand this logic:
                                    * raw_end is where the note's tail is in _ticks_
                                    * snapped_end would be the predicted position of the tail once snapped <- this is my current point of interest
                                        (scrapped) raw_end / snap_ticks does basically a floor. This is kind of a flaw in the logic, let me think, i believe snapping of tail should happen away from 0
                                            Turns out rounding from the midpoint is better.
                                    * min_end makes sense: a note cannot be smaller than this, basically.
                                        Thinking about it, I think there are _two_ minimum ends:
                                            1. this one, which is literally the smallest unit of time
                                            2. the configured minimum.
                                            Let me think.
                                            So, FL Studio, for example, does not limit notes to a minimum tick, instead it has a somewhat larger limit. When a note is _that_ small it turns it into a beat (uses a different rendering style for the note, to signal it cannot be smaller)
                                            This is useful for percussives. Perhaps, the whole start + snap_ticks is enough for now, but this should be able to be even smaller: by holding alt
                                            So there _are_ at least two mins, just like for the tail: where you can have raw and snapped, min can be start + snapping, or start + n_ticks (when holding alt). "n_ticks" is just a placeholder, could be 1, 2, 8... idk. Emperical testing would reveal
                            */
                            Tick raw_end     = piano_roll_x_to_tick(song, view, cx, 0);
                            Tick snapped_end = snap_to_nearest(raw_end, snap_ticks);
                            note.duration    = std::max(snapped_end, note.start + snap_ticks) - note.start;

                        } else { // HEAD
                            Tick snapped  = snap_to_nearest(piano_roll_x_to_tick(song, view, cx, 0), snap_ticks);
                            note.start    = std::clamp(snapped, 0LL, original_end - snap_ticks);
                            note.duration = original_end - note.start;
                        }
                    }

                    if (view.rmb_held && !view.lmb_dragging_note) {
                        auto hit = piano_roll_hit_test(song, view, cx, cy);
                        if (hit)
                            journal.commit(std::make_unique<RemoveNoteCommand>(
                                song, hit->track, hit->note_idx, "Remove note"));
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
        glClearColor(0.012f, 0.027f, 0.051f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);

        Tick cursor = engine.cursor_tick();

        nvgBeginFrame(nvg, (float)win_width, (float)win_height, pixel_ratio);
        draw_piano_roll(nvg, song, view);
        if (view.lmb_placing && view.pending_note)
            piano_roll_draw_ghost(nvg, song, view, *view.pending_note);
        if (engine.is_playing())
            piano_roll_draw_cursor(nvg, song, view, cursor);
        draw_toolbar(nvg, toolbar_box, song, cursor, engine.is_playing());
        nvgEndFrame(nvg);

        glfwSwapBuffers(window.handle);
    }

    nvgDeleteGL3(nvg);
    glfwMakeContextCurrent(nullptr);
}
