#include <thread>

#include <coop/pipe.hpp>

auto main() -> int {
    auto pipe = coop::Pipe();
    auto t1   = std::thread([&pipe]() {
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
        pipe.write("hello", 5);
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
        pipe.write("world", 5);
    });
    auto t2   = std::thread([&pipe]() {
        auto buf = std::array<char, 8>();
        coop::line_print(std::string_view(buf.data(), pipe.read(&buf, buf.size())));
        coop::line_print(std::string_view(buf.data(), pipe.read(&buf, buf.size())));
    });
    t1.join();
    t2.join();

    return 0;
}
