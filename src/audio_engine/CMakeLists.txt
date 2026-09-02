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

enable_testing()

set(LIBRARY_OUTPUT_PATH ${PROJECT_SOURCE_DIR}/lib)
set(EXECUTABLE_OUTPUT_PATH ${PROJECT_SOURCE_DIR}/bin)

# Third-party dependencies live at the repository root (git submodule layout).
# The top-level super build fetches them via FetchContent and hands the source
# dirs / absl targets down; these fallbacks keep standalone builds working.
set(THIRD_PARTY_ROOT "${PROJECT_SOURCE_DIR}/../../third_party")

if(NOT DEFINED NEON_FFT_SOURCE_DIR)
    set(NEON_FFT_SOURCE_DIR "${THIRD_PARTY_ROOT}/neon-fft")
endif()
if(NOT DEFINED PFFFT_SOURCE_DIR)
    set(PFFFT_SOURCE_DIR "${THIRD_PARTY_ROOT}/pffft")
endif()

# Vendored sources include "third_party/pffft/pffft.h" (WebRTC layout); bridge
# that path to wherever PFFFT_SOURCE_DIR actually lives (submodule or FetchContent).
set(PFFFT_COMPAT_DIR "${CMAKE_CURRENT_BINARY_DIR}/pffft_compat/third_party/pffft")
file(MAKE_DIRECTORY "${PFFFT_COMPAT_DIR}/src")
file(WRITE "${PFFFT_COMPAT_DIR}/pffft.h"
     "#include \"${PFFFT_SOURCE_DIR}/pffft.h\"\n")
file(WRITE "${PFFFT_COMPAT_DIR}/src/pffft.h"
     "#include \"${PFFFT_SOURCE_DIR}/pffft.h\"\n")

set(CMAKE_VERBOSE_MAKEFILE ON)

# absl: the top-level super build provides absl::strings via FetchContent;
# fall back to find_package for standalone builds.
if(NOT TARGET absl::strings)
    set(CMAKE_PREFIX_PATH
        "${THIRD_PARTY_ROOT}/abseil-cpp/install"
        ${CMAKE_PREFIX_PATH}
    )
    find_package(absl REQUIRED)
endif()

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
option(BUILD_AE_BF   "Build libAE_BF and test_ae_bf"     ON)
option(BUILD_AE_HS   "Build libAE_HS and test_ae_hs"     ON)
option(BUILD_AE_IE   "Build libAE_IE and test_ae_ie"     ON)
option(BUILD_AE_TS   "Build libAE_TS and test_ae_ts"     ON)
option(BUILD_AE_DR   "Build libAE_DR and test_ae_dr"     ON)

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

    # Tests read data/... relative to the audio_engine source dir
    add_test(NAME ${TEST_NAME} COMMAND ${TEST_NAME}
             WORKING_DIRECTORY "${PROJECT_SOURCE_DIR}")
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
    "${NEON_FFT_SOURCE_DIR}/src/NE10_fft.c"
    "${NEON_FFT_SOURCE_DIR}/src/NE10_fft_float32.c"
    "${NEON_FFT_SOURCE_DIR}/src/NE10_fft_generic_float32.c"
    "${NEON_FFT_SOURCE_DIR}/src/NE10_rfft_float32.c"
)

set(NE10_FFT_INCLUDE
    "${NEON_FFT_SOURCE_DIR}/include/"
)

# ---- Resampler sources (used by modules that need AudioBuffer) ----
file(GLOB RESAMPLER_SRC "${PROJECT_SOURCE_DIR}/audio_processing/sample_rate_conversion/*.cc")
list(FILTER RESAMPLER_SRC EXCLUDE REGEX ".*_neon\\.cc$")

# sinc_resampler needs AlignedMalloc/AlignedFree (utils/aligned_malloc.cc)
set(RESAMPLER_DEPS "${PROJECT_SOURCE_DIR}/utils/aligned_malloc.cc")

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
        "${PROJECT_SOURCE_DIR}/audio_processing/voice_activity_detection/vad_core/webrtc_vad.c"
        "${PROJECT_SOURCE_DIR}/audio_processing/voice_activity_detection/vad_core/vad_core.c"
        "${PROJECT_SOURCE_DIR}/audio_processing/voice_activity_detection/vad_core/vad_filterbank.c"
        "${PROJECT_SOURCE_DIR}/audio_processing/voice_activity_detection/vad_core/vad_gmm.c"
        "${PROJECT_SOURCE_DIR}/audio_processing/voice_activity_detection/vad_core/vad_sp.c"
        ${UTILS_AUDIO_BUFFER_AND_SPLITTING}
    )

    file(GLOB VAD_SRC "${PROJECT_SOURCE_DIR}/audio_processing/voice_activity_detection/*.cc")
    file(GLOB ISAC_VAD_SRC "${PROJECT_SOURCE_DIR}/audio_processing/voice_activity_detection/isac/main/source/*.c")
    target_sources(AE_VAD PRIVATE ${VAD_SRC} ${ISAC_VAD_SRC} ${RESAMPLER_SRC} ${RESAMPLER_DEPS})

    set_source_files_properties(
        "${PROJECT_SOURCE_DIR}/audio_processing/sample_rate_conversion/sinc_resampler_avx2.cc"
        PROPERTIES COMPILE_FLAGS "-mavx2 -mfma"
    )

    target_include_directories(AE_VAD PUBLIC
        ${LIB_COMMON_INCLUDES}
        "${PROJECT_SOURCE_DIR}/audio_processing/sample_rate_conversion/"
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

    target_sources(AE_SRC PRIVATE ${RESAMPLER_SRC} ${RESAMPLER_DEPS})

    set_source_files_properties(
        "${PROJECT_SOURCE_DIR}/audio_processing/sample_rate_conversion/sinc_resampler_avx2.cc"
        PROPERTIES COMPILE_FLAGS "-mavx2 -mfma"
    )

    target_include_directories(AE_SRC PUBLIC
        ${LIB_COMMON_INCLUDES}
        "${PROJECT_SOURCE_DIR}/audio_processing/sample_rate_conversion/"
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

    file(GLOB NS_SRC "${PROJECT_SOURCE_DIR}/audio_processing/noise_suppressor/*.cc")
    target_sources(AE_NS PRIVATE ${NS_SRC} ${RESAMPLER_SRC} ${RESAMPLER_DEPS})

    target_include_directories(AE_NS PUBLIC
        ${LIB_COMMON_INCLUDES}
        "${PROJECT_SOURCE_DIR}/audio_processing/sample_rate_conversion/"
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

    file(GLOB AGC_SRC "${PROJECT_SOURCE_DIR}/audio_processing/automatic_gain_control/*.cc")
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
        "${PFFFT_SOURCE_DIR}/pffft.c"
        "${PFFFT_SOURCE_DIR}/fftpack.c"
    )

    file(GLOB AGC2_CORE "${PROJECT_SOURCE_DIR}/audio_processing/automatic_gain_control2/*.cc")
    file(GLOB RNN_VAD_SRC "${PROJECT_SOURCE_DIR}/audio_processing/automatic_gain_control2/rnn_vad/*.cc")
    target_sources(AE_AGC2 PRIVATE ${AGC2_CORE} ${RNN_VAD_SRC} ${RESAMPLER_SRC} ${RESAMPLER_DEPS})

    set_source_files_properties(
        "${PROJECT_SOURCE_DIR}/audio_processing/automatic_gain_control2/rnn_vad/vector_math_avx2.cc"
        "${PROJECT_SOURCE_DIR}/audio_processing/sample_rate_conversion/sinc_resampler_avx2.cc"
        PROPERTIES COMPILE_FLAGS "-mavx2 -mfma"
    )

    target_include_directories(AE_AGC2 PUBLIC
        ${LIB_COMMON_INCLUDES}
        "${PROJECT_SOURCE_DIR}/audio_processing/sample_rate_conversion/"
        "${PFFFT_SOURCE_DIR}/"
        "${CMAKE_CURRENT_BINARY_DIR}/pffft_compat"
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

    file(GLOB AECM_SRC "${PROJECT_SOURCE_DIR}/audio_processing/acoustic_echo_cancellation_mobile/*.cc")
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
        "${PROJECT_SOURCE_DIR}/audio_processing/acoustic_echo_cancellation3/echo_canceller3_config.cc"
        ${NE10_FFT_SRC}
        ${SIGNAL_PROCESSING_COMMON}
        ${SIGNAL_PROCESSING_RESAMPLE}
        "${PROJECT_SOURCE_DIR}/signal_processing/splitting_filter.c"
        ${UTILS_AUDIO_BUFFER_AND_SPLITTING}
        "${PROJECT_SOURCE_DIR}/signal_processing/high_pass_filter.cc"
        "${PROJECT_SOURCE_DIR}/signal_processing/cascaded_biquad_filter.cc"
    )

    file(GLOB AEC3_SRC "${PROJECT_SOURCE_DIR}/audio_processing/acoustic_echo_cancellation3/*.cc")
    list(FILTER AEC3_SRC EXCLUDE REGEX ".*neural_feature_extractor\\.cc$")
    list(FILTER AEC3_SRC EXCLUDE REGEX ".*neural_residual_echo_estimator_impl\\.cc$")
    target_sources(AE_AEC PRIVATE ${AEC3_SRC} ${RESAMPLER_SRC} ${RESAMPLER_DEPS})

    set_source_files_properties(
        "${PROJECT_SOURCE_DIR}/audio_processing/acoustic_echo_cancellation3/adaptive_fir_filter_avx2.cc"
        "${PROJECT_SOURCE_DIR}/audio_processing/acoustic_echo_cancellation3/adaptive_fir_filter_erl_avx2.cc"
        "${PROJECT_SOURCE_DIR}/audio_processing/acoustic_echo_cancellation3/fft_data_avx2.cc"
        "${PROJECT_SOURCE_DIR}/audio_processing/acoustic_echo_cancellation3/matched_filter_avx2.cc"
        "${PROJECT_SOURCE_DIR}/audio_processing/acoustic_echo_cancellation3/vector_math_avx2.cc"
        "${PROJECT_SOURCE_DIR}/audio_processing/sample_rate_conversion/sinc_resampler_avx2.cc"
        PROPERTIES COMPILE_FLAGS "-mavx2 -mfma"
    )

    target_include_directories(AE_AEC PUBLIC
        ${LIB_COMMON_INCLUDES}
        "${PROJECT_SOURCE_DIR}/audio_processing/sample_rate_conversion/"
        ${NE10_FFT_INCLUDE}
    )
    target_link_libraries(AE_AEC PUBLIC absl::strings)
    target_compile_definitions(AE_AEC PRIVATE AUDIO_ENGINE_EXPORTS)
    set_target_properties(AE_AEC PROPERTIES ARCHIVE_OUTPUT_DIRECTORY "${PROJECT_SOURCE_DIR}/lib")

    add_ae_test(test_ae_aec "test_ae_aec.cc" AE_AEC)
endif()

# ============================================================================
# libAE_BF + test_ae_bf
# ============================================================================
if(BUILD_AE_BF)
    add_library(AE_BF STATIC
        "${PROJECT_SOURCE_DIR}/interface/audio_engine_bf.cpp"
        ${UTILS_WAV}
        "${PROJECT_SOURCE_DIR}/utils/string_builder.cc"
        ${NE10_FFT_SRC}
        ${SIGNAL_PROCESSING_COMMON}
        ${SIGNAL_PROCESSING_RESAMPLE}
        "${PROJECT_SOURCE_DIR}/signal_processing/splitting_filter.c"
        "${PROJECT_SOURCE_DIR}/signal_processing/randomization_functions.c"
        ${UTILS_AUDIO_BUFFER_AND_SPLITTING}
        # BF core sources
        "${PROJECT_SOURCE_DIR}/audio_processing/beamforming/nonlinear_beamformer.cc"
        "${PROJECT_SOURCE_DIR}/audio_processing/beamforming/array_util.cc"
        "${PROJECT_SOURCE_DIR}/audio_processing/beamforming/covariance_matrix_generator.cc"
        # BF dependencies (common_audio)
        "${PROJECT_SOURCE_DIR}/signal_processing/lapped_transform.cc"
        "${PROJECT_SOURCE_DIR}/signal_processing/window_generator.cc"
        "${PROJECT_SOURCE_DIR}/utils/blocker.cc"
        "${PROJECT_SOURCE_DIR}/utils/ring_buffer.c"
        "${PROJECT_SOURCE_DIR}/utils/audio_ring_buffer.cc"
        # RealFourier (NE10-based R2C/C2R FFT)
        "${PROJECT_SOURCE_DIR}/signal_processing/real_fourier.cc"
        "${PROJECT_SOURCE_DIR}/utils/aligned_malloc.cc"
    )

    target_sources(AE_BF PRIVATE ${RESAMPLER_SRC})

    set_source_files_properties(
        "${PROJECT_SOURCE_DIR}/audio_processing/sample_rate_conversion/sinc_resampler_avx2.cc"
        PROPERTIES COMPILE_FLAGS "-mavx2 -mfma"
    )

    target_include_directories(AE_BF PUBLIC
        ${LIB_COMMON_INCLUDES}
        "${PROJECT_SOURCE_DIR}/audio_processing/beamforming/"
        "${PROJECT_SOURCE_DIR}/audio_processing/sample_rate_conversion/"
        ${NE10_FFT_INCLUDE}
    )
    target_link_libraries(AE_BF PUBLIC absl::strings)
    target_compile_definitions(AE_BF PRIVATE AUDIO_ENGINE_EXPORTS)
    set_target_properties(AE_BF PROPERTIES ARCHIVE_OUTPUT_DIRECTORY "${PROJECT_SOURCE_DIR}/lib")

    add_ae_test(test_ae_bf "test_ae_bf.cc" AE_BF)
endif()

# ============================================================================
# libAE_HS + test_ae_hs
# ============================================================================
if(BUILD_AE_HS)
    add_library(AE_HS STATIC
        "${PROJECT_SOURCE_DIR}/interface/audio_engine_hs.cpp"
        ${UTILS_WAV}
        "${PROJECT_SOURCE_DIR}/audio_processing/howling_suppressor/howling_core.c"
        "${PROJECT_SOURCE_DIR}/audio_processing/howling_suppressor/biquad.c"
        "${PROJECT_SOURCE_DIR}/audio_processing/howling_suppressor/tone_detect.c"
    )

    target_include_directories(AE_HS PUBLIC
        ${LIB_COMMON_INCLUDES}
        "${PROJECT_SOURCE_DIR}/audio_processing/howling_suppressor/"
    )
    target_link_libraries(AE_HS PUBLIC absl::strings)
    target_compile_definitions(AE_HS PRIVATE AUDIO_ENGINE_EXPORTS)
    set_target_properties(AE_HS PROPERTIES ARCHIVE_OUTPUT_DIRECTORY "${PROJECT_SOURCE_DIR}/lib")

    add_ae_test(test_ae_hs "test_ae_hs.cc" AE_HS)
endif()

# ============================================================================
# libAE_IE + test_ae_ie
# ============================================================================
if(BUILD_AE_IE)
    add_library(AE_IE STATIC
        "${PROJECT_SOURCE_DIR}/interface/audio_engine_ie.cpp"
        ${UTILS_WAV}
        "${PROJECT_SOURCE_DIR}/utils/string_builder.cc"
        ${NE10_FFT_SRC}
        ${SIGNAL_PROCESSING_COMMON}
        ${SIGNAL_PROCESSING_RESAMPLE}
        "${PROJECT_SOURCE_DIR}/signal_processing/splitting_filter.c"
        "${PROJECT_SOURCE_DIR}/signal_processing/randomization_functions.c"
        ${UTILS_AUDIO_BUFFER_AND_SPLITTING}
        # IE core sources
        "${PROJECT_SOURCE_DIR}/audio_processing/intelligibility_enhancer/intelligibility_enhancer.cc"
        "${PROJECT_SOURCE_DIR}/audio_processing/intelligibility_enhancer/intelligibility_utils.cc"
        # Shared deps (same as BF: lapped_transform, real_fourier, window_generator, blocker)
        "${PROJECT_SOURCE_DIR}/signal_processing/lapped_transform.cc"
        "${PROJECT_SOURCE_DIR}/signal_processing/window_generator.cc"
        "${PROJECT_SOURCE_DIR}/utils/blocker.cc"
        "${PROJECT_SOURCE_DIR}/utils/ring_buffer.c"
        "${PROJECT_SOURCE_DIR}/utils/audio_ring_buffer.cc"
        # RealFourier (NE10-based R2C/C2R FFT)
        "${PROJECT_SOURCE_DIR}/signal_processing/real_fourier.cc"
        "${PROJECT_SOURCE_DIR}/utils/aligned_malloc.cc"
    )

    target_sources(AE_IE PRIVATE ${RESAMPLER_SRC})

    set_source_files_properties(
        "${PROJECT_SOURCE_DIR}/audio_processing/sample_rate_conversion/sinc_resampler_avx2.cc"
        PROPERTIES COMPILE_FLAGS "-mavx2 -mfma"
    )

    target_include_directories(AE_IE PUBLIC
        ${LIB_COMMON_INCLUDES}
        "${PROJECT_SOURCE_DIR}/audio_processing/intelligibility_enhancer/"
        "${PROJECT_SOURCE_DIR}/audio_processing/sample_rate_conversion/"
        ${NE10_FFT_INCLUDE}
    )
    target_link_libraries(AE_IE PUBLIC absl::strings)
    target_compile_definitions(AE_IE PRIVATE AUDIO_ENGINE_EXPORTS)
    set_target_properties(AE_IE PROPERTIES ARCHIVE_OUTPUT_DIRECTORY "${PROJECT_SOURCE_DIR}/lib")

    add_ae_test(test_ae_ie "test_ae_ie.cc" AE_IE)
endif()

# ============================================================================
# libAE_TS + test_ae_ts
# ============================================================================
if(BUILD_AE_TS)
    add_library(AE_TS STATIC
        "${PROJECT_SOURCE_DIR}/interface/audio_engine_ts.cpp"
        ${UTILS_WAV}
        ${NE10_FFT_SRC}
        ${SIGNAL_PROCESSING_COMMON}
        "${PROJECT_SOURCE_DIR}/signal_processing/randomization_functions.c"
        # TS core sources
        "${PROJECT_SOURCE_DIR}/audio_processing/transient_suppressor/transient_suppressor.cc"
        "${PROJECT_SOURCE_DIR}/audio_processing/transient_suppressor/transient_detector.cc"
        "${PROJECT_SOURCE_DIR}/audio_processing/transient_suppressor/moving_moments.cc"
        "${PROJECT_SOURCE_DIR}/audio_processing/transient_suppressor/wpd_node.cc"
        "${PROJECT_SOURCE_DIR}/audio_processing/transient_suppressor/wpd_tree.cc"
        # FIR filter (plain C implementation)
        "${PROJECT_SOURCE_DIR}/utils/fir_filter.cc"
    )

    target_include_directories(AE_TS PUBLIC
        ${LIB_COMMON_INCLUDES}
        "${PROJECT_SOURCE_DIR}/audio_processing/transient_suppressor/"
        "${PROJECT_SOURCE_DIR}/audio_processing/noise_suppressor/"
        ${NE10_FFT_INCLUDE}
    )
    target_link_libraries(AE_TS PUBLIC absl::strings)
    target_compile_definitions(AE_TS PRIVATE AUDIO_ENGINE_EXPORTS)
    set_target_properties(AE_TS PROPERTIES ARCHIVE_OUTPUT_DIRECTORY "${PROJECT_SOURCE_DIR}/lib")

    add_ae_test(test_ae_ts "test_ae_ts.cc" AE_TS)
endif()

# ============================================================================
# libAE_DR + test_ae_dr
# ============================================================================
# Eigen (header-only, double-precision FFT required by the dereverberation
# EKF — kept on purpose, see real_fft.h). Vendored copy is used by default;
# override with -DEIGEN3_ROOT=<eigen source root>.
set(EIGEN3_ROOT "" CACHE PATH "Path to Eigen source root (optional, vendored copy used by default)")

if(BUILD_AE_DR)
    find_path(EIGEN3_INCLUDE_DIR "Eigen/Dense"
        HINTS "${THIRD_PARTY_ROOT}/eigen"
              ${EIGEN3_ROOT} $ENV{EIGEN3_ROOT}
        NO_DEFAULT_PATH
    )
    if(NOT EIGEN3_INCLUDE_DIR)
        message(FATAL_ERROR "Eigen not found (BUILD_AE_DR=ON). "
                "Set -DEIGEN3_ROOT=<eigen source root> to point at Eigen.")
    endif()
    message(STATUS "EIGEN3_INCLUDE_DIR = ${EIGEN3_INCLUDE_DIR}")

    add_library(AE_DR STATIC
        "${PROJECT_SOURCE_DIR}/interface/audio_engine_dr.cpp"
        ${UTILS_WAV}
        "${PROJECT_SOURCE_DIR}/audio_processing/dereverberation/dereverberation.cc"
        "${PROJECT_SOURCE_DIR}/audio_processing/dereverberation/gaussian_likelihood.cc"
        "${PROJECT_SOURCE_DIR}/audio_processing/dereverberation/hmm_dictionary.cc"
        "${PROJECT_SOURCE_DIR}/audio_processing/dereverberation/mel_filterbank.cc"
        "${PROJECT_SOURCE_DIR}/audio_processing/dereverberation/real_fft.cc"
    )

    target_include_directories(AE_DR PUBLIC
        ${LIB_COMMON_INCLUDES}
        "${PROJECT_SOURCE_DIR}/audio_processing/"
        ${EIGEN3_INCLUDE_DIR}
    )
    target_link_libraries(AE_DR PUBLIC absl::strings)
    target_compile_definitions(AE_DR PRIVATE AUDIO_ENGINE_EXPORTS)
    set_target_properties(AE_DR PROPERTIES ARCHIVE_OUTPUT_DIRECTORY "${PROJECT_SOURCE_DIR}/lib")

    add_ae_test(test_ae_dr "test_ae_dr.cc" AE_DR)
endif()
