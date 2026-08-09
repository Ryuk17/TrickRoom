#ifndef RTC_BASE_NUMERICS_SAFE_CONVERSIONS_H_
#define RTC_BASE_NUMERICS_SAFE_CONVERSIONS_H_

#include <limits>
#include <type_traits>

namespace webrtc {

// Saturated cast — clamps value to the range of the target type.
template <typename D, typename S>
constexpr D saturated_cast(S value) {
  static_assert(std::is_integral_v<S> && std::is_integral_v<D>);
  if constexpr (std::is_signed_v<S> == std::is_signed_v<D>) {
    if (sizeof(S) > sizeof(D)) {
      if (value > static_cast<S>(std::numeric_limits<D>::max()))
        return std::numeric_limits<D>::max();
      if (value < static_cast<S>(std::numeric_limits<D>::min()))
        return std::numeric_limits<D>::min();
    }
    return static_cast<D>(value);
  } else if constexpr (std::is_signed_v<S>) {
    // S signed, D unsigned
    if (value < 0) return 0;
    if (static_cast<std::make_unsigned_t<S>>(value) >
        static_cast<std::make_unsigned_t<S>>(std::numeric_limits<D>::max()))
      return std::numeric_limits<D>::max();
    return static_cast<D>(value);
  } else {
    // S unsigned, D signed
    if (value > static_cast<std::make_unsigned_t<D>>(
                    std::numeric_limits<D>::max()))
      return std::numeric_limits<D>::max();
    return static_cast<D>(value);
  }
}

// dchecked_cast: like static_cast but with a consistency check in debug mode.
// In the pure build this is a simple static_cast (no RTC_DCHECK available).
template <typename Dst, typename Src>
inline constexpr Dst dchecked_cast(Src value) {
  return static_cast<Dst>(value);
}

}  // namespace webrtc

#endif  // RTC_BASE_NUMERICS_SAFE_CONVERSIONS_H_
