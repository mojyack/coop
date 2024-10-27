#pragma once
#include <concepts>
#include <thread>

#include <sys/eventfd.h>
#include <unistd.h>

#include "io-pre.hpp"
#include "runner-pre.hpp"

namespace coop {
struct ThreadEvent {
    int fd = -1;

    ThreadEvent(ThreadEvent&& o)
        : fd(std::exchange(o.fd, -1)) {
    }

    ThreadEvent()
        : fd(eventfd(0, 0)) {
    }

    ~ThreadEvent() {
        if(fd != -1) {
            close(fd);
        }
    }
};

template <class Ret, class... Args>
struct run_blocking /* ThreadAdapter */ {
    Runner*                     runner;
    std::thread                 thread;
    std::function<Ret(Args...)> function;
    std::tuple<Args...>         args;
    Ret                         ret;
    IOWaitResult                result;
    ThreadEvent                 event;

    auto await_ready() const -> bool;
    template <CoHandleLike CoHandle>
    auto await_suspend(CoHandle caller_task) -> void;
    auto await_resume() -> Ret;

    run_blocking(std::function<Ret(Args...)> function, Args... args);
    ~run_blocking();
};

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
        eventfd_write(event.fd, 1);
    });
    runner = caller_task.promise().runner;
    runner->io_wait(event.fd, true, false, result);
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
