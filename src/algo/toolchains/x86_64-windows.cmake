# cmake .. -G "MinGW Makefiles" -DCMAKE_TOOLCHAIN_FILE="../toolchains/x86_64-windows.cmake"

set(CMAKE_SYSTEM_NAME Windows)
set(CMAKE_SYSTEM_PROCESSOR x86_64)

set(CMAKE_C_COMPILER "gcc")
set(CMAKE_CXX_COMPILER "g++")

set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)

add_definitions(-DWEBRTC_WIN)
# NOTE: Do NOT add -mavx2 -mfma globally. These flags cause the compiler to
# auto-vectorize code using AVX2 instructions, which require 32-byte alignment.
# Stack and heap allocations on Windows/MinGW are only 16-byte aligned, leading
# to SIGSEGV crashes. AVX2+FMA flags are set per-file in the individual
# cmake/test_*.cmake files for source files that explicitly use AVX2 intrinsics.