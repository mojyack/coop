#pragma once
#include <algorithm>
#include <cstring>
#include <utility>

#include "io-pre.hpp"
#include "multi-event-pre.hpp"
#include "promise-pre.hpp"
#include "runner-pre.hpp"
#include "single-event-pre.hpp"
#include "task-handle-pre.hpp"

#include "assert-def.hpp"

namespace coop {
namespace impl {
inline auto siblings(Task& task) -> std::list<Task>& {
    return task.parent->children;
}

inline auto find_iterator(Task& child) -> decltype(Task::children)::iterator {
    auto& children = siblings(child);
    for(auto i = children.begin(); i != children.end(); i = std::next(i)) {
        auto& current = *i;
        if(&current == &child) {
            return i;
        }
    }
    return children.end();
}

inline auto revents_to_io_result(const short revents) -> IOWaitResult {
    return IOWaitResult{
        .read  = bool(revents & POLLIN),
        .write = bool(revents & POLLOUT),
        .error = bool(revents & (POLLHUP | POLLERR | POLLNVAL)),
    };
}
} // namespace impl

inline auto Runner::dump_task_tree(const Task& task, int depth) -> void {
    const auto indent = std::string(depth, ' ');
    std::println("{}- task={} parent={} suspend={} obj={} zombie={}", indent, (void*)&task, (void*)task.parent, task.suspend_reason.index(), task.objective_of, task.zombie);
    for(const auto& child : task.children) {
        dump_task_tree(child, depth + 2);
    }
}

inline auto Runner::gather_resumable_tasks(Task& task, GatheringResult& result) -> void {
    if(!task.children.empty()) {
        for(auto& child : task.children) {
            gather_resumable_tasks(child, result);
        }
        return;
    }

    auto& reason = task.suspend_reason;
    switch(reason.index()) {
    case running_index:
        running_tasks.push_back(&task);
        break;
    case by_timer_index: {
        auto& timer = std::get<by_timer_index>(reason);
        if(timer.suspend_until < result.now) {
            reason.emplace<Running>();
            running_tasks.push_back(&task);
        } else {
            result.sleep = std::min(result.sleep, timer.suspend_until - result.now);
        }
    } break;
    case by_single_event_index:
        break;
    case by_multi_event_index:
        break;
    case by_io_index: {
        if(!running_tasks.empty()) {
            break; // polling will be skipped
        }
        auto& io = std::get<by_io_index>(reason);
        result.polling_fds.emplace_back(pollfd{io.file, short((io.read ? POLLIN : 0) | (io.write ? POLLOUT : 0)), 0});
        result.polling_tasks.push_back(&task);
    } break;
    case by_awaiting_index:
        break;
    default:
        PANIC("index={}", reason.index());
    }
}

inline auto Runner::run_tasks() -> void {
    const auto orig_loop_count = loop_count;
    for(auto& ptr : running_tasks) {
        if(ptr == nullptr) {
            continue; // destroyed
        }
        auto& task = *std::exchange(ptr, nullptr);
        DEBUG("resuming task={} handle={}", (void*)&task, task.handle.address());
        current_task = &task;
        task.handle.resume();
        DEBUG("task done task={} done={} zombie={}", (void*)&task, task.handle.done(), task.zombie);
        if(task.handle.done() || task.zombie) {
            remove_task(task);
        }
        if(orig_loop_count != loop_count) {
            // run() called inside the handle.resume()
            // this loop is obsolete
            TRACE("interrupted by another loop, discarding current resume queue");
            current_task = &task;
            break;
        }
    }
    running_tasks.clear();
}

template <CoHandleLike CoHandle>
inline auto Runner::push_task(const bool independent, const bool transfer_handle, CoHandle& handle, TaskHandle* const user_handle, size_t objective_of) -> void {
    handle.promise().runner = this;

    auto& parent = independent ? root : *current_task;
    // transfer cohandle to runner if independent
    // since child task may live longer than this generator
    auto& task = parent.children.emplace_back(Task{
        .handle       = transfer_handle ? std::exchange(handle, nullptr) : handle,
        .parent       = &parent,
        .user_handle  = user_handle,
        .objective_of = objective_of,
        .handle_owned = transfer_handle,
    });

    if(user_handle != nullptr) {
        *user_handle = TaskHandle{.task = &task, .runner = this, .destroyed = false};
    }

    DEBUG("new task pushed task={} handle={} current={}", (void*)&task, task.handle.address(), (void*)current_task);
    // dump_task_tree(root);
}

template <CoGeneratorLike Generator>
auto Runner::push_task(Generator generator, TaskHandle* const user_handle) -> void {
    push_task(true, true, generator.handle, user_handle, 0);
}

template <CoGeneratorLike Generator>
auto Runner::push_dependent_task(Generator generator) -> void {
    push_task(false, true, generator.handle, nullptr, 0);
}

template <CoGeneratorLike Generator>
auto Runner::await(Generator generator) -> decltype(auto) {
    push_task(false, false, generator.handle, nullptr, objective_task_finished.size());
    current_task->suspend_reason.emplace<ByAwaiting>();
    run();
    current_task->suspend_reason.emplace<Running>();
    if constexpr(PromiseWithRetValue<decltype(Generator::handle.promise())>) {
        using Opt = std::optional<std::remove_reference_t<decltype(generator.await_resume())>>;
        return current_task->zombie ? Opt() : Opt(generator.await_resume());
    } else {
        return !current_task->zombie;
    }
}

inline auto Runner::destroy_task(std::list<Task>::iterator iter) -> bool {
    auto& task = *iter;
    TRACE("destroy task={} handle={}", (void*)&task, task.handle.address());
    // destroy recursively
    for(auto c = task.children.begin(); c != task.children.end();) {
        c = destroy_task(c) ? task.children.erase(c) : std::next(c);
    }
    objective_task_finished[task.objective_of] = true;
    if(task.suspend_reason.index() == by_awaiting_index || !task.children.empty()) {
        // cannot destroy it for now
        task.zombie = true;
        TRACE("zombified task={}", (void*)&task);
        return false;
    }
    TRACE("performing destroy task={}", (void*)&task);

    // cancel events
    switch(task.suspend_reason.index()) {
    case running_index:
        if(&task == current_task) {
            // called from run_tasks
            break;
        }
        // called from cancel_task
        // we have to prevent the target task from being resumed
        for(auto& candidate : running_tasks) {
            if(candidate == &task) {
                candidate = nullptr; // remove target task from resume queue
                break;
            }
        }
        break;
    case by_single_event_index: {
        auto& event  = std::get<by_single_event_index>(task.suspend_reason);
        auto& waiter = event.event->waiter;
        ASSERT(waiter == &task, "task={}", (void*)&task);
        waiter = nullptr;
    } break;
    case by_multi_event_index: {
        auto& event   = std::get<by_multi_event_index>(task.suspend_reason);
        auto& waiters = event.event->waiters;
        auto  iter    = std::ranges::find(waiters, &task);
        ASSERT(iter != waiters.end(), "task={}", (void*)&task);
        waiters.erase(iter);
    } break;
    }

    if(task.user_handle != nullptr) {
        task.user_handle->task      = nullptr;
        task.user_handle->destroyed = true;
    }
    if(task.handle_owned) {
        // destroy() might call Runner::await() in destructor
        // but this marks as awaiting only the caller task of this function,
        // as current_task is not updated.
        // mark this task as awaiting to prevent scheduler to pick this up.
        task.suspend_reason.emplace<ByAwaiting>();
        task.handle.destroy();
    }

    return true;
}

inline auto Runner::remove_task(Task& task) -> bool {
    const auto iter = impl::find_iterator(task);
    ASSERT(iter != impl::siblings(task).end(), "parent={} child={}", (void*)task.parent, (void*)&task);
    if(destroy_task(iter)) {
        impl::siblings(task).erase(iter);
    }
    // dump_task_tree(root);
    return true;
}

inline auto Runner::cancel_task(TaskHandle& handle) -> bool {
    TRACE("cancel caller={} target={}", (void*)current_task, (void*)handle.task);
    if(handle.task == nullptr || handle.destroyed) {
        return true;
    }

    return remove_task(*handle.task);
}

inline auto Runner::join(TaskHandle& handle) -> void {
    TRACE("join task={} child={}", (void*)current_task, (void*)handle.task);
    if(handle.task == nullptr) {
        return;
    }

    auto& child = *handle.task;
    ASSERT(child.parent == &root, "task {} tried steal child task {} from another task {}", (void*)current_task, (void*)handle.task, (void*)handle.task->parent);

    // transfer task object
    auto& new_sib = current_task->children;
    auto& old_sib = impl::siblings(child);
    auto  iter    = impl::find_iterator(child);
    ASSERT(iter != old_sib.end(), "parent={} child={}", (void*)child.parent, (void*)&child);

    new_sib.splice(new_sib.end(), old_sib, iter);
    child.parent = current_task;
}

inline auto Runner::delay(const std::chrono::system_clock::duration duration) -> void {
    TRACE("delay task={} duratoin={}", (void*)current_task, duration);
    current_task->suspend_reason.emplace<ByTimer>(std::chrono::system_clock::now() + duration);
}

inline auto Runner::event_wait(SingleEvent& event) -> void {
    TRACE("event_wait(single) task={} event={}", (void*)current_task, (void*)&event);
    current_task->suspend_reason.emplace<BySingleEvent>(&event);
    event.waiter = current_task;
}

inline auto Runner::event_notify(SingleEvent& event) -> void {
    TRACE("event_notify(single) task={} event={} event.waiter={}", (void*)current_task, (void*)&event, (void*)event.waiter);
    const auto task = event.waiter;
    ASSERT(std::get_if<BySingleEvent>(&task->suspend_reason) != nullptr, "task={} index={}", (void*)task, task->suspend_reason.index());
    task->suspend_reason.emplace<Running>();
    event.waiter = nullptr;
}

inline auto Runner::event_wait(MultiEvent& event) -> void {
    TRACE("event_wait(multi) task={} event={}", (void*)current_task, (void*)&event);
    current_task->suspend_reason.emplace<ByMultiEvent>(&event);
    event.waiters.push_back(current_task);
}

inline auto Runner::event_notify(MultiEvent& event, size_t n) -> void {
    TRACE("event_notify(multi) task={} event={} n={}", (void*)current_task, (void*)&event, n);
    auto& waiters = event.waiters;
    n             = n == 0 ? waiters.size() : std::min(n, waiters.size());
    for(auto i = 0uz; i < n; i += 1) {
        const auto task = waiters[i];
        TRACE("  target {}", (void*)task);
        ASSERT(task->suspend_reason.index() == by_multi_event_index, "task={} index={}", (void*)task, task->suspend_reason.index());
        task->suspend_reason.emplace<Running>();
    }
    waiters.erase(waiters.begin(), waiters.begin() + n);
}

inline auto Runner::io_wait(const IOHandle fd, const bool read, const bool write, IOWaitResult& result) -> void {
    TRACE("io_wait task={} fd={} read={} write={}", (void*)current_task, fd, read, write);
    current_task->suspend_reason.emplace<ByIO>(&result, fd, read, write);
}

inline auto Runner::run() -> void {
    const auto orig_current_task = current_task;
    objective_task_finished.push_back(false);
    DEBUG("starting mainloop depth={}", objective_task_finished.size());

loop:
    if(objective_task_finished.size() == 1 ? /*root loop*/ root.children.empty() : /*derived loop*/ objective_task_finished.back()) {
        DEBUG("mainloop finished depth={}", objective_task_finished.size());
        current_task = orig_current_task;
        objective_task_finished.pop_back();
        return;
    }

    loop_count += 1;
    DEBUG("loop {}", loop_count);
    // dump_task_tree(root);

    if(!running_tasks.empty()) {
        run_tasks(); // resume previously gathered tasks inherited from parent loop
        goto loop;
    }

    auto result = GatheringResult();
    gather_resumable_tasks(root, result);
    const auto timeout_ms = std::chrono::duration_cast<std::chrono::milliseconds>(result.sleep).count();

    if(!running_tasks.empty()) {
        run_tasks(); // resume already waked tasks
        goto loop;
    }

    if(!result.polling_tasks.empty()) {
        // wait for io
        auto& pollfds = result.polling_fds;
        DEBUG("poll start timeout={}", timeout_ms);
        const auto poll_timeout = timeout_ms + 1; // +1 to correct rounding error
#if defined(_WIN32)
        const auto nfds = WSAPoll(pollfds.data(), pollfds.size(), poll_timeout);
        ASSERT(nfds != SOCKET_ERROR, "poll failed errno={}", WSAGetLastError());
#else
        const auto nfds = poll(pollfds.data(), pollfds.size(), poll_timeout);
        ASSERT(nfds >= 0 || errno == EINTR, "poll failed errno={}({})", errno, strerror(errno));
#endif
        DEBUG("poll done count={}", nfds);

        for(auto i = 0, c = 0; i < int(pollfds.size()) && c < nfds; i += 1) {
            if(pollfds[i].revents == 0) {
                continue;
            }
            c += 1;
            auto& task = *result.polling_tasks[i];

            const auto io = std::get_if<ByIO>(&task.suspend_reason);
            ASSERT(io != nullptr, "index={}", task.suspend_reason.index());
            *io->result = impl::revents_to_io_result(pollfds[i].revents);
            task.suspend_reason.emplace<Running>();
            running_tasks.push_back(&task);
        }
        run_tasks();
        goto loop;
    }

    DEBUG("sleeping {}", timeout_ms);
    std::this_thread::sleep_for(result.sleep);
    goto loop;
}
} // namespace coop

#include "assert-undef.hpp"
