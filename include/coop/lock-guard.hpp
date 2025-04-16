#pragma once
#include "lock-guard-pre.hpp"
#include "mutex.hpp"
#include "promise.hpp"

namespace coop {
inline auto LockGuard::lock(Mutex& mutex) -> coop::Async<LockGuard> {
    co_await mutex.lock();
    auto lock  = LockGuard();
    lock.mutex = &mutex;
    co_return lock;
}

inline auto LockGuard::unlock() -> void {
    if(mutex != nullptr) {
        mutex->unlock();
        mutex = nullptr;
    }
}

inline auto LockGuard::operator=(LockGuard&& other) -> LockGuard& {
    unlock();
    std::swap(mutex, other.mutex);
    return *this;
}

inline LockGuard::LockGuard(LockGuard&& other) {
    *this = std::move(other);
}

inline LockGuard::~LockGuard() {
    unlock();
}

} // namespace coop
