#pragma once
#include <coroutine>

namespace coop {
struct Runner;

template <class T>
concept CoHandleLike = requires(T handle) {
    std::is_same_v<Runner*, decltype(handle.promise().runner)>;
    std::is_convertible_v<T, std::coroutine_handle<>>;
};
} // namespace coop
