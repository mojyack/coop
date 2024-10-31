#pragma once
#include <functional>
#include <thread>

#include "thread-event-pre.hpp"

namespace coop {
template <class Ret, class... Args>
struct [[nodiscard]] run_blocking /* ThreadAdapter */ {
    std::thread                 thread;
    std::function<Ret(Args...)> function;
    std::tuple<Args...>         args;
    Ret                         ret;
    ThreadEvent                 event;

    auto await_ready() const -> bool;
    template <CoHandleLike CoHandle>
    auto await_suspend(CoHandle caller_task) -> void;
    auto await_resume() -> Ret;

    run_blocking(std::function<Ret(Args...)> function, Args... args);
    ~run_blocking();
};
} // namespace coop
