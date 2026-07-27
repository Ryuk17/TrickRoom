#ifndef RTC_BASE_CPU_INFO_H_
#define RTC_BASE_CPU_INFO_H_

namespace webrtc {
namespace cpu_info {

enum class ISA {
  kSSE2,
  kAVX2,
  kFMA3,
};

// Minimal stub — always returns false.
// Real runtime detection can be added if needed.
inline bool Supports(ISA /* isa */) {
  return false;
}

}  // namespace cpu_info
}  // namespace webrtc

#endif  // RTC_BASE_CPU_INFO_H_
