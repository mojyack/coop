#pragma once
#include <exception>
#include <iostream>
#include <source_location>

namespace coop {
inline auto format_filename(std::string filename) -> std::string {
    if(const auto p = filename.rfind('/'); p != filename.npos) {
        filename = filename.substr(p + 1);
    }
    return filename;
}

template <class... Args>
struct line_print {
    line_print(Args&&... args, const std::source_location location = std::source_location::current()) {
        ((std::cout << format_filename(location.file_name()) << ":" << location.line() << " ") << ... << args) << std::endl;
    }
};

template <class... Args>
line_print(Args&&... args) -> line_print<Args...>;

template <class... Args>
struct assert {
    assert(const int cond, Args&&... args, const std::source_location location = std::source_location::current()) {
        if(cond) {
            return;
        }
        ((std::cerr << format_filename(location.file_name()) << ":" << location.line() << " ") << ... << args) << std::endl;
        std::terminate();
    }
};

template <class... Args>
assert(bool cond, Args&&... args) -> assert<Args...>;
} // namespace coop
