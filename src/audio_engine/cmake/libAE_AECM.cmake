function(add_libAE_AECM_target)
    set(LIB_NAME "AE_AECM")

    add_library(${LIB_NAME} STATIC
        # Interface
        "${PROJECT_SOURCE_DIR}/interface/audio_engine_aecm.cpp"

        # WAV I/O utilities
        "${PROJECT_SOURCE_DIR}/utils/dr_wav.cc"
        "${PROJECT_SOURCE_DIR}/utils/audio_util.cc"

        # NE10 FFT (generic C implementation, used by AECM)
        "${PROJECT_SOURCE_DIR}/third_party/neon-fft/src/NE10_fft.c"
        "${PROJECT_SOURCE_DIR}/third_party/neon-fft/src/NE10_fft_float32.c"
        "${PROJECT_SOURCE_DIR}/third_party/neon-fft/src/NE10_fft_generic_float32.c"
        "${PROJECT_SOURCE_DIR}/third_party/neon-fft/src/NE10_rfft_float32.c"

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
        "${PROJECT_SOURCE_DIR}/interface/"
        "${PROJECT_SOURCE_DIR}/utils/"
        "${PROJECT_SOURCE_DIR}/signal_processing/"
        "${PROJECT_SOURCE_DIR}/signal_processing/include/"
        "${PROJECT_SOURCE_DIR}/third_party/neon-fft/include/"
        "${CMAKE_PREFIX_PATH}/include/"
    )

    # Link abseil for header access
    target_link_libraries(${LIB_NAME} PUBLIC absl::strings)

    # Export symbols when building the library itself
    target_compile_definitions(${LIB_NAME} PRIVATE AUDIO_ENGINE_EXPORTS)

    # Output libAE_AECM.a to lib/
    set_target_properties(${LIB_NAME} PROPERTIES
        ARCHIVE_OUTPUT_DIRECTORY "${PROJECT_SOURCE_DIR}/lib"
    )
endfunction()
