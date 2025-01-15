// warning: this file is not intended to be used by users
// include this file twice to avoid leaking macros to users
#ifndef COOP_ASSERT_H
#define COOP_ASSERT_H
#include <exception>
#include <iostream>
#include <source_location>

namespace coop {
inline auto format_filename(const std::string_view filename) -> std::string_view {
    if(const auto p = filename.rfind('/'); p != filename.npos) {
        return filename.substr(p + 1);
    } else {
        return filename;
    }
}

template <class... Args>
auto print(Args&&... args) -> void {
    (std::cout << ... << args) << std::endl;
}

template <class... Args>
auto warn(Args&&... args) -> void {
    (std::cerr << ... << args) << std::endl;
}
} // namespace coop
#endif // COOP_ASSERT_H

// macros
#ifndef COOP_MACROS
#define COOP_MACROS

#pragma push_macro("DEBUG")
#pragma push_macro("TRACE")
#pragma push_macro("PANIC")
#pragma push_macro("ASSERT")

// DEBUG
#if defined(COOP_DEBUG) || defined(COOP_TRACE)
#define DEBUG(...) print(format_filename(__FILE__), ":", __LINE__, " ", __VA_ARGS__)
#else
#define DEBUG(...)
#endif

// TRACE
#if defined(COOP_TRACE)
#define TRACE(...) print(format_filename(__FILE__), ":", __LINE__, " ", __VA_ARGS__)
#else
#define TRACE(...)
#endif

#define PANIC(...)                                                                                      \
    {                                                                                                   \
        warn("panicked at ", format_filename(__FILE__), ":", __LINE__ __VA_OPT__(, " ", ) __VA_ARGS__); \
        std::terminate();                                                                               \
    }

#define ASSERT(cond, ...)                                          \
    if(!(cond)) {                                                  \
        PANIC("assertion failed" __VA_OPT__(, " ", ) __VA_ARGS__); \
    }

#else
#undef COOP_MACROS

#pragma pop_macro("DEBUG")
#pragma pop_macro("TRACE")
#pragma pop_macro("PANIC")
#pragma pop_macro("ASSERT")

#endif
