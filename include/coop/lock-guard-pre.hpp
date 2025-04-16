#pragma once
#include "generator.hpp"
#include "mutex-pre.hpp"

namespace coop {
struct LockGuard {
    Mutex* mutex = nullptr;

    static auto lock(Mutex& mutex) -> coop::Async<LockGuard>;

    auto unlock() -> void;

    auto operator=(LockGuard&) -> LockGuard& = delete;
    auto operator=(LockGuard&&) -> LockGuard&;
    LockGuard()           = default;
    LockGuard(LockGuard&) = delete;
    LockGuard(LockGuard&&);
    ~LockGuard();
};
} // namespace coop
