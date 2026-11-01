# ============================================================================
# TrickRoom — top-level super build
#
#   Builds the trickroom Windows chat client. The audio-engine libraries
#   (libAE_*) come from the soundproof repository, fetched here via
#   FetchContent (git). soundproof fetches its own third-party dependencies
#   (abseil-cpp, neon-fft, pffft, eigen) as URL tarballs + SHA256 checksums.
#   openal-soft and opus are fetched here.
#
#   Options:
#     BUILD_AUDIO_ENGINE   Fetch and build soundproof (libAE_*)  (default ON)
#     BUILD_TRICKROOM      Build the trickroom client, Windows  (default OFF)
#
#   Build (trickroom, MinGW on Windows):
#     cmake -S . -B build -G "MinGW Makefiles" -DBUILD_TRICKROOM=ON
#     cmake --build build -j
#
#   Local soundproof development — use a local checkout instead of the git
#   fetch (standard FetchContent source-dir override; GIT_* args are ignored):
#     cmake -S . -B build -G "MinGW Makefiles" -DBUILD_TRICKROOM=ON \
#           -DFETCHCONTENT_SOURCE_DIR_SOUNDPROOF=F:/share/soundproof
# ============================================================================
cmake_minimum_required(VERSION 3.16)

project(TrickRoomSuperBuild LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED True)

option(BUILD_AUDIO_ENGINE "Fetch and build the soundproof audio-engine libraries (libAE_*)" ON)
option(BUILD_TRICKROOM    "Build the trickroom chat client (Windows only)" OFF)

enable_testing()

# trickroom depends on the audio engine libraries
if(BUILD_TRICKROOM AND NOT BUILD_AUDIO_ENGINE)
    set(BUILD_AUDIO_ENGINE ON CACHE BOOL "trickroom requires the audio engine" FORCE)
endif()

include(FetchContent)

# ----------------------------------------------------------------------------
# Audio engine (soundproof — fetched from git)
#
# soundproof is designed for super-build consumption: it defines the AE_VAD
# ... AE_DR static library targets, each with PUBLIC includes
# (<soundproof>/soundproof/{,interface/,utils/,dsp/}) and a PUBLIC link to
# absl::strings. It fetches its own dependencies (abseil-cpp, neon-fft,
# pffft, eigen) into this build tree's _deps when they are not provided.
#
# Local dev: -DFETCHCONTENT_SOURCE_DIR_SOUNDPROOF=F:/share/soundproof
# (the GIT_* arguments are ignored when this override is used).
# ----------------------------------------------------------------------------
if(BUILD_AUDIO_ENGINE)
    FetchContent_Declare(soundproof
        GIT_REPOSITORY https://github.com/Ryuk17/soundproof.git
        GIT_TAG        master
        GIT_SHALLOW    TRUE
    )
    FetchContent_MakeAvailable(soundproof)   # defines AE_VAD ... AE_DR
endif()

# ----------------------------------------------------------------------------
# trickroom chat client (Windows only)
# ----------------------------------------------------------------------------
if(BUILD_TRICKROOM)
    if(NOT WIN32)
        message(FATAL_ERROR "BUILD_TRICKROOM is only supported on Windows (WIN32).")
    endif()

    # ---- openal-soft / opus (URL tarball + SHA256) ----
    # openal-soft 1.21.0 declares cmake_minimum_required(VERSION < 3.5), which
    # CMake >= 4.0 rejects outright. Raise the compatibility floor so the old
    # dependency still configures; drop this once openal-soft is upgraded.
    if(CMAKE_VERSION VERSION_GREATER_EQUAL 4.0)
        set(CMAKE_POLICY_VERSION_MINIMUM 3.5)
    endif()

    set(OPENAL_VERSION "openal-soft-1.21.0")
    set(OPENAL_URL     "https://github.com/kcat/openal-soft/archive/refs/tags/${OPENAL_VERSION}.tar.gz")
    set(OPENAL_HASH    "SHA256=cd3650530866f3906058225f4bfbe0052be19e0a29dcc6df185a460f9948feec")

    set(OPUS_VERSION "v1.5.2")
    set(OPUS_URL     "https://github.com/xiph/opus/archive/refs/tags/${OPUS_VERSION}.tar.gz")
    set(OPUS_HASH    "SHA256=9480e329e989f70d69886ded470c7f8cfe6c0667cc4196d4837ac9e668fb7404")

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

    # The voice pipeline needs these four libAE modules (soundproof builds all
    # twelve by default; fail early with a clear message if one is disabled).
    foreach(ae_lib AE_AEC AE_NS AE_AGC2 AE_SRC)
        if(NOT TARGET ${ae_lib})
            message(FATAL_ERROR "trickroom requires ${ae_lib}, but soundproof did not "
                                "define it (BUILD_AE_* disabled or bad soundproof source)")
        endif()
    endforeach()

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
        "${soundproof_SOURCE_DIR}/soundproof"   # interface/ utils/ (libAE headers)
    )

    target_link_libraries(trickroom PRIVATE
        # Audio pipeline libs (AEC → NS → AGC2 → SRC)
        AE_AEC
        AE_NS
        AE_AGC2
        AE_SRC
        absl::strings   # redundant (AE_* link it PUBLIC) — kept for clarity/robustness
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
