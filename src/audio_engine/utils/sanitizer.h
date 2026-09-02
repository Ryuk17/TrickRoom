#ifndef RTC_BASE_SANITIZER_H_
#define RTC_BASE_SANITIZER_H_

#include <stddef.h>  // For size_t / ptrdiff_t

// rtc_base/sanitizer.h — stub
// Original defines RTC_NO_SANITIZE macros and MSan helper functions.
// These are hints for the compiler; no-ops in this pure build.

#ifdef __cplusplus
extern "C" {
#endif

// Memory sanitizer check — no-op stub
static inline void rtc_MsanCheckInitialized(const volatile void* ptr,
                                            size_t element_size,
                                            size_t element_count) {
  (void)ptr;
  (void)element_size;
  (void)element_count;
}

#ifdef __cplusplus
}
#endif

#if defined(__has_attribute)
#if __has_attribute(no_sanitize)
#define RTC_NO_SANITIZE(sanitizer) __attribute__((no_sanitize(sanitizer)))
#endif
#endif

#ifndef RTC_NO_SANITIZE
#define RTC_NO_SANITIZE(sanitizer)
#endif

#endif  // RTC_BASE_SANITIZER_H_
