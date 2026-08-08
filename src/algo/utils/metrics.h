/*
 * Minimal Metrics stub for the pure build.
 * RTC_HISTOGRAM_* macros are no-ops.
 */
#ifndef SYSTEM_WRAPPERS_INCLUDE_METRICS_H_
#define SYSTEM_WRAPPERS_INCLUDE_METRICS_H_

// No-op histogram macros (metrics disabled in pure build).
#define RTC_HISTOGRAM_COUNTS_LINEAR(name, sample, min, max, bucket_count) \
  do { (void)(sample); } while (0)
#define RTC_HISTOGRAM_COUNTS(name, sample, min, max, bucket_count) \
  do { (void)(sample); } while (0)
#define RTC_HISTOGRAM_COUNTS_SPARSE_100(name, sample) \
  do { (void)(sample); } while (0)
#define RTC_HISTOGRAM_COUNTS_100(name, sample) \
  do { (void)(sample); } while (0)
#define RTC_HISTOGRAM_COUNTS_1000(name, sample) \
  do { (void)(sample); } while (0)
#define RTC_HISTOGRAM_COUNTS_10000(name, sample) \
  do { (void)(sample); } while (0)
#define RTC_HISTOGRAM_ENUMERATION(name, sample, boundary) \
  do { (void)(sample); } while (0)
#define RTC_HISTOGRAM_BOOLEAN(name, sample) \
  do { (void)(sample); } while (0)
#define RTC_HISTOGRAM_PERCENTAGE(name, sample) \
  do { (void)(sample); } while (0)
#define RTC_HISTOGRAM_MEMORY_KB(name, sample) \
  do { (void)(sample); } while (0)
#define RTC_HISTOGRAM_MEMORY_LARGE_KB(name, sample) \
  do { (void)(sample); } while (0)
#define RTC_HISTOGRAM_TIME(name, sample, min, max, bucket_count) \
  do { (void)(sample); } while (0)

#endif  // SYSTEM_WRAPPERS_INCLUDE_METRICS_H_
