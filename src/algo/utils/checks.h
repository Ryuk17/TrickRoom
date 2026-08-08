#ifndef RTC_BASE_CHECKS_H_
#define RTC_BASE_CHECKS_H_

#include <stdio.h>
#include <stdlib.h>

#if !defined(NDEBUG) || defined(DCHECK_ALWAYS_ON)
#define RTC_DCHECK_IS_ON 1
#else
#define RTC_DCHECK_IS_ON 0
#endif

#if defined(_MSC_VER)
#define RTC_NORETURN __declspec(noreturn)
#elif defined(__GNUC__)
#define RTC_NORETURN __attribute__((__noreturn__))
#else
#define RTC_NORETURN
#endif

#ifdef __cplusplus

#include <iostream>
#include <type_traits>

struct FatalLogCall {
  template <typename T>
  RTC_NORETURN void operator&(const T&) const {
    fprintf(stderr, "CHECK failed: %s\n  at %s:%d\n", msg_, file_, line_);
    abort();
  }
  const char* file_;
  int line_;
  const char* msg_;
};

#define RTC_CHECK(condition) \
  (condition) ? (void)0 : FatalLogCall{__FILE__, __LINE__, #condition} & std::cerr

// Safe comparison helpers — handle signed/unsigned mixed types.
// When one operand is unsigned and the other is signed, cast the unsigned
// value to a signed type of sufficient width to avoid negative values
// wrapping to huge positive numbers.
template <typename T, typename U, bool IsTU = std::is_unsigned_v<T>,
          bool IsUU = std::is_unsigned_v<U>>
struct SafeCmpHelper;

template <typename T, typename U>
struct SafeCmpHelper<T, U, false, false> {
  static constexpr bool Eq(T a, U b) { return a == b; }
  static constexpr bool Ge(T a, U b) { return a >= b; }
  static constexpr bool Gt(T a, U b) { return a > b; }
  static constexpr bool Le(T a, U b) { return a <= b; }
  static constexpr bool Lt(T a, U b) { return a < b; }
};

template <typename T, typename U>
struct SafeCmpHelper<T, U, true, false> {
  using S = std::make_signed_t<T>;
  static constexpr bool Eq(T a, U b) { return static_cast<S>(a) == b; }
  static constexpr bool Ge(T a, U b) { return static_cast<S>(a) >= b; }
  static constexpr bool Gt(T a, U b) { return static_cast<S>(a) > b; }
  static constexpr bool Le(T a, U b) { return static_cast<S>(a) <= b; }
  static constexpr bool Lt(T a, U b) { return static_cast<S>(a) < b; }
};

template <typename T, typename U>
struct SafeCmpHelper<T, U, false, true> {
  using S = std::make_signed_t<U>;
  static constexpr bool Eq(T a, U b) { return a == static_cast<S>(b); }
  static constexpr bool Ge(T a, U b) { return a >= static_cast<S>(b); }
  static constexpr bool Gt(T a, U b) { return a > static_cast<S>(b); }
  static constexpr bool Le(T a, U b) { return a <= static_cast<S>(b); }
  static constexpr bool Lt(T a, U b) { return a < static_cast<S>(b); }
};

template <typename T, typename U>
struct SafeCmpHelper<T, U, true, true> {
  static constexpr bool Eq(T a, U b) { return a == b; }
  static constexpr bool Ge(T a, U b) { return a >= b; }
  static constexpr bool Gt(T a, U b) { return a > b; }
  static constexpr bool Le(T a, U b) { return a <= b; }
  static constexpr bool Lt(T a, U b) { return a < b; }
};

template <typename T, typename U>
constexpr bool SafeCmpEq(T a, U b) { return SafeCmpHelper<T, U>::Eq(a, b); }
template <typename T, typename U>
constexpr bool SafeCmpNe(T a, U b) { return !(a == b); }
template <typename T, typename U>
constexpr bool SafeCmpLe(T a, U b) { return SafeCmpHelper<T, U>::Le(a, b); }
template <typename T, typename U>
constexpr bool SafeCmpLt(T a, U b) { return SafeCmpHelper<T, U>::Lt(a, b); }
template <typename T, typename U>
constexpr bool SafeCmpGe(T a, U b) { return SafeCmpHelper<T, U>::Ge(a, b); }
template <typename T, typename U>
constexpr bool SafeCmpGt(T a, U b) { return SafeCmpHelper<T, U>::Gt(a, b); }

#define RTC_CHECK_EQ(v1, v2) RTC_CHECK(SafeCmpEq(v1, v2))
#define RTC_CHECK_NE(v1, v2) RTC_CHECK(SafeCmpNe(v1, v2))
#define RTC_CHECK_LE(v1, v2) RTC_CHECK(SafeCmpLe(v1, v2))
#define RTC_CHECK_LT(v1, v2) RTC_CHECK(SafeCmpLt(v1, v2))
#define RTC_CHECK_GE(v1, v2) RTC_CHECK(SafeCmpGe(v1, v2))
#define RTC_CHECK_GT(v1, v2) RTC_CHECK(SafeCmpGt(v1, v2))

#if RTC_DCHECK_IS_ON
#define RTC_DCHECK(condition) RTC_CHECK(condition)
#define RTC_DCHECK_EQ(v1, v2) RTC_CHECK_EQ(v1, v2)
#define RTC_DCHECK_NE(v1, v2) RTC_CHECK_NE(v1, v2)
#define RTC_DCHECK_LE(v1, v2) RTC_CHECK_LE(v1, v2)
#define RTC_DCHECK_LT(v1, v2) RTC_CHECK_LT(v1, v2)
#define RTC_DCHECK_GE(v1, v2) RTC_CHECK_GE(v1, v2)
#define RTC_DCHECK_GT(v1, v2) RTC_CHECK_GT(v1, v2)
#else
#define RTC_DCHECK(condition) \
  (true) ? (void)0 : FatalLogCall{__FILE__, __LINE__, #condition} & std::cerr
#define RTC_DCHECK_EQ(v1, v2) RTC_DCHECK((v1) == (v2))
#define RTC_DCHECK_NE(v1, v2) RTC_DCHECK((v1) != (v2))
#define RTC_DCHECK_LE(v1, v2) RTC_DCHECK((v1) <= (v2))
#define RTC_DCHECK_LT(v1, v2) RTC_DCHECK((v1) < (v2))
#define RTC_DCHECK_GE(v1, v2) RTC_DCHECK((v1) >= (v2))
#define RTC_DCHECK_GT(v1, v2) RTC_DCHECK((v1) > (v2))
#endif

#define RTC_CHECK_NOTREACHED() \
  (fprintf(stderr, "NOTREACHED at %s:%d\n", __FILE__, __LINE__), abort(), (void)0)
#define RTC_DCHECK_NOTREACHED() RTC_DCHECK(false)
#define RTC_FATAL() \
  (fprintf(stderr, "FATAL at %s:%d\n", __FILE__, __LINE__), abort(), (void)0)

template <typename T>
inline T CheckedDivExact(T a, T b) {
  RTC_CHECK_EQ(a % b, 0) << a << " is not evenly divisible by " << b;
  return a / b;
}

#else  // __cplusplus

#define RTC_CHECK(condition)                            \
  do {                                                   \
    if (!(condition)) {                                   \
      fprintf(stderr, "CHECK failed: %s\n  at %s:%d\n",  \
              #condition, __FILE__, __LINE__);            \
      abort();                                            \
    }                                                     \
  } while (0)

#define RTC_CHECK_EQ(a, b) RTC_CHECK((a) == (b))
#define RTC_CHECK_NE(a, b) RTC_CHECK((a) != (b))
#define RTC_CHECK_LE(a, b) RTC_CHECK((a) <= (b))
#define RTC_CHECK_LT(a, b) RTC_CHECK((a) < (b))
#define RTC_CHECK_GE(a, b) RTC_CHECK((a) >= (b))
#define RTC_CHECK_GT(a, b) RTC_CHECK((a) > (b))

#if RTC_DCHECK_IS_ON
#define RTC_DCHECK(condition) RTC_CHECK(condition)
#define RTC_DCHECK_EQ(a, b) RTC_CHECK_EQ(a, b)
#define RTC_DCHECK_NE(a, b) RTC_CHECK_NE(a, b)
#define RTC_DCHECK_LE(a, b) RTC_CHECK_LE(a, b)
#define RTC_DCHECK_LT(a, b) RTC_CHECK_LT(a, b)
#define RTC_DCHECK_GE(a, b) RTC_CHECK_GE(a, b)
#define RTC_DCHECK_GT(a, b) RTC_CHECK_GT(a, b)
#else
#define RTC_DCHECK(condition) ((void)0)
#define RTC_DCHECK_EQ(a, b) ((void)0)
#define RTC_DCHECK_NE(a, b) ((void)0)
#define RTC_DCHECK_LE(a, b) ((void)0)
#define RTC_DCHECK_LT(a, b) ((void)0)
#define RTC_DCHECK_GE(a, b) ((void)0)
#define RTC_DCHECK_GT(a, b) ((void)0)
#endif

#define RTC_CHECK_NOTREACHED() \
  (fprintf(stderr, "NOTREACHED at %s:%d\n", __FILE__, __LINE__), abort(), (void)0)
#define RTC_DCHECK_NOTREACHED() RTC_DCHECK(0)
#define RTC_DCHECK_RUNS_SERIALIZED() ((void)0)
#define RTC_FATAL() \
  (fprintf(stderr, "FATAL at %s:%d\n", __FILE__, __LINE__), abort(), (void)0)

#endif  // __cplusplus

#endif  // RTC_BASE_CHECKS_H_
