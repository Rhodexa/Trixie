// window.cpp
// GLFW window creation and context handoff.

#include "window.h"

#include <cstdio>

bool window_create(Window& window, int width, int height, const char* title) {
    if (!glfwInit()) {
        fprintf(stderr, "trixie: glfwInit failed\n");
        return false;
    }

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    window.handle = glfwCreateWindow(width, height, title, nullptr, nullptr);
    if (!window.handle) {
        fprintf(stderr, "trixie: failed to create window\n");
        glfwTerminate();
        return false;
    }

    window.width  = width;
    window.height = height;

    glfwMakeContextCurrent(window.handle);
    return true;
}

void window_release_context() {
    // GLFW releases the context from whichever thread calls this.
    // Passing nullptr detaches without binding a new one.
    glfwMakeContextCurrent(nullptr);
}

void window_destroy(Window& window) {
    glfwDestroyWindow(window.handle);
    glfwTerminate();
    window.handle = nullptr;
}
