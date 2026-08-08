/*
 * Minimal safe comparison functions for the pure build.
 * Extracted from rtc_base/numerics/safe_compare.h.
 */
#ifndef RTC_BASE_NUMERICS_SAFE_COMPARE_H_
#define RTC_BASE_NUMERICS_SAFE_COMPARE_H_

#include <cstdint>
#include <limits>
#include <type_traits>

namespace webrtc {

namespace safe_cmp_impl {

template <size_t N>
struct SignedIntOfSize;

template <>
struct SignedIntOfSize<1> {
  using type = int8_t;
};
template <>
struct SignedIntOfSize<2> {
  using type = int16_t;
};
template <>
struct SignedIntOfSize<4> {
  using type = int32_t;
};
template <>
struct SignedIntOfSize<8> {
  using type = int64_t;
};

template <typename T>
constexpr typename std::make_unsigned<T>::type MakeUnsigned(T a) {
  return static_cast<typename std::make_unsigned<T>::type>(a);
}

template <typename Op, typename T1, typename T2>
constexpr bool Cmp(T1 a, T2 b) {
  using S1 = typename SignedIntOfSize<sizeof(T1)>::type;
  using S2 = typename SignedIntOfSize<sizeof(T2)>::type;
  using U1 = typename std::make_unsigned<T1>::type;
  using U2 = typename std::make_unsigned<T2>::type;
  if (std::is_signed<T1>::value && std::is_signed<T2>::value)
    return Op::template Op<T1, T2>(a, b);
  if (std::is_signed<T1>::value && std::is_unsigned<T2>::value)
    return Op::template Op<T1, T2>(a, b);
  if (std::is_unsigned<T1>::value && std::is_signed<T2>::value)
    return Op::template Op<T1, T2>(a, b);
  return Op::template Op<U1, U2>(MakeUnsigned(a), MakeUnsigned(b));
}

struct LtOp {
  template <typename T1, typename T2>
  static constexpr bool Op(T1 a, T2 b) {
    return a < b;
  }
};

}  // namespace safe_cmp_impl

template <typename T1, typename T2>
constexpr bool SafeLt(T1 a, T2 b) {
  return safe_cmp_impl::Cmp<safe_cmp_impl::LtOp>(a, b);
}

}  // namespace webrtc

#endif  // RTC_BASE_NUMERICS_SAFE_COMPARE_H_
