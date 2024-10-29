#pragma once
#include <concepts>
#include <functional>

#include "thread-event.hpp"
#include "thread-pre.hpp"

namespace coop {
template <class Ret, class... Args>
auto run_blocking<Ret, Args...>::await_ready() const -> bool {
    return false;
}

template <class Ret, class... Args>
template <CoHandleLike CoHandle>
auto run_blocking<Ret, Args...>::await_suspend(CoHandle caller_task) -> void {
    thread = std::thread([this]() {
        ret = std::apply(
            [this](auto... args) {
                return function(std::forward<Args>(args)...);
            },
            args);
        event.notify();
    });
    event.await_suspend(caller_task);
}

template <class Ret, class... Args>
auto run_blocking<Ret, Args...>::await_resume() -> Ret {
    // TODO: check result.error
    return std::move(ret);
}

template <class Ret, class... Args>
run_blocking<Ret, Args...>::run_blocking(std::function<Ret(Args...)> function, Args... args)
    : function(function),
      args(std::forward<Args>(args)...) {
}

template <class Ret, class... Args>
run_blocking<Ret, Args...>::~run_blocking() {
    if(thread.joinable()) {
        thread.join();
    }
}

template <class Func, class... Args>
    requires std::invocable<Func, Args...>
run_blocking(Func function, Args... args)
    -> run_blocking<std::invoke_result_t<Func, Args...>, Args...>;
} // namespace coop
