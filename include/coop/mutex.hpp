#pragma once
#include "multi-event.hpp"
#include "mutex-pre.hpp"

namespace coop {
inline auto MutexAwaiter::await_ready() const -> bool {
    return !std::exchange(mutex->held, true);
}

template <CoHandleLike CoHandle>
inline auto MutexAwaiter::await_suspend(CoHandle caller_task) -> void {
    mutex->event.await_suspend(std::move(caller_task));
}

inline auto Mutex::lock() -> MutexAwaiter {
    return MutexAwaiter(this);
}

inline auto Mutex::unlock() -> void {
    if(event.waiters.empty()) {
        held = false;
    } else {
        event.notify(1);
    }
}
} // namespace coop
