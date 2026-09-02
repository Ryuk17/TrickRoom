# ============================================================================
# TrickRoom — top-level super build
#
#   Fetches third-party dependencies via CMake FetchContent (URL tarball +
#   SHA256 checksum — no git clone, no git submodules), builds the
#   audio-engine libraries (libAE_*) and, optionally, the Windows-only
#   trickroom chat client.
#
#   Options:
#     BUILD_AUDIO_ENGINE   Build the audio engine libraries     (default ON)
#     BUILD_TRICKROOM      Build the trickroom client, Windows  (default OFF)
#
#   Build (audio engine only):
#     cmake -S . -B build && cmake --build build -j
#
#   Build (trickroom, MinGW on Windows):
#     cmake -S . -B build -G "MinGW Makefiles" -DBUILD_TRICKROOM=ON
#     cmake --build build -j
# ============================================================================
cmake_minimum_required(VERSION 3.16)

project(TrickRoomSuperBuild LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED True)

option(BUILD_AUDIO_ENGINE "Build the audio engine libraries (libAE_*)" ON)
option(BUILD_TRICKROOM    "Build the trickroom chat client (Windows only)" OFF)

enable_testing()

# trickroom depends on the audio engine libraries
if(BUILD_TRICKROOM AND NOT BUILD_AUDIO_ENGINE)
    set(BUILD_AUDIO_ENGINE ON CACHE BOOL "trickroom requires the audio engine" FORCE)
endif()

include(FetchContent)

# ----------------------------------------------------------------------------
# Third-party dependencies (URL + SHA256, no git clone)
# ----------------------------------------------------------------------------
set(ABSEIL_VERSION "20240116.2")
set(ABSEIL_URL     "https://github.com/abseil/abseil-cpp/archive/refs/tags/${ABSEIL_VERSION}.tar.gz")
set(ABSEIL_HASH    "SHA256=733726b8c3a6d39a4120d7e45ea8b41a434cdacde401cba500f14236c49b39dc")

set(NEON_FFT_VERSION "v0.1.0")
set(NEON_FFT_URL     "https://github.com/Ryuk17/neon-fft/archive/refs/tags/${NEON_FFT_VERSION}.tar.gz")
set(NEON_FFT_HASH    "SHA256=4b557d080a09c9eb5d1852b32daa8f022816772c58a9addc19c032f5b89a30d4")

set(PFFFT_VERSION "0aec0327a6912e1a0ec5326eef737c2ce19bc836")  # jpommier/pffft @ master
set(PFFFT_URL     "https://bitbucket.org/jpommier/pffft/get/${PFFFT_VERSION}.tar.gz")
set(PFFFT_HASH    "SHA256=29b3b0bf83ace5e32ad4fb26a99fe13284240a4d4f1b762970aa19c3821ddb1f")

set(EIGEN_VERSION "3.4.0")
set(EIGEN_URL     "https://gitlab.com/libeigen/eigen/-/archive/${EIGEN_VERSION}/eigen-${EIGEN_VERSION}.tar.gz")
set(EIGEN_HASH    "SHA256=8586084f71f9bde545ee7fa6d00288b264a2b7ac3607b974e54d13e7162c1c72")

set(OPENAL_VERSION "openal-soft-1.21.0")
set(OPENAL_URL     "https://github.com/kcat/openal-soft/archive/refs/tags/${OPENAL_VERSION}.tar.gz")
set(OPENAL_HASH    "SHA256=cd3650530866f3906058225f4bfbe0052be19e0a29dcc6df185a460f9948feec")

set(OPUS_VERSION "v1.5.2")
set(OPUS_URL     "https://github.com/xiph/opus/archive/refs/tags/${OPUS_VERSION}.tar.gz")
set(OPUS_HASH    "SHA256=9480e329e989f70d69886ded470c7f8cfe6c0667cc4196d4837ac9e668fb7404")

# ----------------------------------------------------------------------------
# Audio engine dependencies (abseil + source-only FFT / eigen)
# ----------------------------------------------------------------------------
if(BUILD_AUDIO_ENGINE)
    FetchContent_Declare(abseil-cpp
        URL      ${ABSEIL_URL}
        URL_HASH ${ABSEIL_HASH}
    )
    FetchContent_MakeAvailable(abseil-cpp)   # provides absl::strings

    FetchContent_Declare(neon-fft
        URL      ${NEON_FFT_URL}
        URL_HASH ${NEON_FFT_HASH}
    )
    FetchContent_Declare(pffft
        URL      ${PFFFT_URL}
        URL_HASH ${PFFFT_HASH}
    )
    FetchContent_Declare(eigen
        URL      ${EIGEN_URL}
        URL_HASH ${EIGEN_HASH}
    )

    # neon-fft / pffft / eigen are compiled straight into the libAE_* libraries,
    # so we only need their sources, not their own CMake targets.
    if(POLICY CMP0169)
        cmake_policy(SET CMP0169 OLD)
    endif()
    foreach(dep neon-fft pffft eigen)
        FetchContent_GetProperties(${dep})
        if(NOT ${dep}_POPULATED)
            FetchContent_Populate(${dep})
        endif()
    endforeach()

    # Hand the fetched source dirs to src/audio_engine.
    set(NEON_FFT_SOURCE_DIR "${neon-fft_SOURCE_DIR}")
    set(PFFFT_SOURCE_DIR    "${pffft_SOURCE_DIR}")
    set(EIGEN3_ROOT         "${eigen_SOURCE_DIR}" CACHE PATH "Eigen source root" FORCE)

    add_subdirectory(src/audio_engine)
endif()

# ----------------------------------------------------------------------------
# trickroom chat client (Windows only)
# ----------------------------------------------------------------------------
if(BUILD_TRICKROOM)
    if(NOT WIN32)
        message(FATAL_ERROR "BUILD_TRICKROOM is only supported on Windows (WIN32).")
    endif()

    FetchContent_Declare(openal-soft
        URL      ${OPENAL_URL}
        URL_HASH ${OPENAL_HASH}
    )
    FetchContent_Declare(opus
        URL      ${OPUS_URL}
        URL_HASH ${OPUS_HASH}
    )

    set(LIBTYPE             STATIC CACHE STRING "" FORCE)
    set(ALSOFT_EXAMPLES     OFF    CACHE BOOL   "" FORCE)
    set(ALSOFT_UTILS        OFF    CACHE BOOL   "" FORCE)
    set(ALSOFT_INSTALL      OFF    CACHE BOOL   "" FORCE)
    set(OPUS_BUILD_PROGRAMS OFF    CACHE BOOL   "" FORCE)
    set(OPUS_BUILD_TESTING  OFF    CACHE BOOL   "" FORCE)

    FetchContent_MakeAvailable(openal-soft opus)

    add_executable(trickroom WIN32
        src/audio.cpp
        src/voice.cpp
        src/winmain.cpp
        src/misc.cpp
        src/queue.cpp
        src/sock.cpp
        src/trickroom.cpp
    )

    target_compile_definitions(trickroom PRIVATE
        AUDIO_ENGINE_STATIC
        _CRT_SECURE_NO_WARNINGS
    )

    target_include_directories(trickroom PRIVATE
        "${CMAKE_SOURCE_DIR}/src"
        "${CMAKE_SOURCE_DIR}/src/audio_engine"
    )

    target_link_libraries(trickroom PRIVATE
        # Audio pipeline libs (AEC → NS → AGC2 → SRC)
        AE_AEC
        AE_NS
        AE_AGC2
        AE_SRC
        absl::strings
        # Codecs / playback
        OpenAL
        opus
        # Win32: sockets, VFW camera, GDI, OpenAL backends
        ws2_32 vfw32 gdi32 user32 kernel32 winmm ole32 shell32 advapi32 uuid avrt
    )

    # Standalone exe: no libgcc/libstdc++ DLLs needed at runtime
    if(MINGW)
        target_link_options(trickroom PRIVATE -static-libgcc -static-libstdc++)
    endif()

    set_target_properties(trickroom PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/bin"
    )
endif()
