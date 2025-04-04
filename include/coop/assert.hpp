// warning: this file is not intended to be used by users
// include this file twice to avoid leaking macros to users
#pragma once
#include <string_view>

namespace coop {
inline auto format_filename(const std::string_view filename) -> std::string_view {
    if(const auto p = filename.rfind('/'); p != filename.npos) {
        return filename.substr(p + 1);
    } else {
        return filename;
    }
}
} // namespace coop
