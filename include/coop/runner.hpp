#pragma once
#include <cstring>
#include <iostream>
#include <thread>
#include <vector>

#if !defined(_WIN32)
#include "poll.h"
#endif

#include "io-pre.hpp"
#include "multi-event-pre.hpp"
#include "runner-pre.hpp"
#include "single-event-pre.hpp"

namespace coop {
namespace impl {
template <class... Args>
inline auto debug(Args... args) -> void {
#ifdef COOP_DEBUG
    (std::cout << ... << args) << std::endl;
#else
    ((void)args, ...);
#endif
}

template <class... Args>
inline auto error(Args... args) -> void {
    (std::cout << ... << args) << std::endl;
}

inline auto remove_task_child(Task& child) -> bool {
    auto& children = child.parent->children;
    for(auto i = children.begin(); i != children.end(); i = std::next(i)) {
        auto& current = *i;
        if(&current == &child) {
            children.erase(i);
            return true;
        }
    }
    return false;
}

struct CollectContext {
    std::vector<Task*>                    result;
    std::vector<Task*>                    poll_tasks;
    std::vector<pollfd>                   poll_fds;
    std::chrono::system_clock::time_point now   = std::chrono::system_clock::now();
    std::chrono::system_clock::duration   sleep = std::chrono::system_clock::duration::max();
};

inline auto collect_resumable_tasks(Task& task, CollectContext& context) -> void {
    if(!task.children.empty()) {
        for(auto& child : task.children) {
            collect_resumable_tasks(child, context);
        }
        return;
    }

    auto& reason = task.suspend_reason;
    switch(reason.index()) {
    case 0: // Running
        context.result.push_back(&task);
        break;
    case 1: { // ByTimer
        auto& timer = std::get<1>(reason);
        if(timer.suspend_until < context.now) {
            reason.emplace<Running>();
            context.result.push_back(&task);
        } else {
            context.sleep = std::min(context.sleep, timer.suspend_until - context.now);
        }
    } break;
    case 2: // BySingleEvent
        break;
    case 3: // ByMultiEvent
        break;
    case 4: { // ByIO
        auto& io = std::get<4>(reason);
        context.poll_fds.emplace_back(pollfd{io.file, short((io.read ? POLLIN : 0) | (io.write ? POLLOUT : 0)), 0});
        context.poll_tasks.push_back(&task);
    } break;
    default:
        impl::error("bug index=", reason.index());
        break;
    }
}

inline auto revents_to_io_result(const short revents) -> IOWaitResult {
    return IOWaitResult{
        .read  = bool(revents & POLLIN),
        .write = bool(revents & POLLOUT),
        .error = bool(revents & (POLLHUP | POLLERR)),
    };
}
} // namespace impl

inline auto TaskHandle::cancel() -> bool {
    return runner->cancel_task(*this);
}

inline auto Runner::run_tasks(const std::span<Task*> tasks) -> void {
    for(auto& task : tasks) {
        impl::debug("resuming task=", task, " handle=", task->handle.address());
        current_task = task;
        task->handle.resume();
        impl::debug("task done");
        if(task->handle.done()) {
            destroy_task(*task);
        }
    }
}

inline auto Runner::push_task(const bool independent, const std::span<TaskHandle* const> user_handles, const std::span<Task> tasks) -> void {
    if(!user_handles.empty() && user_handles.size() != tasks.size()) {
        impl::error(__LINE__, " bug");
        return;
    }

    const auto parent = independent ? &root : current_task;
    for(auto i = 0u; i < tasks.size(); i += 1) {
        auto& task  = parent->children.emplace_back(tasks[i]);
        task.parent = parent;
        if(user_handles.empty() || user_handles[i] == nullptr) {
            continue;
        }
        auto& handle     = *user_handles[i];
        task.user_handle = &handle;
        handle           = TaskHandle{.task = &task, .runner = this, .destroyed = false};
    }
}

template <CoHandleLike... CoHandles>
inline auto Runner::push_task(const bool independent, const bool transfer_handle, std::span<TaskHandle* const> user_handles, CoHandles... co_handles) -> void {
    ([this, &co_handles]() { co_handles.promise().runner = this; }(), ...);
    auto tasks = std::array{Task{.handle = co_handles, .handle_owned = transfer_handle}...};
    return push_task(independent, user_handles, tasks);
}

template <CoHandleLike CoHandle>
inline auto Runner::push_task(const bool independent, const bool transfer_handle, std::span<TaskHandle* const> user_handles, const std::span<CoHandle> co_handles) -> void {
    auto tasks = std::vector<Task>();
    for(auto& handle : co_handles) {
        handle.promise().runner = this;
        tasks.emplace_back(Task{.handle = handle, .handle_owned = transfer_handle});
    }
    return push_task(independent, user_handles, tasks);
}

template <CoGeneratorLike... Generators>
inline auto Runner::push_task(Generators... generators) -> void {
    push_task({}, std::forward<Generators>(generators)...);
}

template <CoGeneratorLike... Generators>
inline auto Runner::push_task(const std::span<TaskHandle* const> user_handles, Generators... generators) -> void {
    push_task(true, true, user_handles, generators.handle...);
    ([&generators]() { generators.handle = nullptr; }(), ...);
}

inline auto Runner::destroy_task(Task& task) -> bool {
    // impl::debug("destroy task=", &task, " handle=", task.handle.address());
    if(task.handle_owned) {
        task.handle.destroy();
    }
    if(task.user_handle != nullptr) {
        task.user_handle->task      = nullptr;
        task.user_handle->destroyed = true;
    }
    switch(task.suspend_reason.index()) {
    case 2: {
        auto& event  = std::get<2>(task.suspend_reason);
        auto& waiter = event.event->waiter;
        if(waiter == &task) {
            waiter = nullptr;
        } else {
            impl::error(__LINE__, " bug: task=", &task);
        }
    } break;
    case 3: {
        auto& event   = std::get<3>(task.suspend_reason);
        auto& waiters = event.event->waiters;
        auto  iter    = std::ranges::find(waiters, &task);
        if(iter != waiters.end()) {
            waiters.erase(iter);
        } else {
            impl::error(__LINE__, " bug: task=", &task);
        }
    } break;
    }
    if(!impl::remove_task_child(task)) {
        impl::error(__LINE__, " bug: parent=", task.parent, " child=", &task);
    }
    return true;
}

inline auto Runner::cancel_task(TaskHandle& handle) -> bool {
    if(handle.task == nullptr || handle.destroyed) {
        return true;
    }

    return destroy_task(*handle.task);
}

inline auto Runner::delay(const std::chrono::system_clock::duration duration) -> void {
    // impl::debug("delay task=", current_task, " duratoin=", duration);
    current_task->suspend_reason.emplace<ByTimer>(std::chrono::system_clock::now() + duration);
}

inline auto Runner::event_wait(SingleEvent& event) -> void {
    // impl::debug("wait ", &event, " task=", current_task);
    current_task->suspend_reason.emplace<BySingleEvent>(&event);
    event.waiter = current_task;
}

inline auto Runner::event_notify(SingleEvent& event) -> void {
    // impl::debug("notify ", &event, " size=", event.waiters.size());
    const auto task = event.waiter;
    if(std::get_if<BySingleEvent>(&task->suspend_reason) == nullptr) {
        impl::error(__LINE__, " bug: task=", task, " index=", task->suspend_reason.index());
        return;
    }
    task->suspend_reason.emplace<Running>();
    event.waiter = nullptr;
}

inline auto Runner::event_wait(MultiEvent& event) -> void {
    // impl::debug("wait ", &event, " task=", current_task);
    current_task->suspend_reason.emplace<ByMultiEvent>(&event);
    event.waiters.push_back(current_task);
}

inline auto Runner::event_notify(MultiEvent& event) -> void {
    // impl::debug("notify ", &event, " size=", event.waiters.size());
    for(const auto task : event.waiters) {
        if(std::get_if<ByMultiEvent>(&task->suspend_reason) == nullptr) {
            impl::error(__LINE__, " bug: task=", task, " index=", task->suspend_reason.index());
            continue;
        }
        task->suspend_reason.emplace<Running>();
    }
    event.waiters.clear();
}

inline auto Runner::io_wait(const IOHandle fd, const bool read, const bool write, IOWaitResult& result) -> void {
    // impl::debug("fd=", fd, " read=", read, " write=", write);
    current_task->suspend_reason.emplace<ByIO>(&result, fd, read, write);
}

inline auto Runner::run() -> void {
    auto loop_count = size_t(0);
loop:
    if(root.children.empty()) {
        current_task = &root;
        return;
    }

    impl::debug("loop ", (loop_count += 1));
    auto context = impl::CollectContext();
    collect_resumable_tasks(root, context);
    const auto timeout_ms = std::chrono::duration_cast<std::chrono::milliseconds>(context.sleep).count();

    if(!context.result.empty()) {
        run_tasks(context.result); // resume already waked tasks
        goto loop;
    }
    if(!context.poll_tasks.empty()) {
        // wait for io
        auto& pollfds = context.poll_fds;
        impl::debug("poll start timeout=", timeout_ms);
        const auto poll_timeout = timeout_ms + 1; // +1 to correct rounding error
#if defined(_WIN32)
        const auto nfds = WSAPoll(pollfds.data(), pollfds.size(), poll_timeout);
        if(nfds == SOCKET_ERROR) {
            impl::error(__LINE__, " poll failed errno=", WSAGetLastError());
            return;
        }
#else
        const auto nfds = poll(pollfds.data(), pollfds.size(), poll_timeout);
        if(nfds < 0 && errno != EINTR) {
            impl::error(__LINE__, " poll failed errno=", strerror(errno));
            return;
        }
#endif
        impl::debug("poll done count=", nfds);

        auto ready = std::vector<Task*>();
        for(auto i = 0, c = 0; i < int(pollfds.size()) && c < nfds; i += 1) {
            if(pollfds[i].revents == 0) {
                continue;
            }
            c += 1;
            auto& task = *context.poll_tasks[i];

            if(const auto io = std::get_if<ByIO>(&task.suspend_reason); io == nullptr) {
                impl::error(__LINE__, " bug");
            } else {
                *io->result = impl::revents_to_io_result(pollfds[i].revents);
            }
            task.suspend_reason.emplace<Running>();
            ready.push_back(&task);
        }
        run_tasks(context.result);
        goto loop;
    }

    impl::debug("sleeping ", timeout_ms);
    std::this_thread::sleep_for(context.sleep);
    goto loop;
}
} // namespace coop
