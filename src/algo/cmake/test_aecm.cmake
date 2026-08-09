function(add_aecm_test_module)
    set(LIB_NAME "aecm_test_lib")

    add_library(${LIB_NAME} STATIC
        # WAV I/O utilities
        "${PROJECT_SOURCE_DIR}/utils/dr_wav.cc"
        "${PROJECT_SOURCE_DIR}/utils/audio_util.cc"

        # NE10 FFT (generic C implementation, 128-point)
        "${PROJECT_SOURCE_DIR}/third_party/neon_fft/src/NE10_fft.c"
        "${PROJECT_SOURCE_DIR}/third_party/neon_fft/src/NE10_fft_float32.c"
        "${PROJECT_SOURCE_DIR}/third_party/neon_fft/src/NE10_fft_generic_float32.c"
        "${PROJECT_SOURCE_DIR}/third_party/neon_fft/src/NE10_rfft_float32.c"

        # Signal processing library
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
        "${PROJECT_SOURCE_DIR}/signal_processing/randomization_functions.c"
        "${PROJECT_SOURCE_DIR}/signal_processing/cross_correlation.c"
        "${PROJECT_SOURCE_DIR}/signal_processing/downsample_fast.c"

        # Ring buffer
        "${PROJECT_SOURCE_DIR}/utils/ring_buffer.c"

        # Delay estimator
        "${PROJECT_SOURCE_DIR}/signal_processing/delay_estimator.cc"
        "${PROJECT_SOURCE_DIR}/signal_processing/delay_estimator_wrapper.cc"
    )

    # AECM sources (exclude MIPS/NEON platform-specific files on x86)
    file(GLOB AECM_SRC "${PROJECT_SOURCE_DIR}/audio_processing/aecm/*.cc")
    list(FILTER AECM_SRC EXCLUDE REGEX ".*_mips\\.cc$")
    list(FILTER AECM_SRC EXCLUDE REGEX ".*_neon\\.cc$")
    target_sources(${LIB_NAME} PRIVATE ${AECM_SRC})

    # Include directories
    target_include_directories(${LIB_NAME} PUBLIC
        "${PROJECT_SOURCE_DIR}"
        "${PROJECT_SOURCE_DIR}/utils/"
        "${PROJECT_SOURCE_DIR}/signal_processing/"
        "${PROJECT_SOURCE_DIR}/signal_processing/include/"
        "${PROJECT_SOURCE_DIR}/third_party/neon_fft/include/"
        "${CMAKE_PREFIX_PATH}/include/"
    )

    # Link abseil for header access
    target_link_libraries(${LIB_NAME} PUBLIC absl::strings)

    # --- Test executable ---
    set(TEST_NAME "test_aecm")

    add_executable(${TEST_NAME} "${PROJECT_SOURCE_DIR}/test/test_aecm.cc")

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
