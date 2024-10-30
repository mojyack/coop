#pragma once
#include "cohandle.hpp"

namespace coop {
template <class T>
concept CoGeneratorLike = CoHandleLike<decltype(T::handle)>;

template <class T>
struct CoGenerator;
} // namespace coop
