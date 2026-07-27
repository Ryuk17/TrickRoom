#ifndef RTC_BASE_SYSTEM_ARCH_H_
#define RTC_BASE_SYSTEM_ARCH_H_

// Architecture detection macros
#if defined(__x86_64__) || defined(_M_X64) || defined(__i386__) || defined(_M_IX86)
#define WEBRTC_ARCH_X86_FAMILY
#endif

#if defined(__ARM_NEON__) || defined(__ARM_NEON)
#define WEBRTC_HAS_NEON
#endif

#if defined(__ARM_ARCH_7A__) || defined(__ARM_ARCH_7R__)
#define WEBRTC_ARCH_ARM_V7
#endif

#endif  // RTC_BASE_SYSTEM_ARCH_H_
