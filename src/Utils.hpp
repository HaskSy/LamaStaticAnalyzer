/**
 * @file Utils.hpp
 * @brief This file contains some little utility functions that are
 * usefull in code
 *
 */
#pragma once

#include "Types.hpp"
#include <cassert>
#include <cstring>
#include <limits>
#include <type_traits>

#if (defined(__clang__) || defined(__GNUC__) || defined(__GNUG__))
#define LI_ALWAYS_INLINE __attribute__((always_inline)) inline
#else
#define LI_ALWAYS_INLINE inline
#endif

/**
 * @brief Returns underlying integer value of an enum. Backport of already
 * existing in C++23 `std::to_underlying`
 */
template<typename Enum>
[[nodiscard]]
// NOLINTNEXTLINE
constexpr std::underlying_type_t<Enum> to_underlying(Enum enumClass) {
    return static_cast<std::underlying_type_t<Enum>>(enumClass);
}

// NOLINTNEXTLINE
#define TODO() std::terminate()

// NOLINTNEXTLINE
#define FAIL() std::terminate()

// FIXME: check that these types are trivially copyable (or something like this)
// And check their size
template<typename T, typename U>
void copyValues(T* to, const U* from) {
    std::memcpy(to, from, sizeof(T));
}

LI_ALWAYS_INLINE
auto satAdd(u32 lhs, u32 rhs) -> u32 {
    u32 res = lhs + rhs;
    if (res < lhs) { res = std::numeric_limits<u32>::max(); }
    return res;
}
