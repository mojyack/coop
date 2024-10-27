#pragma once
#include <coroutine>
#include <exception>
#include <utility>

#include "generator.hpp"
#include "runner-pre.hpp"

namespace coop {
struct PromiseBase {
    Runner* runner = nullptr;

    auto initial_suspend() -> std::suspend_always {
        return {};
    }

    auto final_suspend() noexcept -> std::suspend_always {
        return {};
    }

    auto unhandled_exception() -> void {
        std::terminate();
    }
};

template <class T>
struct Promise : PromiseBase {
    T data;

    auto get_return_object() -> CoGenerator<T> {
        return CoGenerator(*this);
    }

    auto yield_value(T data) -> std::suspend_always {
        this->data = std::move(data);
        return {};
    }

    auto return_value(T data) -> void {
        this->data = std::move(data);
    }
};

template <>
struct Promise<void> : PromiseBase {
    auto get_return_object() -> CoGenerator<void> {
        return CoGenerator(*this);
    }

    auto return_void() -> void {
    }
};
} // namespace coop
