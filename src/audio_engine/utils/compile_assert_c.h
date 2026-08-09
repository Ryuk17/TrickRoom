#ifndef RTC_BASE_COMPILE_ASSERT_C_H_
#define RTC_BASE_COMPILE_ASSERT_C_H_

// For C code, use _Static_assert (C11) or a simple static_assert wrapper
#ifdef __cplusplus
#define RTC_COMPILE_ASSERT(expr) static_assert(expr, #expr)
#else
#define RTC_COMPILE_ASSERT(expr) _Static_assert(expr, #expr)
#endif

#endif  // RTC_BASE_COMPILE_ASSERT_C_H_
