// input_queue.h
// Thread-safe queue for input events flowing from the OS event thread
// to the render thread.
//
// The main thread pushes events from GLFW callbacks.
// The render thread drains them once per frame and routes them to panels.
// Using std::variant so all event types travel through one queue.

#pragma once

#include <mutex>
#include <variant>
#include <vector>

// Modifier flags — match GLFW_MOD_* values for direct use with GLFW.
enum InputMod {
    INPUT_MOD_SHIFT   = 0x0001,
    INPUT_MOD_CONTROL = 0x0002,
    INPUT_MOD_ALT     = 0x0004,
};

struct ScrollEvent {
    float mx, my;  // mouse position at time of scroll
    float dx, dy;  // scroll delta (dy is the main wheel axis)
    int   mods;    // InputMod flags
};

struct MouseMoveEvent {
    float x,  y;   // current mouse position
    float dx, dy;  // delta from previous position
};

struct MouseButtonEvent {
    int   button;  // GLFW button index (e.g. GLFW_MOUSE_BUTTON_MIDDLE)
    bool  pressed;
    float x, y;    // mouse position at time of event
    int   mods;    // InputMod flags
};

struct KeyEvent {
    int key;    // GLFW_KEY_*
    int action; // GLFW_PRESS, GLFW_REPEAT, GLFW_RELEASE
    int mods;   // InputMod flags
};

using InputEvent = std::variant<ScrollEvent, MouseMoveEvent, MouseButtonEvent, KeyEvent>;

class InputQueue {
public:
    void push(const InputEvent& event);

    // Returns all pending events and clears the queue atomically.
    // The render thread calls this once per frame.
    std::vector<InputEvent> drain();

private:
    std::mutex           mutex_;
    std::vector<InputEvent> pending_;
};
