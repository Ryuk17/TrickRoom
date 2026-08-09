function(add_libAE_AGC2_target)
    set(LIB_NAME "AE_AGC2")

    add_library(${LIB_NAME} STATIC
        # Interface
        "${PROJECT_SOURCE_DIR}/interface/audio_engine_agc2.cpp"

        # WAV I/O utilities
        "${PROJECT_SOURCE_DIR}/utils/dr_wav.cc"
        "${PROJECT_SOURCE_DIR}/utils/audio_util.cc"
        "${PROJECT_SOURCE_DIR}/utils/string_builder.cc"

        # Signal processing library (same set as test_agc2.cmake)
        "${PROJECT_SOURCE_DIR}/signal_processing/spl_sqrt.c"
        "${PROJECT_SOURCE_DIR}/signal_processing/spl_sqrt_floor.c"
        "${PROJECT_SOURCE_DIR}/signal_processing/spl_init.c"
        "${PROJECT_SOURCE_DIR}/signal_processing/spl_inl.c"
        "${PROJECT_SOURCE_DIR}/signal_processing/copy_set_operations.c"
        "${PROJECT_SOURCE_DIR}/signal_processing/dot_product_with_scale.cc"
        "${PROJECT_SOURCE_DIR}/signal_processing/energy.c"
        "${PROJECT_SOURCE_DIR}/signal_processing/resample.c"
        "${PROJECT_SOURCE_DIR}/signal_processing/resample_fractional.c"
        "${PROJECT_SOURCE_DIR}/signal_processing/resample_48khz.c"
        "${PROJECT_SOURCE_DIR}/signal_processing/resample_by_2.c"
        "${PROJECT_SOURCE_DIR}/signal_processing/resample_by_2_internal.c"
        "${PROJECT_SOURCE_DIR}/signal_processing/division_operations.c"
        "${PROJECT_SOURCE_DIR}/signal_processing/get_scaling_square.c"
        "${PROJECT_SOURCE_DIR}/signal_processing/min_max_operations.c"
        "${PROJECT_SOURCE_DIR}/signal_processing/vector_operations.c"
        "${PROJECT_SOURCE_DIR}/signal_processing/vector_scaling_operations.c"
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

    # AGC2 core sources
    file(GLOB AGC2_CORE "${PROJECT_SOURCE_DIR}/audio_processing/agc2/*.cc")
    target_sources(${LIB_NAME} PRIVATE ${AGC2_CORE})

    # RNN VAD sources
    file(GLOB RNN_VAD_SRC "${PROJECT_SOURCE_DIR}/audio_processing/agc2/rnn_vad/*.cc")
    target_sources(${LIB_NAME} PRIVATE ${RNN_VAD_SRC})

    # RNNoise weights
    target_sources(${LIB_NAME} PRIVATE
        "${PROJECT_SOURCE_DIR}/model_weights/rnnoise/src/rnn_vad_weights.cc"
    )

    # PFFFT (non-power-of-2 FFT support for RNN VAD spectral features)
    target_sources(${LIB_NAME} PRIVATE
        "${PROJECT_SOURCE_DIR}/signal_processing/pffft_wrapper.cc"
        "${PROJECT_SOURCE_DIR}/third_party/pffft/pffft.c"
        "${PROJECT_SOURCE_DIR}/third_party/pffft/fftpack.c"
    )

    # Resampler sources (for VoiceActivityDetectorWrapper)
    file(GLOB RESAMPLER_SRC "${PROJECT_SOURCE_DIR}/audio_processing/resample/*.cc")
    list(FILTER RESAMPLER_SRC EXCLUDE REGEX ".*_neon\\.cc$")
    target_sources(${LIB_NAME} PRIVATE ${RESAMPLER_SRC})

    # AVX2+FMA for vector_math_avx2.cc and sinc_resampler_avx2.cc
    set_source_files_properties(
        "${PROJECT_SOURCE_DIR}/audio_processing/agc2/rnn_vad/vector_math_avx2.cc"
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
        "${PROJECT_SOURCE_DIR}/third_party/pffft/"
        "${CMAKE_PREFIX_PATH}/include/"
    )

    # Link abseil for header access
    target_link_libraries(${LIB_NAME} PUBLIC absl::strings)

    # Export symbols when building the library itself
    target_compile_definitions(${LIB_NAME} PRIVATE AUDIO_ENGINE_EXPORTS)

    # Output libAE_AGC2.a to lib/
    set_target_properties(${LIB_NAME} PROPERTIES
        ARCHIVE_OUTPUT_DIRECTORY "${PROJECT_SOURCE_DIR}/lib"
    )
endfunction()
