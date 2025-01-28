#pragma once
#include <concepts>
#include <functional>

#include "thread-event.hpp"
#include "thread-pre.hpp"

namespace coop {
template <class Ret, class... Args>
auto ThreadAdapter<Ret, Args...>::await_ready() const -> bool {
    return false;
}

template <class Ret, class... Args>
template <CoHandleLike CoHandle>
auto ThreadAdapter<Ret, Args...>::await_suspend(CoHandle caller_task) -> void {
    thread = std::thread([this]() {
        if constexpr(!void_ret) {
            ret = std::apply([this](auto... args) { return function(std::forward<Args>(args)...); }, args);
        } else {
            std::apply([this](auto... args) { function(std::forward<Args>(args)...); }, args);
        }
        event.notify();
    });
    event.await_suspend(caller_task);
}

template <class Ret, class... Args>
auto ThreadAdapter<Ret, Args...>::await_resume() -> Ret {
    // TODO: check result.error
    if constexpr(!void_ret) {
        return std::move(ret);
    }
}

template <class Ret, class... Args>
ThreadAdapter<Ret, Args...>::ThreadAdapter(std::function<Ret(Args...)> function, Args... args)
    : function(function),
      args(std::forward<Args>(args)...) {
}

template <class Ret, class... Args>
ThreadAdapter<Ret, Args...>::~ThreadAdapter() {
    if(thread.joinable()) {
        thread.join();
    }
}

template <class Func, class... Args>
// requires std::invocable<Func, Args...> // crashes clang
ThreadAdapter(Func function, Args... args)
    -> ThreadAdapter<std::invoke_result_t<Func, Args...>, Args...>;
} // namespace coop
