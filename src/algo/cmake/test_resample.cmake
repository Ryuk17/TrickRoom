function(add_resample_test_module)
    set(LIB_NAME "resample_test_lib")

    add_library(${LIB_NAME} STATIC
        "${PROJECT_SOURCE_DIR}/rtc_base/strings/string_builder.cc"
        "${PROJECT_SOURCE_DIR}/common_audio/dr_wav_impl.cc"
        "${PROJECT_SOURCE_DIR}/common_audio/audio_util.cc"

        "${PROJECT_SOURCE_DIR}/common_audio/signal_processing/energy.c"
        "${PROJECT_SOURCE_DIR}/common_audio/signal_processing/resample.c"
        "${PROJECT_SOURCE_DIR}/common_audio/signal_processing/resample_fractional.c"
        "${PROJECT_SOURCE_DIR}/common_audio/signal_processing/resample_48khz.c"
        "${PROJECT_SOURCE_DIR}/common_audio/signal_processing/resample_by_2.c"
        "${PROJECT_SOURCE_DIR}/common_audio/signal_processing/resample_by_2_internal.c"
        "${PROJECT_SOURCE_DIR}/common_audio/signal_processing/splitting_filter.c"
        "${PROJECT_SOURCE_DIR}/common_audio/signal_processing/division_operations.c"
        "${PROJECT_SOURCE_DIR}/common_audio/signal_processing/get_scaling_square.c"
        "${PROJECT_SOURCE_DIR}/common_audio/signal_processing/min_max_operations.c"
        "${PROJECT_SOURCE_DIR}/common_audio/signal_processing/spl_init.c"
        "${PROJECT_SOURCE_DIR}/common_audio/signal_processing/spl_inl.c"
        "${PROJECT_SOURCE_DIR}/common_audio/signal_processing/vector_operations.c"
        "${PROJECT_SOURCE_DIR}/common_audio/signal_processing/copy_set_operations.c"
        "${PROJECT_SOURCE_DIR}/common_audio/signal_processing/downsample_fast.c"
        "${PROJECT_SOURCE_DIR}/common_audio/signal_processing/filter_ar.c"
        "${PROJECT_SOURCE_DIR}/common_audio/signal_processing/vector_scaling_operations.c"
        "${PROJECT_SOURCE_DIR}/common_audio/signal_processing/dot_product_with_scale.cc"

        "${PROJECT_SOURCE_DIR}/common_audio/third_party/spl_sqrt_floor/spl_sqrt_floor.c"

        "${PROJECT_SOURCE_DIR}/rtc_base/time_utils.cc"
        "${PROJECT_SOURCE_DIR}/rtc_base/system_time.cc"
        "${PROJECT_SOURCE_DIR}/rtc_base/platform_thread_types.cc"
        "${PROJECT_SOURCE_DIR}/api/units/timestamp.cc"

        "${PROJECT_SOURCE_DIR}/system_wrappers/source/clock.cc"
    )
    

    # 添加 resampler 源文件
    file(GLOB RESAMPLER_SRC "${PROJECT_SOURCE_DIR}/common_audio/resampler/*.cc")
    target_sources(${LIB_NAME} PRIVATE
        ${RESAMPLER_SRC}
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
    set(TEST_NAME "test_resampler")

    add_executable(${TEST_NAME} "${PROJECT_SOURCE_DIR}/test/test_resampler.cc")


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
