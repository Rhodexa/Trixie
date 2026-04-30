// playback_engine.h
// Owns the playback thread.
// Walks the Song note list, fires note_on/note_off events to an InstrumentPlugin,
// and exposes a cursor position (in ticks) for the UI to read.
//
// Threading: play()/stop()/toggle() are safe to call from any thread.
// cursor_tick() and is_playing() are atomic reads — safe for the render thread.

#pragma once

#include "song.h"
#include "plugin.h"

#include <atomic>
#include <condition_variable>
#include <mutex>
#include <thread>

class PlaybackEngine {
public:
    PlaybackEngine(const Song& song, InstrumentPlugin& plugin);
    ~PlaybackEngine();

    void play();
    void stop();
    void toggle();

    bool is_playing()  const { return playing_.load(); }
    Tick cursor_tick() const { return cursor_.load(); }

private:
    void    thread_func();
    int64_t tick_to_us(Tick t) const;

    const Song&             song_;
    InstrumentPlugin&       plugin_;

    std::atomic<bool>       playing_{false};
    std::atomic<bool>       stop_requested_{false};
    std::atomic<bool>       quit_{false};
    std::atomic<Tick>       cursor_{0};

    std::mutex              mutex_;
    std::condition_variable cv_;
    std::thread             thread_;
};
