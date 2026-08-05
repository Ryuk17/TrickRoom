/*
 * Minimal thread annotation stubs (no-ops in pure build, single-threaded).
 */
#ifndef RTC_BASE_THREAD_ANNOTATIONS_H_
#define RTC_BASE_THREAD_ANNOTATIONS_H_

#define RTC_GUARDED_BY(x)
#define RTC_PT_GUARDED_BY(x)
#define RTC_NO_THREAD_SAFETY_ANALYSIS

#endif  // RTC_BASE_THREAD_ANNOTATIONS_H_
