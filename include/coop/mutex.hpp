#pragma once
#include "multi-event.hpp"
#include "mutex-pre.hpp"
#include "promise.hpp"

namespace coop {
inline auto Mutex::lock() -> coop::Async<void> {
    if(held) {
        co_await event;
    }
    held = true;
}

inline auto Mutex::unlock() -> void {
    held = false;
    event.notify(1);
}
} // namespace coop
