function(add_vad_test_module)
    set(LIB_NAME "vad_test_lib")

    add_library(${LIB_NAME} STATIC
        # WAV I/O utilities
        "${PROJECT_SOURCE_DIR}/utils/dr_wav.cc"
        "${PROJECT_SOURCE_DIR}/utils/audio_util.cc"

        # String builder (used by WebRTC code)
        "${PROJECT_SOURCE_DIR}/utils/string_builder.cc"

        # NE10 FFT (generic C implementation, no NEON)
        "${PROJECT_SOURCE_DIR}/common/neon_fft/src/NE10_fft.c"
        "${PROJECT_SOURCE_DIR}/common/neon_fft/src/NE10_fft_float32.c"
        "${PROJECT_SOURCE_DIR}/common/neon_fft/src/NE10_fft_generic_float32.c"
        "${PROJECT_SOURCE_DIR}/common/neon_fft/src/NE10_rfft_float32.c"

        # Signal processing library
        "${PROJECT_SOURCE_DIR}/common/signal_processing/energy.c"
        "${PROJECT_SOURCE_DIR}/common/signal_processing/resample.c"
        "${PROJECT_SOURCE_DIR}/common/signal_processing/resample_fractional.c"
        "${PROJECT_SOURCE_DIR}/common/signal_processing/resample_48khz.c"
        "${PROJECT_SOURCE_DIR}/common/signal_processing/resample_by_2.c"
        "${PROJECT_SOURCE_DIR}/common/signal_processing/resample_by_2_internal.c"
        "${PROJECT_SOURCE_DIR}/common/signal_processing/splitting_filter.c"
        "${PROJECT_SOURCE_DIR}/common/signal_processing/division_operations.c"
        "${PROJECT_SOURCE_DIR}/common/signal_processing/get_scaling_square.c"
        "${PROJECT_SOURCE_DIR}/common/signal_processing/min_max_operations.c"
        "${PROJECT_SOURCE_DIR}/common/signal_processing/spl_init.c"
        "${PROJECT_SOURCE_DIR}/common/signal_processing/spl_inl.c"
        "${PROJECT_SOURCE_DIR}/common/signal_processing/spl_sqrt.c"
        "${PROJECT_SOURCE_DIR}/common/signal_processing/spl_sqrt_floor.c"
        "${PROJECT_SOURCE_DIR}/common/signal_processing/vector_operations.c"
        "${PROJECT_SOURCE_DIR}/common/signal_processing/copy_set_operations.c"
        "${PROJECT_SOURCE_DIR}/common/signal_processing/downsample_fast.c"
        "${PROJECT_SOURCE_DIR}/common/signal_processing/filter_ar.c"
        "${PROJECT_SOURCE_DIR}/common/signal_processing/vector_scaling_operations.c"
        "${PROJECT_SOURCE_DIR}/common/signal_processing/dot_product_with_scale.cc"
        "${PROJECT_SOURCE_DIR}/common/signal_processing/randomization_functions.c"
        "${PROJECT_SOURCE_DIR}/common/signal_processing/cross_correlation.c"

        # Low-level WebRTC VAD (vad_core)
        "${PROJECT_SOURCE_DIR}/audio_processing/vad/vad_core/webrtc_vad.c"
        "${PROJECT_SOURCE_DIR}/audio_processing/vad/vad_core/vad_core.c"
        "${PROJECT_SOURCE_DIR}/audio_processing/vad/vad_core/vad_filterbank.c"
        "${PROJECT_SOURCE_DIR}/audio_processing/vad/vad_core/vad_gmm.c"
        "${PROJECT_SOURCE_DIR}/audio_processing/vad/vad_core/vad_sp.c"
    )

    # High-level VAD module sources
    file(GLOB VAD_SRC "${PROJECT_SOURCE_DIR}/audio_processing/vad/*.cc")
    target_sources(${LIB_NAME} PRIVATE ${VAD_SRC})

    # ISAC VAD sources (C files)
    file(GLOB ISAC_VAD_SRC "${PROJECT_SOURCE_DIR}/audio_processing/vad/isac/main/source/*.c")
    target_sources(${LIB_NAME} PRIVATE ${ISAC_VAD_SRC})

    # Resampler sources (needed by voice_activity_detector)
    file(GLOB RESAMPLER_SRC "${PROJECT_SOURCE_DIR}/audio_processing/resample/*.cc")
    list(FILTER RESAMPLER_SRC EXCLUDE REGEX ".*_neon\\.cc$")
    target_sources(${LIB_NAME} PRIVATE ${RESAMPLER_SRC})

    # Include directories
    target_include_directories(${LIB_NAME} PUBLIC
        "${PROJECT_SOURCE_DIR}"
        "${PROJECT_SOURCE_DIR}/utils/"
        "${PROJECT_SOURCE_DIR}/audio_processing/resample/"
        "${PROJECT_SOURCE_DIR}/common/signal_processing/"
        "${PROJECT_SOURCE_DIR}/common/signal_processing/include/"
        "${PROJECT_SOURCE_DIR}/common/neon_fft/include/"
        "${CMAKE_PREFIX_PATH}/include/"
    )

    # Link abseil for header access
    target_link_libraries(${LIB_NAME} PUBLIC absl::strings)

    # --- Test executable ---
    set(TEST_NAME "test_vad")

    add_executable(${TEST_NAME} "${PROJECT_SOURCE_DIR}/test/test_vad.cc")

    if(WIN32)
        if(MINGW)
            target_link_libraries(${TEST_NAME} PRIVATE
                ${LIB_NAME}
                absl::strings
                winmm
            )
        endif()
    else()
        target_link_libraries(${TEST_NAME} PRIVATE
            ${LIB_NAME}
            absl::strings
        )
    endif()

    add_test(NAME ${TEST_NAME} COMMAND ${TEST_NAME})
endfunction()
