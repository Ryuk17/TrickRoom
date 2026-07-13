function(add_agc2_test_module)
    set(LIB_NAME "agc2")
    add_definitions(-DWEBRTC_APM_DEBUG_DUMP=0 -DWEBRTC_WIN)

    add_library(${LIB_NAME} STATIC
        "${PROJECT_SOURCE_DIR}/common_audio/dr_wav_impl.cc"
        "${PROJECT_SOURCE_DIR}/common_audio/audio_util.cc"
        "${PROJECT_SOURCE_DIR}/common_audio/real_fourier_ooura.cc"

        "${PROJECT_SOURCE_DIR}/common_audio/signal_processing/energy.c"
        "${PROJECT_SOURCE_DIR}/common_audio/signal_processing/resample.c"
        "${PROJECT_SOURCE_DIR}/common_audio/signal_processing/resample_fractional.c"
        "${PROJECT_SOURCE_DIR}/common_audio/signal_processing/resample_48khz.c"
        "${PROJECT_SOURCE_DIR}/common_audio/signal_processing/resample_by_2.c"
        "${PROJECT_SOURCE_DIR}/common_audio/signal_processing/resample_by_2_internal.c"
        "${PROJECT_SOURCE_DIR}/common_audio/signal_processing/splitting_filter.c"
        "${PROJECT_SOURCE_DIR}/common_audio/signal_processing/division_operations.c"
        "${PROJECT_SOURCE_DIR}/common_audio/signal_processing/get_scaling_square.c"
        "${PROJECT_SOURCE_DIR}/common_audio/third_party/ooura/fft_size_256/fft4g.cc"

        "${PROJECT_SOURCE_DIR}/rtc_base/checks.cc"
        "${PROJECT_SOURCE_DIR}/rtc_base/logging.cc"
        "${PROJECT_SOURCE_DIR}/rtc_base/time_utils.cc"
        "${PROJECT_SOURCE_DIR}/rtc_base/cpu_info.cc"
        "${PROJECT_SOURCE_DIR}/rtc_base/platform_thread_types.cc"
        "${PROJECT_SOURCE_DIR}/rtc_base/platform_thread.cc"
        "${PROJECT_SOURCE_DIR}/rtc_base/string_encode.cc"
        "${PROJECT_SOURCE_DIR}/rtc_base/string_utils.cc"
        "${PROJECT_SOURCE_DIR}/rtc_base/system_time.cc"
        "${PROJECT_SOURCE_DIR}/rtc_base/strings/string_builder.cc"
        "${PROJECT_SOURCE_DIR}/rtc_base/memory/aligned_malloc.cc"

        "${PROJECT_SOURCE_DIR}/system_wrappers/source/clock.cc"
        "${PROJECT_SOURCE_DIR}/system_wrappers/source/metrics.cc"

        # AGC2 core source files
        "${PROJECT_SOURCE_DIR}/modules/audio_processing/agc2/adaptive_digital_gain_controller.cc"
        "${PROJECT_SOURCE_DIR}/modules/audio_processing/agc2/gain_applier.cc"
        "${PROJECT_SOURCE_DIR}/modules/audio_processing/agc2/speech_level_estimator.cc"
        "${PROJECT_SOURCE_DIR}/modules/audio_processing/agc2/noise_level_estimator.cc"
        "${PROJECT_SOURCE_DIR}/modules/audio_processing/agc2/fixed_digital_level_estimator.cc"
        "${PROJECT_SOURCE_DIR}/modules/audio_processing/agc2/biquad_filter.cc"
        "${PROJECT_SOURCE_DIR}/modules/audio_processing/agc2/cpu_features.cc"
        "${PROJECT_SOURCE_DIR}/modules/audio_processing/agc2/vector_float_frame.cc"

        # AGC2 RNN VAD wrapper
        "${PROJECT_SOURCE_DIR}/modules/audio_processing/agc2/vad_wrapper.cc"

        # RNN VAD source files
        "${PROJECT_SOURCE_DIR}/modules/audio_processing/agc2/rnn_vad/auto_correlation.cc"
        "${PROJECT_SOURCE_DIR}/modules/audio_processing/agc2/rnn_vad/features_extraction.cc"
        "${PROJECT_SOURCE_DIR}/modules/audio_processing/agc2/rnn_vad/lp_residual.cc"
        "${PROJECT_SOURCE_DIR}/modules/audio_processing/agc2/rnn_vad/pitch_search.cc"
        "${PROJECT_SOURCE_DIR}/modules/audio_processing/agc2/rnn_vad/pitch_search_internal.cc"
        "${PROJECT_SOURCE_DIR}/modules/audio_processing/agc2/rnn_vad/rnn.cc"
        "${PROJECT_SOURCE_DIR}/modules/audio_processing/agc2/rnn_vad/rnn_fc.cc"
        "${PROJECT_SOURCE_DIR}/modules/audio_processing/agc2/rnn_vad/rnn_gru.cc"
        "${PROJECT_SOURCE_DIR}/modules/audio_processing/agc2/rnn_vad/spectral_features.cc"
        "${PROJECT_SOURCE_DIR}/modules/audio_processing/agc2/rnn_vad/spectral_features_internal.cc"
        "${PROJECT_SOURCE_DIR}/modules/audio_processing/agc2/rnn_vad/vector_math_avx2.cc"

        # Third-party RNNoise (RNN VAD weights)
        "${PROJECT_SOURCE_DIR}/third_party/rnnoise/src/rnn_vad_weights.cc"

        # Pffft FFT library used by RNN VAD (auto_correlation, spectral_features)
        "${PROJECT_SOURCE_DIR}/modules/audio_processing/utility/pffft_wrapper.cc"
        "${PROJECT_SOURCE_DIR}/third_party/pffft/src/pffft.c"
        "${PROJECT_SOURCE_DIR}/third_party/pffft/src/fftpack.c"

        # SincResampler AVX2 convolution
        "${PROJECT_SOURCE_DIR}/common_audio/resampler/sinc_resampler_avx2.cc"

        # ApmDataDumper required by AGC2 components
        "${PROJECT_SOURCE_DIR}/modules/audio_processing/logging/apm_data_dumper.cc"
    )

    # Resampler needed by VoiceActivityDetectorWrapper
    file(GLOB RESAMPLE_SRC "${PROJECT_SOURCE_DIR}/common_audio/resampler/*.cc")
    target_sources(${LIB_NAME} PRIVATE ${RESAMPLE_SRC})

    # Enable AVX2+FMA for vector_math_avx2.cc (runtime guarded)
    if(MSVC)
        set_source_files_properties(
            "${PROJECT_SOURCE_DIR}/modules/audio_processing/agc2/rnn_vad/vector_math_avx2.cc"
            PROPERTIES COMPILE_FLAGS "/arch:AVX2"
        )
    else()
        set_source_files_properties(
            "${PROJECT_SOURCE_DIR}/modules/audio_processing/agc2/rnn_vad/vector_math_avx2.cc"
            "${PROJECT_SOURCE_DIR}/common_audio/resampler/sinc_resampler_avx2.cc"
            PROPERTIES COMPILE_FLAGS "-mavx2 -mfma"
        )
    endif()

    # 设置该库所需的头文件路径
    target_include_directories(${LIB_NAME} PUBLIC
        "${PROJECT_SOURCE_DIR}/"
        "${PROJECT_SOURCE_DIR}/rtc_base/"
        "${PROJECT_SOURCE_DIR}/system_wrappers/include/"
        "${PROJECT_SOURCE_DIR}/common_audio/resampler/include/"
        "${CMAKE_PREFIX_PATH}/include/"
    )

    # --- 2. 定义测试可执行程序 ---
    set(TEST_NAME "test_agc2")

    add_executable(${TEST_NAME} "${PROJECT_SOURCE_DIR}/test/test_agc2.cc")


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
