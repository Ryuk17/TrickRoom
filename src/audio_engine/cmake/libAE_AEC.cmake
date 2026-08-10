function(add_libAE_AEC_target)
    set(LIB_NAME "AE_AEC")

    add_library(${LIB_NAME} STATIC
        # Interface
        "${PROJECT_SOURCE_DIR}/interface/audio_engine_aec.cpp"

        # WAV I/O + utils
        "${PROJECT_SOURCE_DIR}/utils/dr_wav.cc"
        "${PROJECT_SOURCE_DIR}/utils/audio_util.cc"
        "${PROJECT_SOURCE_DIR}/utils/string_builder.cc"
        "${PROJECT_SOURCE_DIR}/audio_processing/aec3/echo_canceller3_config.cc"

        # NE10 FFT (generic C implementation)
        "${PROJECT_SOURCE_DIR}/third_party/neon-fft/src/NE10_fft.c"
        "${PROJECT_SOURCE_DIR}/third_party/neon-fft/src/NE10_fft_float32.c"
        "${PROJECT_SOURCE_DIR}/third_party/neon-fft/src/NE10_fft_generic_float32.c"
        "${PROJECT_SOURCE_DIR}/third_party/neon-fft/src/NE10_rfft_float32.c"

        # Signal processing library
        "${PROJECT_SOURCE_DIR}/signal_processing/spl_sqrt.c"
        "${PROJECT_SOURCE_DIR}/signal_processing/spl_sqrt_floor.c"
        "${PROJECT_SOURCE_DIR}/signal_processing/spl_init.c"
        "${PROJECT_SOURCE_DIR}/signal_processing/spl_inl.c"
        "${PROJECT_SOURCE_DIR}/signal_processing/copy_set_operations.c"
        "${PROJECT_SOURCE_DIR}/signal_processing/dot_product_with_scale.cc"
        "${PROJECT_SOURCE_DIR}/signal_processing/division_operations.c"
        "${PROJECT_SOURCE_DIR}/signal_processing/min_max_operations.c"
        "${PROJECT_SOURCE_DIR}/signal_processing/vector_operations.c"
        "${PROJECT_SOURCE_DIR}/signal_processing/vector_scaling_operations.c"
        "${PROJECT_SOURCE_DIR}/signal_processing/energy.c"
        "${PROJECT_SOURCE_DIR}/signal_processing/resample.c"
        "${PROJECT_SOURCE_DIR}/signal_processing/resample_fractional.c"
        "${PROJECT_SOURCE_DIR}/signal_processing/resample_48khz.c"
        "${PROJECT_SOURCE_DIR}/signal_processing/resample_by_2.c"
        "${PROJECT_SOURCE_DIR}/signal_processing/resample_by_2_internal.c"
        "${PROJECT_SOURCE_DIR}/signal_processing/get_scaling_square.c"
        "${PROJECT_SOURCE_DIR}/signal_processing/downsample_fast.c"
        "${PROJECT_SOURCE_DIR}/signal_processing/filter_ar.c"
        "${PROJECT_SOURCE_DIR}/signal_processing/splitting_filter.c"
    )

    # AudioBuffer + splitting + three band filter bank
    target_sources(${LIB_NAME} PRIVATE
        "${PROJECT_SOURCE_DIR}/utils/audio_buffer.cc"
        "${PROJECT_SOURCE_DIR}/signal_processing/splitting_filter.cc"
        "${PROJECT_SOURCE_DIR}/signal_processing/three_band_filter_bank.cc"
    )

    # High pass filter
    target_sources(${LIB_NAME} PRIVATE
        "${PROJECT_SOURCE_DIR}/signal_processing/high_pass_filter.cc"
        "${PROJECT_SOURCE_DIR}/signal_processing/cascaded_biquad_filter.cc"
    )

    # AEC3 sources (all .cc files except neural residual echo estimator)
    file(GLOB AEC3_SRC "${PROJECT_SOURCE_DIR}/audio_processing/aec3/*.cc")
    list(FILTER AEC3_SRC EXCLUDE REGEX ".*neural_feature_extractor\\.cc$")
    list(FILTER AEC3_SRC EXCLUDE REGEX ".*neural_residual_echo_estimator_impl\\.cc$")
    target_sources(${LIB_NAME} PRIVATE ${AEC3_SRC})

    # Resampler sources (used by AudioBuffer)
    file(GLOB RESAMPLER_SRC "${PROJECT_SOURCE_DIR}/audio_processing/resample/*.cc")
    list(FILTER RESAMPLER_SRC EXCLUDE REGEX ".*_neon\\.cc$")
    target_sources(${LIB_NAME} PRIVATE ${RESAMPLER_SRC})

    # AVX2+FMA for AEC3 and resampler SIMD files
    set_source_files_properties(
        "${PROJECT_SOURCE_DIR}/audio_processing/aec3/adaptive_fir_filter_avx2.cc"
        "${PROJECT_SOURCE_DIR}/audio_processing/aec3/adaptive_fir_filter_erl_avx2.cc"
        "${PROJECT_SOURCE_DIR}/audio_processing/aec3/fft_data_avx2.cc"
        "${PROJECT_SOURCE_DIR}/audio_processing/aec3/matched_filter_avx2.cc"
        "${PROJECT_SOURCE_DIR}/audio_processing/aec3/vector_math_avx2.cc"
        "${PROJECT_SOURCE_DIR}/audio_processing/resample/sinc_resampler_avx2.cc"
        PROPERTIES COMPILE_FLAGS "-mavx2 -mfma"
    )

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

    # Output libAE_AEC.a to lib/
    set_target_properties(${LIB_NAME} PROPERTIES
        ARCHIVE_OUTPUT_DIRECTORY "${PROJECT_SOURCE_DIR}/lib"
    )
endfunction()
