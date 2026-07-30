#ifndef RTC_BASE_NUMERICS_SAFE_MINMAX_H_
#define RTC_BASE_NUMERICS_SAFE_MINMAX_H_

#include <algorithm>

// Minimal rtc::SafeMin / rtc::SafeMax — simple passthrough to std::min/max
namespace rtc {

template <typename T1, typename T2>
constexpr auto SafeMin(T1 a, T2 b) -> decltype(a < b ? a : b) {
  return a < b ? a : b;
}

template <typename T1, typename T2>
constexpr auto SafeMax(T1 a, T2 b) -> decltype(a > b ? a : b) {
  return a > b ? a : b;
}

}  // namespace rtc

#endif  // RTC_BASE_NUMERICS_SAFE_MINMAX_H_
