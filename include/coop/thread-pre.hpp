#pragma once
#include <atomic>
#include <functional>
#include <thread>
#include <tuple>
#include <type_traits>

#include "cohandle.hpp"
#include "thread-event-pre.hpp"

namespace coop {
struct Thread;

template <class Functor>
struct [[nodiscard]] ThreadRunAdapter {
    using Ret                      = std::invoke_result_t<Functor>;
    constexpr static auto void_ret = std::is_same_v<Ret, void>;
    using RetStorage               = std::conditional_t<!void_ret, Ret, std::tuple<>>;

    Thread*                          thread;
    Functor                          functor;
    [[no_unique_address]] RetStorage ret;

    auto await_ready() const -> bool;
    template <CoHandleLike CoHandle>
    auto await_suspend(CoHandle caller_task) -> void;
    auto await_resume() -> Ret;
};

struct [[nodiscard]] Thread {
    std::thread           thread;
    std::atomic_flag      wakeup;
    std::atomic_flag      stopping;
    std::function<void()> task;
    ThreadEvent           event;

    auto loop() -> void;

    template <class Functor>
    [[nodiscard]] auto run(Functor functor) -> ThreadRunAdapter<Functor>;

    Thread(std::function<void()> init = {});
    ~Thread();
};
} // namespace coop
