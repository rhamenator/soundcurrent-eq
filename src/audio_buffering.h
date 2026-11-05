// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include <algorithm>
namespace soundcurrent::buffering {
inline constexpr int defaultWindowsMs = 40;
inline constexpr int maximumMs = 200;
inline constexpr int minimumMs = 5;
inline constexpr bool valid(int ms) {
    return ms == 0 || (ms >= minimumMs && ms <= maximumMs);
}
inline constexpr int windowsMs(int ms) {
    return ms == 0 || !valid(ms) ? defaultWindowsMs : ms;
}
inline constexpr long long windowsDuration(int ms) {
    return windowsMs(ms) * 10000LL;
}
inline constexpr int pollMs(int ms) {
    return std::clamp(windowsMs(ms) / 4, 1, 5);
}
} // namespace soundcurrent::buffering
