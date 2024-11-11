#pragma once
#include <functional>
#include <thread>

#include "thread-event-pre.hpp"

namespace coop {
template <class Ret, class... Args>
struct [[nodiscard]] run_blocking /* ThreadAdapter */ {
    constexpr static auto void_ret = std::is_same_v<Ret, void>;
    using RetStorage               = std::conditional_t<!void_ret, Ret, std::tuple<>>;

    std::thread                      thread;
    std::function<Ret(Args...)>      function;
    std::tuple<Args...>              args;
    ThreadEvent                      event;
    [[no_unique_address]] RetStorage ret;

    auto await_ready() const -> bool;
    template <CoHandleLike CoHandle>
    auto await_suspend(CoHandle caller_task) -> void;
    auto await_resume() -> Ret;

    run_blocking(std::function<Ret(Args...)> function, Args... args);
    ~run_blocking();
};
} // namespace coop
