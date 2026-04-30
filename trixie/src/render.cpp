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

    // --- input state ---
    bool mmb_held    = false;
    bool rmb_held    = false;

    // LMB: place new note
    bool lmb_placing = false;
    std::optional<Note> pending_note;

    // LMB: drag existing note (head / body / tail)
    bool     lmb_dragging_note = false;
    int      drag_track        = -1;
    int      drag_note_idx     = -1;
    NotePart drag_part         = NotePart::BODY;
    Note     drag_original     = {};
    Tick     drag_tick_offset  = 0; // cursor tick – note.start at grab time (body drag only)

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
                            if (hit) {
                                // Grab an existing note for drag
                                lmb_dragging_note = true;
                                drag_track        = hit->track;
                                drag_note_idx     = hit->note_idx;
                                drag_part         = hit->part;
                                drag_original     = song.tracks[hit->track].notes[hit->note_idx];
                                // Body drag: preserve relative grab offset within the note
                                Tick cursor_tick  = piano_roll_x_to_tick(song, piano_roll_panel, e.x, 0);
                                drag_tick_offset  = cursor_tick - drag_original.start;
                            } else if (!song.tracks.empty()) {
                                // Start placing a new note
                                auto note = piano_roll_make_note(song, piano_roll_panel,
                                                                 e.x, e.y, snap_ticks, snap_ticks, 100);
                                if (note) { pending_note = note; lmb_placing = true; }
                            }
                        } else {
                            // LMB release — commit whichever operation was in flight
                            if (lmb_dragging_note) {
                                Note& cur = song.tracks[drag_track].notes[drag_note_idx];
                                if (cur.start != drag_original.start ||
                                    cur.pitch != drag_original.pitch  ||
                                    cur.duration != drag_original.duration) {
                                    Note final = cur;
                                    journal.commit(std::make_unique<EditNoteCommand>(
                                        song, drag_track, drag_note_idx,
                                        drag_original, final,
                                        drag_part == NotePart::BODY ? "Move note" :
                                        drag_part == NotePart::TAIL ? "Resize note tail" :
                                                                       "Resize note head"));
                                }
                                lmb_dragging_note = false;
                            }
                            if (lmb_placing && pending_note)
                                journal.commit(std::make_unique<AddNoteCommand>(song, 0, *pending_note, "Add note"));
                            lmb_placing  = false;
                            pending_note = std::nullopt;
                        }

                    } else if (e.button == GLFW_MOUSE_BUTTON_RIGHT) {
                        rmb_held = e.pressed;
                        if (e.pressed && !lmb_dragging_note) {
                            auto hit = piano_roll_hit_test(song, piano_roll_panel, e.x, e.y);
                            if (hit)
                                journal.commit(std::make_unique<RemoveNoteCommand>(
                                    song, hit->track, hit->note_idx, "Remove note"));
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

                    if (lmb_dragging_note) {
                        Note& note         = song.tracks[drag_track].notes[drag_note_idx];
                        Tick  original_end = drag_original.start + drag_original.duration;

                        if (drag_part == NotePart::BODY) {
                            Tick raw   = piano_roll_x_to_tick(song, piano_roll_panel, e.x, 0) - drag_tick_offset;
                            note.start = std::max(0LL, snap_to_nearest(raw, snap_ticks));
                            note.pitch = piano_roll_y_to_pitch(piano_roll_panel, e.y);

                        } else if (drag_part == NotePart::TAIL) {
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
                            Tick raw_end     = piano_roll_x_to_tick(song, piano_roll_panel, e.x, 0);
                            Tick snapped_end = snap_to_nearest(raw_end, snap_ticks);
                            note.duration    = std::max(snapped_end, note.start + snap_ticks) - note.start;

                        } else { // HEAD
                            Tick snapped  = snap_to_nearest(piano_roll_x_to_tick(song, piano_roll_panel, e.x, 0), snap_ticks);
                            note.start    = std::clamp(snapped, 0LL, original_end - snap_ticks);
                            note.duration = original_end - note.start;
                        }
                    }

                    if (rmb_held && !lmb_dragging_note) {
                        auto hit = piano_roll_hit_test(song, piano_roll_panel, e.x, e.y);
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
