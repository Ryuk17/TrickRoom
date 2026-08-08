#ifndef RTC_BASE_GTEST_PROD_UTIL_H_
#define RTC_BASE_GTEST_PROD_UTIL_H_

// Test production utility macros. Without gtest, these are no-ops.
#define FRIEND_TEST_ALL_PREFIXES(test_case_name, test_name) \
  friend class test_case_name##_##test_name##_Test

#endif  // RTC_BASE_GTEST_PROD_UTIL_H_
