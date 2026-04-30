// playback_engine.cpp

#include "playback_engine.h"

#include <algorithm>
#include <chrono>
#include <vector>

// Internal event: one note-on or note-off with its scheduled tick.
struct PlayEvent {
    Tick tick;
    bool on;
    int  channel;
    int  pitch;
    int  velocity;
};

// ---------------------------------------------------------------------------

PlaybackEngine::PlaybackEngine(const Song& song, InstrumentPlugin& plugin)
    : song_(song), plugin_(plugin)
{
    thread_ = std::thread(&PlaybackEngine::thread_func, this);
}

PlaybackEngine::~PlaybackEngine() {
    {
        std::lock_guard<std::mutex> lk(mutex_);
        quit_.store(true);
    }
    cv_.notify_all();
    thread_.join();
}

void PlaybackEngine::play() {
    // Stop any in-progress playback first.
    if (playing_.load()) {
        stop_requested_.store(true);
        while (playing_.load())
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    stop_requested_.store(false);
    cursor_.store(0);
    {
        std::lock_guard<std::mutex> lk(mutex_);
        playing_.store(true);
    }
    cv_.notify_one();
}

void PlaybackEngine::stop() {
    stop_requested_.store(true);
}

void PlaybackEngine::toggle() {
    is_playing() ? stop() : play();
}

int64_t PlaybackEngine::tick_to_us(Tick t) const {
    // microseconds = ticks × (60,000,000 / (ppq × bpm))
    return (int64_t)((double)t * 60'000'000.0 / song_.ppq / song_.bpm);
}

void PlaybackEngine::thread_func() {
    while (!quit_.load()) {
        // Sleep until play() wakes us.
        {
            std::unique_lock<std::mutex> lk(mutex_);
            cv_.wait(lk, [this] { return playing_.load() || quit_.load(); });
        }
        if (quit_.load()) break;

        // Snapshot the note list so edits during playback don't affect us.
        std::vector<PlayEvent> events;
        for (const auto& track : song_.tracks) {
            for (const auto& n : track.notes) {
                events.push_back({ n.start,              true,  n.channel, n.pitch, n.velocity });
                events.push_back({ n.start + n.duration, false, n.channel, n.pitch, 0 });
            }
        }
        // Sort by tick; note-offs before note-ons at the same tick (avoid instant cut-off).
        std::sort(events.begin(), events.end(), [](const PlayEvent& a, const PlayEvent& b) {
            if (a.tick != b.tick) return a.tick < b.tick;
            return !a.on && b.on; // note-off first
        });

        auto start_time = std::chrono::steady_clock::now();

        for (const auto& ev : events) {
            auto fire_time = start_time + std::chrono::microseconds(tick_to_us(ev.tick));

            // Interruptible sleep: poll every 1 ms and update the visible cursor.
            while (!stop_requested_.load() && !quit_.load()) {
                auto now = std::chrono::steady_clock::now();
                if (now >= fire_time) break;
                auto remaining = fire_time - now;
                auto slice     = std::min(remaining, std::chrono::nanoseconds(1'000'000LL));
                std::this_thread::sleep_for(slice);

                // Smooth cursor update while waiting for the next event.
                int64_t elapsed_us = std::chrono::duration_cast<std::chrono::microseconds>(
                    std::chrono::steady_clock::now() - start_time).count();
                cursor_.store((Tick)((double)elapsed_us * song_.ppq * song_.bpm / 60'000'000.0));
            }
            if (stop_requested_.load() || quit_.load()) break;

            cursor_.store(ev.tick);
            if (ev.on)
                plugin_.note_on(ev.channel, ev.pitch, ev.velocity);
            else
                plugin_.note_off(ev.channel, ev.pitch);
        }

        plugin_.reset();
        playing_.store(false);
        stop_requested_.store(false);
        cursor_.store(0);
    }
}
