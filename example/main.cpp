#include <chrono>
#include <cstdlib>

// #define COOP_DEBUG
// #define COOP_TRACE

#include <coop/blocker.hpp>
#include <coop/generator.hpp>
#include <coop/io.hpp>
#include <coop/multi-event.hpp>
#include <coop/parallel.hpp>
#include <coop/pipe.hpp>
#include <coop/promise.hpp>
#include <coop/recursive-blocker.hpp>
#include <coop/runner.hpp>
#include <coop/single-event.hpp>
#include <coop/task-injector.hpp>
#include <coop/thread.hpp>
#include <coop/timer.hpp>
#include <unistd.h>

#include <coop/assert-def.hpp> // do not do this

auto format_filename(const std::string_view filename) -> std::string_view {
    return coop::format_filename(filename);
}

auto speed_rate = 1.0;
auto errors     = 0uz;

#define fail                                                  \
    std::println(stderr, "test failed at line {}", __LINE__); \
    errors += 1;

#define ensure(cond) \
    if(!(cond)) {    \
        fail;        \
    }

struct TimeChecker {
    std::chrono::system_clock::time_point begin;

    auto test_elapsed(const int expected_secs) -> bool {
        constexpr auto tolerance = 0.1;

        const auto expected_ms = expected_secs * 1000 / speed_rate;
        const auto elapsed     = std::chrono::system_clock::now() - begin;
        const auto ms          = std::chrono::duration_cast<std::chrono::milliseconds>(elapsed).count();
        if(ms < expected_ms * (1 - tolerance)) {
            std::println(stderr, "time check failed {} < {}", ms, expected_ms * (1 - tolerance));
            return false;
        }
        if(ms > expected_ms * (1 + tolerance)) {
            std::println(stderr, "time check failed {} > {}", ms, expected_ms * (1 + tolerance));
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

auto multi_event_test() -> coop::Async<void> {
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
    for(auto i = 0; i < 3; i += 1) {
        co_await event;
        count += 1;
    }
    thread.join();
    co_return;
}

auto task_cancel_test() -> coop::Async<void> {
    struct Local {
        static auto fn1() -> coop::Async<void> {
            co_await coop::sleep(delay_secs(2));
            fail;
        }
        static auto fn2(bool& flag) -> coop::Async<void> {
            co_await coop::sleep(delay_secs(1));
            flag = true;
        }
        static auto fn3(coop::SingleEvent& event) -> coop::Async<void> {
            co_await event;
            fail;
        }
        static auto fn4(coop::MultiEvent& event) -> coop::Async<void> {
            co_await event;
            fail;
        }
        static auto fn5(coop::ThreadEvent& event) -> coop::Async<void> {
            co_await event;
            fail;
        }
    };

    auto& runner = *(co_await coop::reveal_runner());

    // cancel
    auto task = coop::TaskHandle();
    runner.push_task(Local::fn1(), &task);
    ensure(!task.destroyed);
    co_await coop::sleep(delay_secs(1));
    task.cancel();
    ensure(task.destroyed);

    // dissociate
    auto flag = false;
    runner.push_task(Local::fn2(flag), &task);
    task.dissociate();
    task.cancel();
    co_await coop::sleep(delay_secs(2));
    ensure(flag);

    // cancel while waiting for single event
    auto sevent = coop::SingleEvent();
    runner.push_task(Local::fn3(sevent), &task);
    co_await coop::sleep(delay_secs(1));
    task.cancel();

    // cancel while waiting for multi event
    auto mevent = coop::MultiEvent();
    runner.push_task(Local::fn4(mevent), &task);
    co_await coop::sleep(delay_secs(1));
    task.cancel();

    // cancel while waiting for thread event
    auto tevent = coop::ThreadEvent();
    runner.push_task(Local::fn5(tevent), &task);
    co_await coop::sleep(delay_secs(1));
    task.cancel();

    co_return;
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
    check.test_elapsed(3);

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
        injector.inject_task(
            [](decltype(tid) tid) -> coop::Async<void> {
                ensure(tid != std::this_thread::get_id());
                co_return;
            }(tid));
        // with return value
        const auto ret = injector.inject_task(
            [](decltype(tid) tid) -> coop::Async<int> {
                ensure(tid != std::this_thread::get_id());
                co_return 0x1d6b;
            }(tid));
        ensure(ret == 0x1d6b);
    });
    co_await coop::run_blocking([&thread]() { thread.join(); });
    co_return;
}

#define test(name) \
    std::pair { #name, &name##_test }
const auto tests = std::array{
    test(sleep),
    test(funccall),
    test(parallel),
    test(single_event),
    test(multi_event),
    test(thread_event),
    test(task_cancel),
    test(io),
    test(run_thread),
    test(blocker),
    test(task_injector),
};

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

    // does not work since some tests do blocking operation such as thread.join()
    // std::println("running all tests at once");
    // for(const auto [name, func] : tests) {
    //     runner.push_task(func());
    // }
    // runner.run();

    if(errors == 0) {
        std::println("pass");
        return 0;
    } else {
        return 1;
    }
}
