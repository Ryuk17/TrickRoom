function(add_aecm_test_module)
    set(LIB_NAME "aecm")

    add_library(${LIB_NAME} STATIC 
        "${PROJECT_SOURCE_DIR}/common_audio/wav_file.cc"
        "${PROJECT_SOURCE_DIR}/common_audio/wav_header.cc"
        "${PROJECT_SOURCE_DIR}/common_audio/audio_util.cc"
        "${PROJECT_SOURCE_DIR}/common_audio/ring_buffer.c"
        "${PROJECT_SOURCE_DIR}/common_audio/signal_processing/splitting_filter.c"
        "${PROJECT_SOURCE_DIR}/common_audio/signal_processing/complex_fft_mips.c"
        "${PROJECT_SOURCE_DIR}/common_audio/signal_processing/complex_fft.c"
        "${PROJECT_SOURCE_DIR}/common_audio/signal_processing/real_fft.c"
        "${PROJECT_SOURCE_DIR}/common_audio/signal_processing/min_max_operations_mips.c"
        "${PROJECT_SOURCE_DIR}/common_audio/signal_processing/min_max_operations_neon.c"
        "${PROJECT_SOURCE_DIR}/common_audio/signal_processing/min_max_operations.c"
        "${PROJECT_SOURCE_DIR}/common_audio/signal_processing/division_operations.c"
        "${PROJECT_SOURCE_DIR}/common_audio/signal_processing/randomization_functions.c"
        "${PROJECT_SOURCE_DIR}/common_audio/signal_processing/complex_bit_reverse_mips.c"
        "${PROJECT_SOURCE_DIR}/common_audio/signal_processing/complex_bit_reverse.c"
        "${PROJECT_SOURCE_DIR}/common_audio/signal_processing/spl_init.c"
        "${PROJECT_SOURCE_DIR}/common_audio/signal_processing/vector_scaling_operations_mips.c"
        "${PROJECT_SOURCE_DIR}/common_audio/signal_processing/vector_scaling_operations.c"
        "${PROJECT_SOURCE_DIR}/common_audio/signal_processing/downsample_fast_neon.c"
        "${PROJECT_SOURCE_DIR}/common_audio/signal_processing/downsample_fast_mips.c"
        "${PROJECT_SOURCE_DIR}/common_audio/signal_processing/downsample_fast.c" 
        "${PROJECT_SOURCE_DIR}/common_audio/signal_processing/cross_correlation_neon.c"   
        "${PROJECT_SOURCE_DIR}/common_audio/signal_processing/cross_correlation_mips.c"
        "${PROJECT_SOURCE_DIR}/common_audio/signal_processing/cross_correlation.c"

        "${PROJECT_SOURCE_DIR}/common_audio/third_party/spl_sqrt_floor/spl_sqrt_floor_mips.c"
        "${PROJECT_SOURCE_DIR}/common_audio/third_party/spl_sqrt_floor/spl_sqrt_floor.c"

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

        "${PROJECT_SOURCE_DIR}/modules/audio_processing/audio_buffer.cc"
        "${PROJECT_SOURCE_DIR}/modules/audio_processing/splitting_filter.cc"
        "${PROJECT_SOURCE_DIR}/modules/audio_processing/three_band_filter_bank.cc"
        "${PROJECT_SOURCE_DIR}/modules/audio_processing/utility/delay_estimator.cc"
        "${PROJECT_SOURCE_DIR}/modules/audio_processing/utility/delay_estimator_wrapper.cc"
    )

    file(GLOB AECM_SRC "${PROJECT_SOURCE_DIR}/modules/audio_processing/aecm/*.cc")

    target_sources(${LIB_NAME} PRIVATE 
        ${AECM_SRC}
        ${RESAMPLE_SRC}
    )

    # 设置该库所需的头文件路径
    target_include_directories(${LIB_NAME} PUBLIC 
        "${PROJECT_SOURCE_DIR}/"
        "${PROJECT_SOURCE_DIR}/rtc_base/"
        "${PROJECT_SOURCE_DIR}/system_wrappers/include/"
        "${PROJECT_SOURCE_DIR}/common_audio/signal_processing/include/"
        "${CMAKE_PREFIX_PATH}/include/"
    )

    # --- 2. 定义测试可执行程序 ---
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