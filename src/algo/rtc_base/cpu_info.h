#ifndef RTC_BASE_CPU_INFO_H_
#define RTC_BASE_CPU_INFO_H_

#include <cstdint>
#include "rtc_base/system/arch.h"

namespace webrtc {
namespace cpu_info {
inline uint32_t DetectNumberOfCores() { return 4; }
enum class ISA { kSSE2 = 0, kSSE3, kAVX2, kFMA3, kNeon };
inline bool Supports(ISA isa) {
  (void)isa;
#if defined(WEBRTC_ARCH_X86_FAMILY)
  return true;
#else
  return false;
#endif
}
}  // namespace cpu_info
}  // namespace webrtc

#endif  // RTC_BASE_CPU_INFO_H_
