#pragma once

namespace tmx {

//inline int64_t clocks() { return __rdtsc(); }

using clk = std::chrono::steady_clock;

inline int64_t tics() noexcept { return clk::now().time_since_epoch().count(); }
inline constexpr int64_t freq() noexcept { return clk::period::den; }

inline int64_t ms() noexcept {
    std::chrono::duration<int64_t, std::ratio<1, freq()>> const u(tics());
    return std::chrono::duration_cast<std::chrono::milliseconds>(u).count();
}

inline int64_t mks() noexcept {
    std::chrono::duration<int64_t, std::ratio<1, freq()>> const u(tics());
    return std::chrono::duration_cast<std::chrono::microseconds>(u).count();
}

inline int64_t nano() noexcept {
    std::chrono::duration<int64_t, std::ratio<1, freq()>> const u(tics());
    return std::chrono::duration_cast<std::chrono::nanoseconds>(u).count();
}

}
