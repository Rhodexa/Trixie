// render.h
// Render thread interface.
// The render thread owns the OpenGL context and NanoVG for its entire lifetime.

#pragma once

#include "window.h"
#include "song.h"
#include "input_queue.h"
#include "playback_engine.h"
#include <atomic>

// Entry point for the render thread.
// Takes ownership of the OpenGL context on entry.
void render_thread_run(Window& window, std::atomic<bool>& running, Song& song, InputQueue& input_queue, PlaybackEngine& engine);
