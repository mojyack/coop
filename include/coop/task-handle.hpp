#pragma once
#include "runner.hpp"
#include "task-handle-pre.hpp"

namespace coop {
inline auto TaskJoiner::await_ready() const -> bool {
    return handle->task == nullptr;
}

template <CoHandleLike CoHandle>
inline auto TaskJoiner::await_suspend(CoHandle caller_task) -> void {
    auto& runner = *caller_task.promise().runner;
    runner.join(*handle);
}

inline auto TaskJoiner::await_resume() const -> void {
}

inline TaskJoiner::TaskJoiner(TaskHandle& handle)
    : handle(&handle) {
}

inline auto TaskHandle::cancel() -> bool {
    return runner->cancel_task(*this);
}

inline auto TaskHandle::dissociate() -> void {
    if(task == nullptr) {
        return;
    }
    task->user_handle = nullptr;
    task              = nullptr;
}

inline auto TaskHandle::join() -> TaskJoiner {
    return TaskJoiner(*this);
}
} // namespace coop
