#pragma once
#include <mutex>

#include "blocker-pre.hpp"

namespace coop {
struct RecursiveBlocker : Blocker {
    std::atomic_size_t lock_count;
    std::mutex         mutex;

    auto block() -> void;
    auto unblock() -> void;
};
} // namespace coop
