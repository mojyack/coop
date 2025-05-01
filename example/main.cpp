#include <chrono>
#include <cstdlib>

// #define COOP_DEBUG
// #define COOP_TRACE

#include <coop/blocker.hpp>
#include <coop/generator.hpp>
#include <coop/io.hpp>
#include <coop/lock-guard.hpp>
#include <coop/multi-event.hpp>
#include <coop/mutex.hpp>
#include <coop/parallel.hpp>
#include <coop/pipe.hpp>
#include <coop/promise.hpp>
#include <coop/recursive-blocker.hpp>
#include <coop/runner.hpp>
#include <coop/select.hpp>
#include <coop/single-event.hpp>
#include <coop/task-handle.hpp>
#include <coop/task-injector.hpp>
#include <coop/thread.hpp>
#include <coop/timer.hpp>
#include <unistd.h>

#include <coop/assert-def.hpp> // do not do this

namespace {
auto format_filename(const std::string_view filename) -> std::string_view {
    return coop::format_filename(filename);
}

auto speed_rate    = 1.0;
auto errors        = 0uz;
auto ignore_errors = false;

auto fail(const int line) -> void {
    if(ignore_errors) {
        std::println("(ignoring assertion failure at line {})", line);
    } else {
        std::println(stderr, "test failed at line {}", line);
        errors += 1;
    }
}

#define fail fail(__LINE__)

#define ensure(cond) \
    if(!(cond)) {    \
        fail;        \
    }

struct TimeChecker {
    std::chrono::system_clock::time_point begin;

    auto test_elapsed(const int expected_secs) -> bool {
        constexpr auto tolerance = std::chrono::milliseconds(10);

        const auto expected = std::chrono::milliseconds(int(expected_secs * 1000 / speed_rate));
        const auto elapsed  = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now() - begin);
        if(expected > tolerance && elapsed < expected - tolerance) {
            std::println(stderr, "time check failed {} < {}", elapsed, expected);
            return false;
        }
        if(elapsed > expected + tolerance) {
            std::println(stderr, "time check failed {} > {}", elapsed, expected);
            return false;
        }
        return true;
    }

    TimeChecker() {
        begin = std::chrono::system_clock::now();
    }
};

auto delay_secs(const int seconds) -> std::chrono::system_clock::duration {
    return std::chrono::milliseconds(size_t(1000 * seconds / speed_rate));
}

auto sleep_test() -> coop::Async<void> {
    auto check = TimeChecker();
    co_await coop::sleep(delay_secs(3));
    ensure(check.test_elapsed(3));
    co_return;
}

auto funccall_test() -> coop::Async<void> {
    struct Local {
        static auto fn1(int rate) -> coop::Async<int> {
            co_return 10 * rate;
        }
        static auto fn2() -> coop::Async<int> {
            co_return co_await fn1(1) + co_await fn1(2);
        }
    };
    ensure(co_await Local::fn2() == 30);
    co_return;
}

auto dependent_task_test() -> coop::Async<void> {
    struct Local {
        static auto fn() -> coop::Async<void> {
            co_await coop::sleep(delay_secs(1));
        }
    };
    auto callback = [&runner = *co_await coop::reveal_runner()] {
        runner.push_dependent_task(Local::fn());
    };
    auto check = TimeChecker();
    callback();
    check.test_elapsed(0);
    co_await coop::yield();
    check.test_elapsed(1);
}

auto parallel_test() -> coop::Async<void> {
    struct Local {
        static auto fn(int& done_count) -> coop::Async<void> {
            co_await coop::sleep(delay_secs(3));
            done_count += 1;
        }

        static auto ret(char value) -> coop::Async<char> {
            co_return value;
        }
    };

    { // args time
        auto check = TimeChecker();
        auto count = 0;
        co_await coop::run_args(Local::fn(count), Local::fn(count), Local::fn(count));
        ensure(check.test_elapsed(3));
        ensure(count == 3);
    }
    { // vec time
        auto check = TimeChecker();
        auto count = 0;
        auto jobs  = std::vector<coop::Async<void>>();
        for(auto i = 0; i < 3; i += 1) {
            jobs.emplace_back(Local::fn(count));
        }
        co_await coop::run_vec(std::move(jobs));
        ensure(check.test_elapsed(3));
        ensure(count == 3);
    }
    { // args return value
        const auto ret = co_await coop::run_args(Local::ret('a'), Local::ret('b'));
        ensure(std::get<0>(ret) == 'a' && std::get<1>(ret) == 'b');
    }
    { // vec return value
        auto jobs = std::vector<coop::Async<char>>();
        for(auto i = 0; i < 2; i += 1) {
            jobs.emplace_back(Local::ret('a' + i));
        }
        const auto ret = co_await coop::run_vec(std::move(jobs));
        ensure(ret[0] == 'a' && ret[1] == 'b');
    }
    co_return;
}

auto single_event_test() -> coop::Async<void> {
    struct Local {
        static auto fn1(int& count, coop::SingleEvent& event1, coop::SingleEvent& event2) -> coop::Async<void> {
            ensure(count == 0);
            event2.notify();
            co_await event1;

            ensure(count == 1);
            count = 2;
            event2.notify();
            co_return;
        }
        static auto fn2(int& count, coop::SingleEvent& event1, coop::SingleEvent& event2) -> coop::Async<void> {
            co_await event2;
            count = 1;
            event1.notify();

            co_await event2;
            ensure(count == 2);
            co_return;
        }
    };

    auto count  = 0;
    auto event1 = coop::SingleEvent();
    auto event2 = coop::SingleEvent();
    co_await coop::run_args(Local::fn1(count, event1, event2), Local::fn2(count, event1, event2));
    co_return;
}

auto multi_event_many_waiters_test() -> coop::Async<void> {
    struct Local {
        static auto fn(int& count, coop::MultiEvent& event) -> coop::Async<void> {
            for(auto i = 0; i < 3; i += 1) {
                co_await event;
                count += 1;
            }
        }
    };

    auto  count  = 0;
    auto  event  = coop::MultiEvent();
    auto& runner = *(co_await coop::reveal_runner());
    for(auto i = 0; i < 10; i += 1) {
        runner.push_task(Local::fn(count, event));
    }
    for(auto i = 0; i < 3; i += 1) {
        co_await coop::sleep(delay_secs(1));
        event.notify();
        co_await coop::sleep(delay_secs(1));
        ensure(count == (i + 1) * 10);
    }
    co_return;
}

auto multi_event_notify_n_test() -> coop::Async<void> {
    struct Local {
        int              count;
        coop::MultiEvent event;

        auto spawn_tasks(const size_t n) -> coop::Async<void> {
            auto& runner = *(co_await coop::reveal_runner());
            for(auto i = 0uz; i < n; i += 1) {
                runner.push_task(fn());
            }
            while(event.waiters.size() != n) {
                co_await coop::yield();
            }
        }

        auto fn() -> coop::Async<void> {
            co_await event;
            count += 1;
        }
    };

    auto local = Local();

    // notify one
    local.count = 0;
    co_await local.spawn_tasks(3);
    for(auto i = 0; i < 3; i += 1) {
        local.event.notify(1);
        co_await coop::sleep(delay_secs(1));
        ensure(local.count == i + 1);
    }

    // notify two
    local.count = 0;
    co_await local.spawn_tasks(4);
    for(auto i = 0; i < 2; i += 1) {
        local.event.notify(2);
        co_await coop::sleep(delay_secs(1));
        ensure(local.count == (i + 1) * 2);
    }
}

auto thread_event_test() -> coop::Async<void> {
    auto count  = 0;
    auto event  = coop::ThreadEvent();
    auto thread = std::thread([&]() {
        for(auto i = 0; i < 3; i += 1) {
            ensure(count == i);
            event.notify();
            std::this_thread::sleep_for(delay_secs(1));
        }
    });
    for(auto i = 0; i < 3;) {
        const auto notified = co_await event;
        ensure(notified == 1);
        i += notified;
        count += 1;
    }
    thread.join();
    co_return;
}

auto task_cancel_test() -> coop::Async<void> {
    struct Local {
        static auto fn() -> coop::Async<void> {
            co_await coop::sleep(delay_secs(2));
            fail;
        }
        static auto fn(bool& flag) -> coop::Async<void> {
            co_await coop::sleep(delay_secs(1));
            flag = true;
        }
        static auto fn(coop::SingleEvent& event) -> coop::Async<void> {
            co_await event;
            fail;
        }
        static auto fn(coop::MultiEvent& event) -> coop::Async<void> {
            co_await event;
            fail;
        }
        static auto fn(coop::ThreadEvent& event) -> coop::Async<void> {
            co_await event;
            fail;
        }
    };

    auto& runner = *(co_await coop::reveal_runner());

    // cancel
    auto task = coop::TaskHandle();
    runner.push_task(Local::fn(), &task);
    ensure(!task.destroyed);
    co_await coop::sleep(delay_secs(1));
    task.cancel();
    ensure(task.destroyed);

    // dissociate
    auto flag = false;
    runner.push_task(Local::fn(flag), &task);
    task.dissociate();
    task.cancel();
    co_await coop::sleep(delay_secs(2));
    ensure(flag);

    // cancel while waiting for single event
    auto sevent = coop::SingleEvent();
    runner.push_task(Local::fn(sevent), &task);
    co_await coop::sleep(delay_secs(1));
    task.cancel();

    // cancel while waiting for multi event
    auto mevent = coop::MultiEvent();
    runner.push_task(Local::fn(mevent), &task);
    co_await coop::sleep(delay_secs(1));
    task.cancel();

    // cancel while waiting for thread event
    auto tevent = coop::ThreadEvent();
    runner.push_task(Local::fn(tevent), &task);
    co_await coop::sleep(delay_secs(1));
    task.cancel();

    co_return;
}

auto nested_task_cancel_test() -> coop::Async<void> {
    struct Local {
        static auto base(coop::MultiEvent& event) -> coop::Async<void> {
            co_await event;
            fail;
        }
        static auto branch(coop::MultiEvent& event) -> coop::Async<void> {
            co_await coop::run_args(base(event), base(event), base(event));
        }
        static auto branch2(coop::MultiEvent& event) -> coop::Async<void> {
            co_await coop::run_args(branch(event), branch(event), branch(event));
        }
    };

    auto& runner = *(co_await coop::reveal_runner());

    // cancel
    auto task  = coop::TaskHandle();
    auto event = coop::MultiEvent();
    runner.push_task(Local::branch2(event), &task);
    co_await coop::sleep(delay_secs(1));
    task.cancel();
    event.notify();
}

auto task_cancel_running_test() -> coop::Async<void> {
    struct Local {
        static auto fn(coop::MultiEvent& event, coop::TaskHandle& handle) -> coop::Async<void> {
            co_await event;
            handle.cancel();
        }
    };

    auto& runner = *(co_await coop::reveal_runner());

    // cancel running task
    auto event = coop::MultiEvent();
    auto task1 = coop::TaskHandle();
    auto task2 = coop::TaskHandle();
    // task1,2 wake at the same time and cancel each other
    runner.push_task(Local::fn(event, task1), &task2);
    runner.push_task(Local::fn(event, task2), &task1);
    co_await coop::sleep(delay_secs(1));
    event.notify();

    co_return;
}

auto task_join_test() -> coop::Async<void> {
    struct Local {
        static auto f1() -> coop::Async<void> {
            co_await coop::sleep(delay_secs(1));
        }
    };

    auto& runner = *(co_await coop::reveal_runner());
    auto  task   = coop::TaskHandle();
    {
        auto check = TimeChecker();
        runner.push_task(Local::f1(), &task);
        ensure(check.test_elapsed(0));
        task.cancel();
    }
    {
        auto check = TimeChecker();
        runner.push_task(Local::f1(), &task);
        co_await task.join();
        ensure(check.test_elapsed(1));
        task.cancel();
    }
}

auto io_test() -> coop::Async<void> {
    auto pipe   = coop::Pipe();
    auto thread = std::thread([&]() {
        for(auto i = 0; i < 3; i += 1) {
            std::this_thread::sleep_for(delay_secs(1));
            ensure(pipe.write(&i, sizeof(i)) == sizeof(i));
        }
        std::this_thread::sleep_for(delay_secs(1));
        pipe.~Pipe();
    });
    for(auto i = 0; i < 3; i += 1) {
        auto check = TimeChecker();
        ensure(!(co_await coop::wait_for_file(pipe.producer(), true, false)).error);
        ensure(check.test_elapsed(1));
        auto n = 0;
        ensure(pipe.read(&n, sizeof(n)) == sizeof(n));
        ensure(n == i);
    }
    // pipe closed by thread
    auto check = TimeChecker();
    ensure((co_await coop::wait_for_file(pipe.producer(), true, false)).error);
    ensure(check.test_elapsed(1));

    thread.join();
    co_return;
}

auto run_thread_test() -> coop::Async<void> {
    struct Local {
        static auto fn(int* count) -> void {
            for(auto i = 0; i < 3; i += 1) {
                std::this_thread::sleep_for(delay_secs(1));
                (*count) += 1;
            }
        }
    };
    auto count = 0;
    auto check = TimeChecker();
    co_await coop::run_blocking(Local::fn, &count);
    ensure(check.test_elapsed(3));
    ensure(count == 3);
    co_return;
}

auto mutex_test() -> coop::Async<void> {
    struct Local {
        coop::Mutex mutex;

        auto fn() -> coop::Async<void> {
            co_await mutex.lock();
            ensure(mutex.held);
            co_await coop::sleep(delay_secs(1));
            mutex.unlock();
        }

        auto fn2() -> coop::Async<void> {
            auto lock = co_await coop::LockGuard::lock(mutex);
            ensure(mutex.held);
            co_await coop::sleep(delay_secs(1));
        }
    };

    auto& runner = *co_await coop::reveal_runner();
    auto  local  = Local();
    // raw mutex
    {
        for(auto i = 0; i < 3; i += 1) {
            runner.push_dependent_task(local.fn());
        }
        auto check = TimeChecker();
        co_await coop::yield();
        ensure(check.test_elapsed(3));
    }
    // lock guard
    {
        for(auto i = 0; i < 3; i += 1) {
            runner.push_dependent_task(local.fn2());
        }
        auto check = TimeChecker();
        co_await coop::yield();
        ensure(check.test_elapsed(3));
    }
}

auto blocker_test() -> coop::Async<void> {
    auto& runner  = *co_await coop::reveal_runner();
    auto  blocker = coop::Blocker();
    blocker.start(runner);

    auto thread = std::thread([&]() {
        blocker.block();
        std::this_thread::sleep_for(delay_secs(3));
        blocker.unblock();
    });

    auto check = TimeChecker();
    co_await coop::sleep(delay_secs(1));
    ensure(check.test_elapsed(3));

    blocker.stop();
    thread.join();
    co_return;
}

auto task_injector_test() -> coop::Async<void> {
    auto& runner   = *co_await coop::reveal_runner();
    auto  injector = coop::TaskInjector(runner);
    auto  thread   = std::thread([&injector]() {
        const auto tid = std::this_thread::get_id();
        // without return value
        injector.inject_task([tid]() -> coop::Async<void> {
            ensure(tid != std::this_thread::get_id());
            co_return;
        }());
        // with return value
        const auto ret = injector.inject_task([tid]() -> coop::Async<int> {
            ensure(tid != std::this_thread::get_id());
            co_return 0x1d6b;
        }());
        ensure(ret == 0x1d6b);
    });
    co_await coop::run_blocking([&thread]() { thread.join(); });
    co_return;
}

auto select_test() -> coop::Async<void> {
    static const auto task = [](int n) -> coop::Async<void> {
        co_await coop::sleep(delay_secs(n));
    };
    ensure(co_await coop::select(task(3), task(1), task(2)) == 1);
    co_return;
}

auto await_from_normal_func_test() -> coop::Async<void> {
    struct Local {
        static auto calc(int count) -> coop::Async<int> {
            co_await coop::sleep(delay_secs(1));
            co_return count * 2;
        }
        static auto fn(coop::Runner& runner) -> int {
            const auto ret = runner.await(calc(1));
            return ret ? *ret : -1;
        }
        static auto call_fn(coop::Runner& runner) -> coop::Async<int> {
            co_return fn(runner);
        }
        static auto call_call_fn(coop::Runner& runner) -> int {
            const auto ret = runner.await(call_fn(runner));
            return ret ? *ret : -1;
        }
    };

    auto& runner = *co_await coop::reveal_runner();
    {
        // call coroutine calc() from normal function fn()
        auto checker = TimeChecker();
        ensure(Local::fn(runner) == 2);
        ensure(checker.test_elapsed(1));
    }
    co_await coop::yield(); // give other tasks chance to release nested loop
    {
        // nested
        auto checker = TimeChecker();
        ensure(Local::call_call_fn(runner) == 2);
        ensure(checker.test_elapsed(1));
    }
}

auto await_in_cancel_test() -> coop::Async<void> {
    struct Worker {
        coop::Runner* runner;
        int*          count;
        bool          destructing = false;

        auto fn() -> coop::Async<void> {
            co_await coop::sleep(delay_secs(1));
            *count = 1;
        }

        ~Worker() {
            destructing = true;
            runner->await(fn());
        }
    };

    struct Local {
        static auto fn(coop::Runner& runner, int& count) -> coop::Async<void> {
            auto worker   = Worker();
            worker.runner = &runner;
            worker.count  = &count;
        loop:
            ensure(!worker.destructing);
            co_await coop::sleep(delay_secs(1));
            goto loop;
        }
    };

    auto  count  = 0;
    auto& runner = *(co_await coop::reveal_runner());
    auto  task   = coop::TaskHandle();
    runner.push_task(Local::fn(runner, count), &task);
    co_await coop::sleep(delay_secs(1));
    auto checker = TimeChecker();
    task.cancel();
    ensure(count == 1);
    ensure(checker.test_elapsed(1));
}

auto await_cancel_test() -> coop::Async<void> {
    struct Local {
        static auto ret(int time) -> coop::Async<int> {
            co_await coop::sleep(delay_secs(time));
            co_return time;
        }
        static auto base() -> coop::Async<void> {
            co_await coop::sleep(delay_secs(2));
        }
        static auto fn(coop::Runner& runner, int depth) -> void {
            runner.await(depth == 0 ? base() : co(runner, depth - 1));
        }
        static auto co(coop::Runner& runner, int depth) -> coop::Async<void> {
            fn(runner, depth);
            co_return;
        }
    };

    auto& runner = *co_await coop::reveal_runner();

    {
        auto checker = TimeChecker();
        co_await Local::co(runner, 5);
        ensure(checker.test_elapsed(2));
    }
    co_await coop::yield();
    {
        auto checker = TimeChecker();
        auto task    = coop::TaskHandle();
        runner.push_task(Local::co(runner, 5), &task);
        co_await coop::sleep(delay_secs(1));
        task.cancel();
        ensure(checker.test_elapsed(1));
    }
}

auto await_complex_cancel_test() -> coop::Async<void> {
    struct Local {
        static auto task4() -> coop::Async<void> {
            co_await coop::sleep(delay_secs(3));
        }
        static auto task3() -> coop::Async<void> {
            (co_await coop::reveal_runner())->await(task4());
        }
        static auto task2() -> coop::Async<void> {
            co_await coop::run_args(task3(), task3(), task3());
        }
        static auto task1() -> coop::Async<void> {
            (co_await coop::reveal_runner())->await(task2());
        }
    };
    /*
    # call stack
    main()
    run(depth=1) # on main
    resume(task1)
      await(task3)
      run(depth=2) # on task1
      resume(task3)
        await(task4)
        run(depth=3) # on task3
        resume(task4)
          co_await (sleep,io,event,...)
        resume(taskN)
          cancel(task1) # what

    # task tree
    root
      task1(awaiting)
        task2(waiting task3)
          task3(awaiting)
            task4(awaiter)
      taskN
     */

    auto& runner = *co_await coop::reveal_runner();

    {
        auto checker = TimeChecker();
        auto task    = coop::TaskHandle();
        runner.push_task(Local::task1(), &task);
        co_await coop::sleep(delay_secs(1));
        task.cancel();
        ensure(checker.test_elapsed(1));
    }
}

#define test(name) \
    std::pair { #name, &name##_test }
const auto tests = std::array{
    test(sleep),
    test(funccall),
    test(dependent_task),
    test(parallel),
    test(single_event),
    test(multi_event_many_waiters),
    test(multi_event_notify_n),
    test(thread_event),
    test(task_cancel),
    test(task_cancel_running),
    test(nested_task_cancel),
    test(task_join),
    test(io),
    test(run_thread),
    test(mutex),
    test(blocker),
    test(task_injector),
    test(select),
    test(await_from_normal_func),
    test(await_in_cancel),
    test(await_cancel),
    test(await_complex_cancel),
};
} // namespace

auto main(const int argc, const char* const* argv) -> int {
    if(argc == 2) {
        speed_rate = std::strtod(argv[1], NULL);
        ASSERT(speed_rate != 0.0);
    }

    auto runner = coop::Runner();

    for(const auto [name, func] : tests) {
        std::println(R"(running test "{}")", name);
        runner.push_task(func());
        runner.run();
    }

    std::println("running all tests at once");
    // some assertion fails because of the scheduler implementation
    // run this test anyway for crash check
    ignore_errors = true;
    for(const auto [name, func] : tests) {
        runner.push_task(func());
    }
    runner.run();

    if(errors == 0) {
        std::println("pass");
        return 0;
    } else {
        return 1;
    }
}
