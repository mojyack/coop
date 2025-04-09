#pragma once
#include "generator.hpp"
#include "multi-event-pre.hpp"

namespace coop {
struct Mutex;

struct MutexAwaiter {
    Mutex* mutex;

    auto await_ready() const -> bool;
    template <CoHandleLike CoHandle>
    auto await_suspend(CoHandle caller_task) -> void;
    auto await_resume() const -> void {}
};

struct Mutex {
    MultiEvent event;
    bool       held = false;

    auto lock() -> MutexAwaiter;
    auto unlock() -> void;
};
} // namespace coop

