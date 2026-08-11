# 设置最低版本要求
cmake_minimum_required(VERSION 3.10.0)

# 设置项目名称
project(AuarProject)

if(CMAKE_TOOLCHAIN_FILE)
    get_filename_component(CMAKE_TOOLCHAIN_FILE_NAME ${CMAKE_TOOLCHAIN_FILE} NAME)
    find_file(CMAKE_TOOLCHAIN_FILE ${CMAKE_TOOLCHAIN_FILE_NAME} PATHS ${CMAKE_SOURCE_DIR} NO_DEFAULT_PATH)
    message(STATUS "CMAKE_TOOLCHAIN_FILE = ${CMAKE_TOOLCHAIN_FILE}")
endif()

# 设置 C++ 标准
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED True)

set(LIBRARY_OUTPUT_PATH ${PROJECT_SOURCE_DIR}/lib)
set(EXECUTABLE_OUTPUT_PATH ${PROJECT_SOURCE_DIR}/bin)

set(CMAKE_VERBOSE_MAKEFILE ON)
set(CMAKE_PREFIX_PATH
    "${PROJECT_SOURCE_DIR}/third_party/abseil-cpp/install"
    ${CMAKE_PREFIX_PATH}
)

find_package(absl REQUIRED)

# Disable WebRTC metrics (no-op stubs)
add_definitions(-DRTC_DISABLE_METRICS)

# ============================================================================
# Build options — one per module (controls lib + interface test)
# ============================================================================
option(BUILD_AE_VAD  "Build libAE_VAD and test_ae_vad"   ON)
option(BUILD_AE_SRC  "Build libAE_SRC and test_ae_src"   ON)
option(BUILD_AE_NS   "Build libAE_NS and test_ae_ns"     ON)
option(BUILD_AE_AGC  "Build libAE_AGC and test_ae_agc"   ON)
option(BUILD_AE_AGC2 "Build libAE_AGC2 and test_ae_agc2" ON)
option(BUILD_AE_AECM "Build libAE_AECM and test_ae_aecm" ON)
option(BUILD_AE_AEC  "Build libAE_AEC and test_ae_aec"   ON)

# ============================================================================
# Helper: register an interface test executable
#   usage: add_ae_test(<test_name> <test_source> <lib_target>)
# ============================================================================
macro(add_ae_test TEST_NAME TEST_SRC LIB_TARGET)
    add_executable(${TEST_NAME} "${PROJECT_SOURCE_DIR}/unitest/${TEST_SRC}")

    target_include_directories(${TEST_NAME} PRIVATE
        "${PROJECT_SOURCE_DIR}"
        "${PROJECT_SOURCE_DIR}/interface/"
        "${PROJECT_SOURCE_DIR}/utils/"
    )

    target_compile_definitions(${TEST_NAME} PRIVATE AUDIO_ENGINE_STATIC)

    if(WIN32 AND MINGW)
        target_link_libraries(${TEST_NAME} PRIVATE
            ${LIB_TARGET}
            absl::strings
            winmm
        )
    else()
        target_link_libraries(${TEST_NAME} PRIVATE
            ${LIB_TARGET}
            absl::strings
        )
    endif()

    add_test(NAME ${TEST_NAME} COMMAND ${TEST_NAME})
endmacro()

# ============================================================================
# Common source groups (shared across multiple libraries)
# ============================================================================
set(SIGNAL_PROCESSING_COMMON
    "${PROJECT_SOURCE_DIR}/signal_processing/spl_init.c"
    "${PROJECT_SOURCE_DIR}/signal_processing/spl_inl.c"
    "${PROJECT_SOURCE_DIR}/signal_processing/spl_sqrt.c"
    "${PROJECT_SOURCE_DIR}/signal_processing/spl_sqrt_floor.c"
    "${PROJECT_SOURCE_DIR}/signal_processing/copy_set_operations.c"
    "${PROJECT_SOURCE_DIR}/signal_processing/dot_product_with_scale.cc"
    "${PROJECT_SOURCE_DIR}/signal_processing/division_operations.c"
    "${PROJECT_SOURCE_DIR}/signal_processing/min_max_operations.c"
    "${PROJECT_SOURCE_DIR}/signal_processing/vector_operations.c"
    "${PROJECT_SOURCE_DIR}/signal_processing/vector_scaling_operations.c"
)

set(SIGNAL_PROCESSING_RESAMPLE
    "${PROJECT_SOURCE_DIR}/signal_processing/energy.c"
    "${PROJECT_SOURCE_DIR}/signal_processing/resample.c"
    "${PROJECT_SOURCE_DIR}/signal_processing/resample_fractional.c"
    "${PROJECT_SOURCE_DIR}/signal_processing/resample_48khz.c"
    "${PROJECT_SOURCE_DIR}/signal_processing/resample_by_2.c"
    "${PROJECT_SOURCE_DIR}/signal_processing/resample_by_2_internal.c"
    "${PROJECT_SOURCE_DIR}/signal_processing/get_scaling_square.c"
    "${PROJECT_SOURCE_DIR}/signal_processing/downsample_fast.c"
    "${PROJECT_SOURCE_DIR}/signal_processing/filter_ar.c"
)

set(NE10_FFT_SRC
    "${PROJECT_SOURCE_DIR}/third_party/neon-fft/src/NE10_fft.c"
    "${PROJECT_SOURCE_DIR}/third_party/neon-fft/src/NE10_fft_float32.c"
    "${PROJECT_SOURCE_DIR}/third_party/neon-fft/src/NE10_fft_generic_float32.c"
    "${PROJECT_SOURCE_DIR}/third_party/neon-fft/src/NE10_rfft_float32.c"
)

set(NE10_FFT_INCLUDE
    "${PROJECT_SOURCE_DIR}/third_party/neon-fft/include/"
)

# ---- Resampler sources (used by modules that need AudioBuffer) ----
file(GLOB RESAMPLER_SRC "${PROJECT_SOURCE_DIR}/audio_processing/resample/*.cc")
list(FILTER RESAMPLER_SRC EXCLUDE REGEX ".*_neon\\.cc$")

# ---- Common include directories for libraries ----
set(LIB_COMMON_INCLUDES
    "${PROJECT_SOURCE_DIR}"
    "${PROJECT_SOURCE_DIR}/interface/"
    "${PROJECT_SOURCE_DIR}/utils/"
    "${PROJECT_SOURCE_DIR}/signal_processing/"
    "${PROJECT_SOURCE_DIR}/signal_processing/include/"
    "${CMAKE_PREFIX_PATH}/include/"
)

# ---- Common utils ----
set(UTILS_WAV
    "${PROJECT_SOURCE_DIR}/utils/dr_wav.cc"
    "${PROJECT_SOURCE_DIR}/utils/audio_util.cc"
)

set(UTILS_AUDIO_BUFFER_AND_SPLITTING
    "${PROJECT_SOURCE_DIR}/utils/audio_buffer.cc"
    "${PROJECT_SOURCE_DIR}/signal_processing/splitting_filter.cc"
    "${PROJECT_SOURCE_DIR}/signal_processing/three_band_filter_bank.cc"
)

# ============================================================================
# libAE_VAD + test_ae_vad
# ============================================================================
if(BUILD_AE_VAD)
    add_library(AE_VAD STATIC
        "${PROJECT_SOURCE_DIR}/interface/audio_engine_vad.cpp"
        ${UTILS_WAV}
        "${PROJECT_SOURCE_DIR}/utils/string_builder.cc"
        ${NE10_FFT_SRC}
        ${SIGNAL_PROCESSING_COMMON}
        ${SIGNAL_PROCESSING_RESAMPLE}
        "${PROJECT_SOURCE_DIR}/signal_processing/splitting_filter.c"
        "${PROJECT_SOURCE_DIR}/signal_processing/randomization_functions.c"
        "${PROJECT_SOURCE_DIR}/signal_processing/cross_correlation.c"
        "${PROJECT_SOURCE_DIR}/audio_processing/vad/vad_core/webrtc_vad.c"
        "${PROJECT_SOURCE_DIR}/audio_processing/vad/vad_core/vad_core.c"
        "${PROJECT_SOURCE_DIR}/audio_processing/vad/vad_core/vad_filterbank.c"
        "${PROJECT_SOURCE_DIR}/audio_processing/vad/vad_core/vad_gmm.c"
        "${PROJECT_SOURCE_DIR}/audio_processing/vad/vad_core/vad_sp.c"
        ${UTILS_AUDIO_BUFFER_AND_SPLITTING}
    )

    file(GLOB VAD_SRC "${PROJECT_SOURCE_DIR}/audio_processing/vad/*.cc")
    file(GLOB ISAC_VAD_SRC "${PROJECT_SOURCE_DIR}/audio_processing/vad/isac/main/source/*.c")
    target_sources(AE_VAD PRIVATE ${VAD_SRC} ${ISAC_VAD_SRC} ${RESAMPLER_SRC})

    set_source_files_properties(
        "${PROJECT_SOURCE_DIR}/audio_processing/resample/sinc_resampler_avx2.cc"
        PROPERTIES COMPILE_FLAGS "-mavx2 -mfma"
    )

    target_include_directories(AE_VAD PUBLIC
        ${LIB_COMMON_INCLUDES}
        "${PROJECT_SOURCE_DIR}/audio_processing/resample/"
        ${NE10_FFT_INCLUDE}
    )
    target_link_libraries(AE_VAD PUBLIC absl::strings)
    target_compile_definitions(AE_VAD PRIVATE AUDIO_ENGINE_EXPORTS)
    set_target_properties(AE_VAD PROPERTIES ARCHIVE_OUTPUT_DIRECTORY "${PROJECT_SOURCE_DIR}/lib")

    add_ae_test(test_ae_vad "test_ae_vad.cc" AE_VAD)
endif()

# ============================================================================
# libAE_SRC + test_ae_src
# ============================================================================
if(BUILD_AE_SRC)
    add_library(AE_SRC STATIC
        "${PROJECT_SOURCE_DIR}/interface/audio_engine_src.cpp"
        ${UTILS_WAV}
        ${SIGNAL_PROCESSING_COMMON}
        ${SIGNAL_PROCESSING_RESAMPLE}
        "${PROJECT_SOURCE_DIR}/signal_processing/splitting_filter.c"
    )

    target_sources(AE_SRC PRIVATE ${RESAMPLER_SRC})

    set_source_files_properties(
        "${PROJECT_SOURCE_DIR}/audio_processing/resample/sinc_resampler_avx2.cc"
        PROPERTIES COMPILE_FLAGS "-mavx2 -mfma"
    )

    target_include_directories(AE_SRC PUBLIC
        ${LIB_COMMON_INCLUDES}
        "${PROJECT_SOURCE_DIR}/audio_processing/resample/"
    )
    target_link_libraries(AE_SRC PUBLIC absl::strings)
    target_compile_definitions(AE_SRC PRIVATE AUDIO_ENGINE_EXPORTS)
    set_target_properties(AE_SRC PROPERTIES ARCHIVE_OUTPUT_DIRECTORY "${PROJECT_SOURCE_DIR}/lib")

    add_ae_test(test_ae_src "test_ae_src.cc" AE_SRC)
endif()

# ============================================================================
# libAE_NS + test_ae_ns
# ============================================================================
if(BUILD_AE_NS)
    add_library(AE_NS STATIC
        "${PROJECT_SOURCE_DIR}/interface/audio_engine_ns.cpp"
        ${UTILS_WAV}
        ${NE10_FFT_SRC}
        ${SIGNAL_PROCESSING_COMMON}
        ${SIGNAL_PROCESSING_RESAMPLE}
        "${PROJECT_SOURCE_DIR}/signal_processing/splitting_filter.c"
        ${UTILS_AUDIO_BUFFER_AND_SPLITTING}
    )

    file(GLOB NS_SRC "${PROJECT_SOURCE_DIR}/audio_processing/ns/*.cc")
    target_sources(AE_NS PRIVATE ${NS_SRC} ${RESAMPLER_SRC})

    target_include_directories(AE_NS PUBLIC
        ${LIB_COMMON_INCLUDES}
        "${PROJECT_SOURCE_DIR}/audio_processing/resample/"
        ${NE10_FFT_INCLUDE}
    )
    target_link_libraries(AE_NS PUBLIC absl::strings)
    target_compile_definitions(AE_NS PRIVATE AUDIO_ENGINE_EXPORTS)
    set_target_properties(AE_NS PROPERTIES ARCHIVE_OUTPUT_DIRECTORY "${PROJECT_SOURCE_DIR}/lib")

    add_ae_test(test_ae_ns "test_ae_ns.cc" AE_NS)
endif()

# ============================================================================
# libAE_AGC + test_ae_agc
# ============================================================================
if(BUILD_AE_AGC)
    add_library(AE_AGC STATIC
        "${PROJECT_SOURCE_DIR}/interface/audio_engine_agc.cpp"
        ${UTILS_WAV}
        ${SIGNAL_PROCESSING_COMMON}
        ${SIGNAL_PROCESSING_RESAMPLE}
    )

    file(GLOB AGC_SRC "${PROJECT_SOURCE_DIR}/audio_processing/agc/*.cc")
    target_sources(AE_AGC PRIVATE ${AGC_SRC})

    target_include_directories(AE_AGC PUBLIC ${LIB_COMMON_INCLUDES})
    target_link_libraries(AE_AGC PUBLIC absl::strings)
    target_compile_definitions(AE_AGC PRIVATE AUDIO_ENGINE_EXPORTS)
    set_target_properties(AE_AGC PROPERTIES ARCHIVE_OUTPUT_DIRECTORY "${PROJECT_SOURCE_DIR}/lib")

    add_ae_test(test_ae_agc "test_ae_agc.cc" AE_AGC)
endif()

# ============================================================================
# libAE_AGC2 + test_ae_agc2
# ============================================================================
if(BUILD_AE_AGC2)
    add_library(AE_AGC2 STATIC
        "${PROJECT_SOURCE_DIR}/interface/audio_engine_agc2.cpp"
        ${UTILS_WAV}
        "${PROJECT_SOURCE_DIR}/utils/string_builder.cc"
        ${SIGNAL_PROCESSING_COMMON}
        ${SIGNAL_PROCESSING_RESAMPLE}
        "${PROJECT_SOURCE_DIR}/signal_processing/splitting_filter.c"
        ${UTILS_AUDIO_BUFFER_AND_SPLITTING}
        "${PROJECT_SOURCE_DIR}/model_weights/rnnoise/src/rnn_vad_weights.cc"
        "${PROJECT_SOURCE_DIR}/signal_processing/pffft_wrapper.cc"
        "${PROJECT_SOURCE_DIR}/third_party/pffft/pffft.c"
        "${PROJECT_SOURCE_DIR}/third_party/pffft/fftpack.c"
    )

    file(GLOB AGC2_CORE "${PROJECT_SOURCE_DIR}/audio_processing/agc2/*.cc")
    file(GLOB RNN_VAD_SRC "${PROJECT_SOURCE_DIR}/audio_processing/agc2/rnn_vad/*.cc")
    target_sources(AE_AGC2 PRIVATE ${AGC2_CORE} ${RNN_VAD_SRC} ${RESAMPLER_SRC})

    set_source_files_properties(
        "${PROJECT_SOURCE_DIR}/audio_processing/agc2/rnn_vad/vector_math_avx2.cc"
        "${PROJECT_SOURCE_DIR}/audio_processing/resample/sinc_resampler_avx2.cc"
        PROPERTIES COMPILE_FLAGS "-mavx2 -mfma"
    )

    target_include_directories(AE_AGC2 PUBLIC
        ${LIB_COMMON_INCLUDES}
        "${PROJECT_SOURCE_DIR}/audio_processing/resample/"
        "${PROJECT_SOURCE_DIR}/third_party/pffft/"
    )
    target_link_libraries(AE_AGC2 PUBLIC absl::strings)
    target_compile_definitions(AE_AGC2 PRIVATE AUDIO_ENGINE_EXPORTS)
    set_target_properties(AE_AGC2 PROPERTIES ARCHIVE_OUTPUT_DIRECTORY "${PROJECT_SOURCE_DIR}/lib")

    add_ae_test(test_ae_agc2 "test_ae_agc2.cc" AE_AGC2)
endif()

# ============================================================================
# libAE_AECM + test_ae_aecm
# ============================================================================
if(BUILD_AE_AECM)
    add_library(AE_AECM STATIC
        "${PROJECT_SOURCE_DIR}/interface/audio_engine_aecm.cpp"
        ${UTILS_WAV}
        ${NE10_FFT_SRC}
        ${SIGNAL_PROCESSING_COMMON}
        "${PROJECT_SOURCE_DIR}/signal_processing/downsample_fast.c"
        "${PROJECT_SOURCE_DIR}/signal_processing/randomization_functions.c"
        "${PROJECT_SOURCE_DIR}/signal_processing/cross_correlation.c"
        "${PROJECT_SOURCE_DIR}/utils/ring_buffer.c"
        "${PROJECT_SOURCE_DIR}/signal_processing/delay_estimator.cc"
        "${PROJECT_SOURCE_DIR}/signal_processing/delay_estimator_wrapper.cc"
    )

    file(GLOB AECM_SRC "${PROJECT_SOURCE_DIR}/audio_processing/aecm/*.cc")
    list(FILTER AECM_SRC EXCLUDE REGEX ".*_mips\\.cc$")
    list(FILTER AECM_SRC EXCLUDE REGEX ".*_neon\\.cc$")
    target_sources(AE_AECM PRIVATE ${AECM_SRC})

    target_include_directories(AE_AECM PUBLIC
        ${LIB_COMMON_INCLUDES}
        ${NE10_FFT_INCLUDE}
    )
    target_link_libraries(AE_AECM PUBLIC absl::strings)
    target_compile_definitions(AE_AECM PRIVATE AUDIO_ENGINE_EXPORTS)
    set_target_properties(AE_AECM PROPERTIES ARCHIVE_OUTPUT_DIRECTORY "${PROJECT_SOURCE_DIR}/lib")

    add_ae_test(test_ae_aecm "test_ae_aecm.cc" AE_AECM)
endif()

# ============================================================================
# libAE_AEC + test_ae_aec
# ============================================================================
if(BUILD_AE_AEC)
    add_library(AE_AEC STATIC
        "${PROJECT_SOURCE_DIR}/interface/audio_engine_aec.cpp"
        ${UTILS_WAV}
        "${PROJECT_SOURCE_DIR}/utils/string_builder.cc"
        "${PROJECT_SOURCE_DIR}/audio_processing/aec3/echo_canceller3_config.cc"
        ${NE10_FFT_SRC}
        ${SIGNAL_PROCESSING_COMMON}
        ${SIGNAL_PROCESSING_RESAMPLE}
        "${PROJECT_SOURCE_DIR}/signal_processing/splitting_filter.c"
        ${UTILS_AUDIO_BUFFER_AND_SPLITTING}
        "${PROJECT_SOURCE_DIR}/signal_processing/high_pass_filter.cc"
        "${PROJECT_SOURCE_DIR}/signal_processing/cascaded_biquad_filter.cc"
    )

    file(GLOB AEC3_SRC "${PROJECT_SOURCE_DIR}/audio_processing/aec3/*.cc")
    list(FILTER AEC3_SRC EXCLUDE REGEX ".*neural_feature_extractor\\.cc$")
    list(FILTER AEC3_SRC EXCLUDE REGEX ".*neural_residual_echo_estimator_impl\\.cc$")
    target_sources(AE_AEC PRIVATE ${AEC3_SRC} ${RESAMPLER_SRC})

    set_source_files_properties(
        "${PROJECT_SOURCE_DIR}/audio_processing/aec3/adaptive_fir_filter_avx2.cc"
        "${PROJECT_SOURCE_DIR}/audio_processing/aec3/adaptive_fir_filter_erl_avx2.cc"
        "${PROJECT_SOURCE_DIR}/audio_processing/aec3/fft_data_avx2.cc"
        "${PROJECT_SOURCE_DIR}/audio_processing/aec3/matched_filter_avx2.cc"
        "${PROJECT_SOURCE_DIR}/audio_processing/aec3/vector_math_avx2.cc"
        "${PROJECT_SOURCE_DIR}/audio_processing/resample/sinc_resampler_avx2.cc"
        PROPERTIES COMPILE_FLAGS "-mavx2 -mfma"
    )

    target_include_directories(AE_AEC PUBLIC
        ${LIB_COMMON_INCLUDES}
        "${PROJECT_SOURCE_DIR}/audio_processing/resample/"
        ${NE10_FFT_INCLUDE}
    )
    target_link_libraries(AE_AEC PUBLIC absl::strings)
    target_compile_definitions(AE_AEC PRIVATE AUDIO_ENGINE_EXPORTS)
    set_target_properties(AE_AEC PROPERTIES ARCHIVE_OUTPUT_DIRECTORY "${PROJECT_SOURCE_DIR}/lib")

    add_ae_test(test_ae_aec "test_ae_aec.cc" AE_AEC)
endif()
