#ifndef RTC_BASE_SYSTEM_IGNORE_WARNINGS_H_
#define RTC_BASE_SYSTEM_IGNORE_WARNINGS_H_

// Minimal stub — provides warning suppression macros
// The original WebRTC header uses pragma push/pop patterns.
// For this pure build, we provide empty no-ops.

#if defined(__GNUC__)
#define RTC_PUSH_IGNORING_WFRAME_LARGER_THAN(x) \
  _Pragma("GCC diagnostic push")                 \
  _Pragma("GCC diagnostic ignored \"-Wframe-larger-than=\"")
#define RTC_POP_IGNORING_WFRAME_LARGER_THAN() \
  _Pragma("GCC diagnostic pop")
#elif defined(_MSC_VER)
#define RTC_PUSH_IGNORING_WFRAME_LARGER_THAN(x) \
  __pragma(warning(push))
#define RTC_POP_IGNORING_WFRAME_LARGER_THAN() \
  __pragma(warning(pop))
#else
#define RTC_PUSH_IGNORING_WFRAME_LARGER_THAN(x)
#define RTC_POP_IGNORING_WFRAME_LARGER_THAN()
#endif

#endif  // RTC_BASE_SYSTEM_IGNORE_WARNINGS_H_
