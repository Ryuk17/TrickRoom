function(add_libAE_NS_target)
    set(LIB_NAME "AE_NS")

    add_library(${LIB_NAME} STATIC
        # Interface
        "${PROJECT_SOURCE_DIR}/interface/audio_engine_ns.cpp"

        # WAV I/O utilities
        "${PROJECT_SOURCE_DIR}/utils/dr_wav.cc"
        "${PROJECT_SOURCE_DIR}/utils/audio_util.cc"

        # NE10 FFT (generic C implementation, used by NrFft)
        "${PROJECT_SOURCE_DIR}/third_party/neon-fft/src/NE10_fft.c"
        "${PROJECT_SOURCE_DIR}/third_party/neon-fft/src/NE10_fft_float32.c"
        "${PROJECT_SOURCE_DIR}/third_party/neon-fft/src/NE10_fft_generic_float32.c"
        "${PROJECT_SOURCE_DIR}/third_party/neon-fft/src/NE10_rfft_float32.c"

        # Signal processing library (splitting filters etc.)
        "${PROJECT_SOURCE_DIR}/signal_processing/splitting_filter.c"
        "${PROJECT_SOURCE_DIR}/signal_processing/energy.c"
        "${PROJECT_SOURCE_DIR}/signal_processing/resample.c"
        "${PROJECT_SOURCE_DIR}/signal_processing/resample_fractional.c"
        "${PROJECT_SOURCE_DIR}/signal_processing/resample_48khz.c"
        "${PROJECT_SOURCE_DIR}/signal_processing/resample_by_2.c"
        "${PROJECT_SOURCE_DIR}/signal_processing/resample_by_2_internal.c"
        "${PROJECT_SOURCE_DIR}/signal_processing/division_operations.c"
        "${PROJECT_SOURCE_DIR}/signal_processing/get_scaling_square.c"
        "${PROJECT_SOURCE_DIR}/signal_processing/min_max_operations.c"
        "${PROJECT_SOURCE_DIR}/signal_processing/spl_init.c"
        "${PROJECT_SOURCE_DIR}/signal_processing/spl_inl.c"
        "${PROJECT_SOURCE_DIR}/signal_processing/vector_operations.c"
        "${PROJECT_SOURCE_DIR}/signal_processing/copy_set_operations.c"
        "${PROJECT_SOURCE_DIR}/signal_processing/downsample_fast.c"
        "${PROJECT_SOURCE_DIR}/signal_processing/filter_ar.c"
        "${PROJECT_SOURCE_DIR}/signal_processing/vector_scaling_operations.c"
        "${PROJECT_SOURCE_DIR}/signal_processing/dot_product_with_scale.cc"
        "${PROJECT_SOURCE_DIR}/signal_processing/spl_sqrt.c"
        "${PROJECT_SOURCE_DIR}/signal_processing/spl_sqrt_floor.c"
    )

    # AudioBuffer + splitting + three band filter bank
    target_sources(${LIB_NAME} PRIVATE
        "${PROJECT_SOURCE_DIR}/utils/audio_buffer.cc"
        "${PROJECT_SOURCE_DIR}/signal_processing/splitting_filter.cc"
        "${PROJECT_SOURCE_DIR}/signal_processing/three_band_filter_bank.cc"
    )

    # Noise suppressor sources
    file(GLOB NS_SRC "${PROJECT_SOURCE_DIR}/audio_processing/ns/*.cc")
    target_sources(${LIB_NAME} PRIVATE ${NS_SRC})

    # Resampler sources (used by AudioBuffer)
    file(GLOB RESAMPLER_SRC "${PROJECT_SOURCE_DIR}/audio_processing/resample/*.cc")
    list(FILTER RESAMPLER_SRC EXCLUDE REGEX ".*_neon\\.cc$")
    target_sources(${LIB_NAME} PRIVATE ${RESAMPLER_SRC})

    # Include directories
    target_include_directories(${LIB_NAME} PUBLIC
        "${PROJECT_SOURCE_DIR}"
        "${PROJECT_SOURCE_DIR}/interface/"
        "${PROJECT_SOURCE_DIR}/utils/"
        "${PROJECT_SOURCE_DIR}/audio_processing/resample/"
        "${PROJECT_SOURCE_DIR}/signal_processing/"
        "${PROJECT_SOURCE_DIR}/signal_processing/include/"
        "${PROJECT_SOURCE_DIR}/third_party/neon-fft/include/"
        "${CMAKE_PREFIX_PATH}/include/"
    )

    # Link abseil for header access
    target_link_libraries(${LIB_NAME} PUBLIC absl::strings)

    # Export symbols when building the library itself
    target_compile_definitions(${LIB_NAME} PRIVATE AUDIO_ENGINE_EXPORTS)

    # Output libAE_NS.a to lib/
    set_target_properties(${LIB_NAME} PROPERTIES
        ARCHIVE_OUTPUT_DIRECTORY "${PROJECT_SOURCE_DIR}/lib"
    )
endfunction()
