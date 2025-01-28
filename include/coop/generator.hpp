#pragma once
#include <coroutine>
#include <utility>

#include "cohandle.hpp"
#include "promise-pre.hpp"

namespace coop {
template <class T>
struct Promise;

template <class T>
struct [[nodiscard]] CoGenerator {
    using promise_type = Promise<T>;

    std::coroutine_handle<promise_type> handle;

    auto await_ready() const -> bool {
        return false;
    }

    template <CoHandleLike CoHandle>
    auto await_suspend(CoHandle caller_task) const -> void {
        auto& runner = *caller_task.promise().runner;
        runner.push_task(false, false, {}, handle);
    }

    auto await_resume() const -> decltype(auto) {
        auto& promise = handle.promise();
        if constexpr(PromiseWithRetValue<decltype(promise)>) {
            return handle.promise().data;
        } else {
            return;
        }
    }

    auto operator=(CoGenerator&) -> CoGenerator& = delete;

    auto operator=(CoGenerator&& other) -> CoGenerator& {
        handle = std::exchange(other.handle, nullptr);
        return *this;
    }

    CoGenerator() = default;

    CoGenerator(CoGenerator&) = delete;

    CoGenerator(CoGenerator&& o)
        : handle(std::exchange(o.handle, nullptr)) {
    }

    CoGenerator(promise_type& promise)
        : handle(std::coroutine_handle<promise_type>::from_promise(promise)) {
    }

    ~CoGenerator() {
        if(handle != nullptr) {
            handle.destroy();
        }
    }
};

template <class T>
using Async = CoGenerator<T>;
} // namespace coop
