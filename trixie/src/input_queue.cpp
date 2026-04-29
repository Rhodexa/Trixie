// input_queue.cpp
// InputQueue implementation.

#include "input_queue.h"

void InputQueue::push(const InputEvent& event) {
    std::lock_guard<std::mutex> lock(mutex_);
    pending_.push_back(event);
}

std::vector<InputEvent> InputQueue::drain() {
    std::lock_guard<std::mutex> lock(mutex_);
    std::vector<InputEvent> out;
    out.swap(pending_); // take ownership atomically, release lock immediately
    return out;
}
