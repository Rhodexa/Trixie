// window.h
// GLFW window and OpenGL context management.
//
// All GLFW calls that create, query, or destroy the window must happen
// on the main (OS event) thread — GLFW requires this on Windows.
// The OpenGL context is created here and then handed off to the render
// thread via window_release_context().

#pragma once

#include <GLFW/glfw3.h>

struct Window {
    GLFWwindow* handle = nullptr;
    int         width  = 0;
    int         height = 0;
};

// Creates the GLFW window and an OpenGL 3.3 core context.
// The context is current on the calling thread after this returns.
// Returns false and prints to stderr if anything fails.
bool window_create(Window& window, int width, int height, const char* title);

// Detaches the OpenGL context from the calling thread so the render
// thread can take ownership of it.
void window_release_context();

// Destroys the window and terminates GLFW.
// Only call this after the render thread has fully stopped.
void window_destroy(Window& window);
