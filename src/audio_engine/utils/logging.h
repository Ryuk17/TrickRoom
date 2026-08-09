#ifndef RTC_BASE_LOGGING_H_
#define RTC_BASE_LOGGING_H_

#include <iostream>

// Log severity levels from WebRTC
enum LoggingSeverity {
  LS_VERBOSE,
  LS_INFO,
  LS_WARNING,
  LS_ERROR,
  LS_NONE,
};

// Minimal logging macros
#define RTC_LOG(severity) \
  ::webrtc::LogMessage(__FILE__, __LINE__, severity).stream()

#define RTC_LOG_ERR(severity) RTC_LOG(severity)
#define RTC_LOG_V(severity) RTC_LOG(severity)

namespace webrtc {

class LogMessage {
 public:
  LogMessage(const char* file, int line, LoggingSeverity sev)
      : severity_(sev) {
    stream() << "[" << file << ":" << line << "] ";
  }
  ~LogMessage() { std::cerr << std::endl; }
  std::ostream& stream() { return std::cerr; }

 private:
  LoggingSeverity severity_;
};

}  // namespace webrtc

#endif  // RTC_BASE_LOGGING_H_
