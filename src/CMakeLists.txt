# ============================================================================
# trickroom — chat-room client application (Windows)
#   Builds the game layer against the static libAE_* archives from audio_engine
#   and the third-party submodules at the repository root (openal-soft, opus).
#
#   Prerequisites (built once, out of tree):
#     1. audio engine:  cmake -S src/audio_engine -B src/audio_engine/build ...
#                       cmake --build src/audio_engine/build -j
#     2. openal-soft:   cmake -S third_party/openal-soft -B third_party/openal-soft/build \
#                         -G "MinGW Makefiles" -DCMAKE_BUILD_TYPE=Release -DLIBTYPE=STATIC \
#                         -DALSOFT_EXAMPLES=OFF -DALSOFT_TESTS=OFF -DALSOFT_UTILS=OFF -DALSOFT_INSTALL=OFF
#                       cmake --build third_party/openal-soft/build -j
#     3. opus:          cmake -S third_party/opus -B third_party/opus/build \
#                         -G "MinGW Makefiles" -DCMAKE_BUILD_TYPE=Release \
#                         -DOPUS_BUILD_PROGRAMS=OFF -DOPUS_BUILD_TESTING=OFF
#                       cmake --build third_party/opus/build -j
#
#   Build:              cmake -S src -B src/build -G "MinGW Makefiles"
#                       cmake --build src/build -j
#   Output:             bin/trickroom.exe
# ============================================================================
cmake_minimum_required(VERSION 3.10.0)

project(TrickRoomProject)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED True)

# Repository root: third_party submodules, audio engine outputs, bin/
get_filename_component(REPO_ROOT "${CMAKE_SOURCE_DIR}/.." ABSOLUTE)
set(THIRD_PARTY_ROOT "${REPO_ROOT}/third_party")
set(AUDIO_ENGINE_ROOT "${CMAKE_SOURCE_DIR}/audio_engine")

# absl (same install tree the audio engine was built against)
set(CMAKE_PREFIX_PATH
    "${THIRD_PARTY_ROOT}/abseil-cpp/install"
    ${CMAKE_PREFIX_PATH}
)
find_package(absl REQUIRED)

add_executable(trickroom WIN32
    audio.cpp
    voice.cpp
    winmain.cpp
    misc.cpp
    queue.cpp
    sock.cpp
    trickroom.cpp
)

target_compile_definitions(trickroom PRIVATE
    AUDIO_ENGINE_STATIC
    _CRT_SECURE_NO_WARNINGS
    AL_LIBTYPE_STATIC    # openal-soft static build: no dllimport on AL symbols
)

target_include_directories(trickroom PRIVATE
    "${CMAKE_SOURCE_DIR}"
    "${AUDIO_ENGINE_ROOT}"
    "${THIRD_PARTY_ROOT}/openal-soft/include"
    "${THIRD_PARTY_ROOT}/opus/include"
)

target_link_libraries(trickroom PRIVATE
    # Audio pipeline libs (AEC → NS → AGC2 → SRC)
    "${AUDIO_ENGINE_ROOT}/lib/libAE_AEC.a"
    "${AUDIO_ENGINE_ROOT}/lib/libAE_NS.a"
    "${AUDIO_ENGINE_ROOT}/lib/libAE_AGC2.a"
    "${AUDIO_ENGINE_ROOT}/lib/libAE_SRC.a"
    absl::strings
    # Codecs / playback
    "${THIRD_PARTY_ROOT}/openal-soft/build/libOpenAL32.a"
    "${THIRD_PARTY_ROOT}/opus/build/libopus.a"
    # Win32: sockets, VFW camera, GDI, OpenAL backends
    ws2_32 vfw32 gdi32 user32 kernel32 winmm ole32 shell32 advapi32 uuid avrt
)

# Standalone exe: no libgcc/libstdc++ DLLs needed at runtime
if(MINGW)
    target_link_options(trickroom PRIVATE -static-libgcc -static-libstdc++)
endif()

# Same location as the prebuilt game binaries
set_target_properties(trickroom PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${REPO_ROOT}/bin"
)
