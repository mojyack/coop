#pragma once
#include <atomic>

namespace coop {
struct AtomicEvent {
    std::atomic_flag flag;

    auto wait() -> void {
        flag.wait(false);
        flag.clear();
    }

    auto notify() -> void {
        flag.test_and_set();
        flag.notify_one();
    }
};
} // namespace coop
