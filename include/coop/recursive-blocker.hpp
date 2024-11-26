#pragma once
#include "blocker.hpp"
#include "recursive-blocker-pre.hpp"

namespace coop {
inline auto RecursiveBlocker::block() -> void {
    if(lock_count.fetch_add(1) == 0) {
        Blocker::block();
    }
    mutex.lock();
}

inline auto RecursiveBlocker::unblock() -> void {
    mutex.unlock();
    if(lock_count.fetch_sub(1) == 1) {
        Blocker::unblock();
    }
}
} // namespace coop
