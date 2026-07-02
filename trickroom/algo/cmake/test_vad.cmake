function(add_vad_test_module)
    set(LIB_NAME "vad")

    add_library(${LIB_NAME} STATIC 
        "${PROJECT_SOURCE_DIR}/common_audio/wav_file.cc"
        "${PROJECT_SOURCE_DIR}/common_audio/wav_header.cc"
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
        "${PROJECT_SOURCE_DIR}/rtc_base/system/file_wrapper.cc"
        "${PROJECT_SOURCE_DIR}/rtc_base/strings/string_builder.cc"
        "${PROJECT_SOURCE_DIR}/rtc_base/memory/aligned_malloc.cc"

        "${PROJECT_SOURCE_DIR}/system_wrappers/source/clock.cc"
    )

    file(GLOB VAD_SRC "${PROJECT_SOURCE_DIR}/modules/audio_processing/vad/*.cc")
    file(GLOB RTC_VAD_SRC "${PROJECT_SOURCE_DIR}/common_audio/vad/*.c")
    file(GLOB ISAC_VAD_SRC "${PROJECT_SOURCE_DIR}/modules/audio_coding/codecs/isac/main/source/*.c")
    file(GLOB RESAMPLE_SRC "${PROJECT_SOURCE_DIR}/common_audio/resampler/*.cc")

    target_sources(${LIB_NAME} PRIVATE 
        ${VAD_SRC}
        ${RTC_VAD_SRC}
        ${ISAC_VAD_SRC}
        ${RESAMPLE_SRC}
    )

    # 设置该库所需的头文件路径
    target_include_directories(${LIB_NAME} PUBLIC 
        "${PROJECT_SOURCE_DIR}/"
        "${PROJECT_SOURCE_DIR}/rtc_base/"
        "${PROJECT_SOURCE_DIR}/system_wrappers/include/"
        "${PROJECT_SOURCE_DIR}/common_audio/resampler/include/"
        "${CMAKE_PREFIX_PATH}/include/"
    )

    # --- 2. 定义测试可执行程序 ---
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