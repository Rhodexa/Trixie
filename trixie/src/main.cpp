// main.cpp
// Trixie entry point.
// Loads the MIDI file, opens the window, registers GLFW input callbacks,
// then sits in the OS event loop until the window closes.
// All input events are pushed onto the InputQueue for the render thread to consume.

#include "window.h"
#include "render.h"
#include "midi_loader.h"
#include "input_queue.h"

#include <atomic>
#include <thread>
#include <cstdio>

// Tracks mouse state needed across multiple callbacks.
struct InputHandler {
    InputQueue* queue        = nullptr;
    float       last_mouse_x = 0.0f;
    float       last_mouse_y = 0.0f;
};

int main(int argc, char* argv[]) {
    if (argc < 2) {
        fprintf(stderr, "usage: trixie <midi-file.mid>\n");
        return 1;
    }

    Song song = load_midi(argv[1]);
    if (song.tracks.empty()) {
        fprintf(stderr, "trixie: no notes found in %s\n", argv[1]);
        return 1;
    }

    Window window = {};
    if (!window_create(window, 1280, 720, "Trixie")) {
        return 1;
    }

    InputQueue   input_queue;
    InputHandler input_handler;
    input_handler.queue = &input_queue;

    glfwSetWindowUserPointer(window.handle, &input_handler);

    // Cursor position — updates cached position and pushes a move event.
    glfwSetCursorPosCallback(window.handle, [](GLFWwindow* w, double x, double y) {
        auto* h = static_cast<InputHandler*>(glfwGetWindowUserPointer(w));
        float fx = (float)x, fy = (float)y;
        float dx = fx - h->last_mouse_x;
        float dy = fy - h->last_mouse_y;
        h->last_mouse_x = fx;
        h->last_mouse_y = fy;
        h->queue->push(MouseMoveEvent{ fx, fy, dx, dy });
    });

    // Scroll wheel — reads modifier keys at the moment of scroll.
    glfwSetScrollCallback(window.handle, [](GLFWwindow* w, double dx, double dy) {
        auto* h = static_cast<InputHandler*>(glfwGetWindowUserPointer(w));
        int mods = 0;
        if (glfwGetKey(w, GLFW_KEY_LEFT_CONTROL)  == GLFW_PRESS ||
            glfwGetKey(w, GLFW_KEY_RIGHT_CONTROL) == GLFW_PRESS)
            mods |= INPUT_MOD_CONTROL;
        if (glfwGetKey(w, GLFW_KEY_LEFT_SHIFT)  == GLFW_PRESS ||
            glfwGetKey(w, GLFW_KEY_RIGHT_SHIFT) == GLFW_PRESS)
            mods |= INPUT_MOD_SHIFT;
        h->queue->push(ScrollEvent{
            h->last_mouse_x, h->last_mouse_y,
            (float)dx, (float)dy, mods
        });
    });

    // Mouse buttons.
    glfwSetMouseButtonCallback(window.handle, [](GLFWwindow* w, int button, int action, int mods) {
        auto* h = static_cast<InputHandler*>(glfwGetWindowUserPointer(w));
        h->queue->push(MouseButtonEvent{
            button, action == GLFW_PRESS,
            h->last_mouse_x, h->last_mouse_y, mods
        });
    });

    // Keyboard — Ctrl+Z / Ctrl+Y undo-redo and any future shortcuts.
    glfwSetKeyCallback(window.handle, [](GLFWwindow* w, int key, int /*scancode*/, int action, int mods) {
        auto* h = static_cast<InputHandler*>(glfwGetWindowUserPointer(w));
        h->queue->push(KeyEvent{ key, action, mods });
    });

    std::atomic<bool> running{true};

    window_release_context();
    std::thread render_thread(render_thread_run,
                              std::ref(window),
                              std::ref(running),
                              std::ref(song),
                              std::ref(input_queue));

    while (!glfwWindowShouldClose(window.handle)) {
        glfwWaitEvents();
    }

    running.store(false);
    render_thread.join();

    window_destroy(window);
    return 0;
}
