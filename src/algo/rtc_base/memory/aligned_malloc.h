#ifndef RTC_BASE_MEMORY_ALIGNED_MALLOC_H_
#define RTC_BASE_MEMORY_ALIGNED_MALLOC_H_

#include <cstdlib>
#include <malloc.h>

namespace webrtc {

inline void* GetRightAlign(const void* ptr, size_t alignment) {
  return reinterpret_cast<void*>(
      (reinterpret_cast<uintptr_t>(ptr) + alignment - 1) & ~(alignment - 1));
}

inline void* AlignedMalloc(size_t size, size_t alignment) {
  return _aligned_malloc(size, alignment);
}
inline void AlignedFree(void* mem_block) {
  _aligned_free(mem_block);
}

template <typename T>
T* GetRightAlign(const T* ptr, size_t alignment) {
  return reinterpret_cast<T*>(
      GetRightAlign(reinterpret_cast<const void*>(ptr), alignment));
}
template <typename T>
T* AlignedMalloc(size_t size, size_t alignment) {
  return reinterpret_cast<T*>(AlignedMalloc(size, alignment));
}

struct AlignedFreeDeleter {
  inline void operator()(void* ptr) const { AlignedFree(ptr); }
};

}  // namespace webrtc

#endif  // RTC_BASE_MEMORY_ALIGNED_MALLOC_H_
