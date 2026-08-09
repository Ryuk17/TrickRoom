#ifndef RTC_BASE_MEMORY_ALIGNED_MALLOC_H_
#define RTC_BASE_MEMORY_ALIGNED_MALLOC_H_

#include <cstdlib>
#include <memory>

#if defined(_WIN32)
#include <malloc.h>
#endif

namespace webrtc {

inline void* AlignedMalloc(size_t size, size_t alignment) {
#if defined(_WIN32)
  // Windows (MSVC and MinGW): use _aligned_malloc from <malloc.h>
  return _aligned_malloc(size, alignment);
#else
  void* ptr = nullptr;
  if (posix_memalign(&ptr, alignment, size) != 0) {
    return nullptr;
  }
  return ptr;
#endif
}

inline void AlignedFree(void* ptr) {
#if defined(_WIN32)
  _aligned_free(ptr);
#else
  free(ptr);
#endif
}

// Deleter for use with std::unique_ptr
struct AlignedFreeDeleter {
  void operator()(void* ptr) const { AlignedFree(ptr); }
};

}  // namespace webrtc

#endif  // RTC_BASE_MEMORY_ALIGNED_MALLOC_H_
