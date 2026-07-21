#ifndef RTC_BASE_LOGGING_H_
#define RTC_BASE_LOGGING_H_

#include <iostream>

#define RTC_LOG(sev) \
  while (false)      \
  std::cerr
#define RTC_LOG_IF(sev, c) \
  while (false)            \
  std::cerr
#define RTC_LOG_V(sev) RTC_LOG(sev)
#define RTC_LOG_F(sev) RTC_LOG(sev)
#define RTC_LOG_ERRNO(sev) RTC_LOG(sev)
#define RTC_LOG_T(sev) RTC_LOG(sev)
#define RTC_LOG_E(sev, ctx, err) RTC_LOG(sev)
#define RTC_DLOG(sev) RTC_LOG(sev)

namespace webrtc {
enum LoggingSeverity { LS_VERBOSE, LS_INFO, LS_WARNING, LS_ERROR, LS_NONE };
}  // namespace webrtc

#endif  // RTC_BASE_LOGGING_H_
