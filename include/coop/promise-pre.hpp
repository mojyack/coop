#pragma once

namespace coop {
template <class T>
concept PromiseWithRetValue = requires(T promise) {
    { promise.data };
};
} // namespace coop
