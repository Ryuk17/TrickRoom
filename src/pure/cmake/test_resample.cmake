function(add_resample_test_module)
    set(LIB_NAME "resample_test_lib")

    add_library(${LIB_NAME} STATIC
        "${PROJECT_SOURCE_DIR}/utils/dr_wav.cc"
        "${PROJECT_SOURCE_DIR}/utils/audio_util.cc"

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
        "${PROJECT_SOURCE_DIR}/common/signal_processing/vector_operations.c"
        "${PROJECT_SOURCE_DIR}/common/signal_processing/copy_set_operations.c"
        "${PROJECT_SOURCE_DIR}/common/signal_processing/downsample_fast.c"
        "${PROJECT_SOURCE_DIR}/common/signal_processing/filter_ar.c"
        "${PROJECT_SOURCE_DIR}/common/signal_processing/vector_scaling_operations.c"
        "${PROJECT_SOURCE_DIR}/common/signal_processing/dot_product_with_scale.cc"
        "${PROJECT_SOURCE_DIR}/common/signal_processing/spl_sqrt_floor.c"
    )
    

    # 添加 resampler 源文件（排除特定架构的文件）
    file(GLOB RESAMPLER_SRC "${PROJECT_SOURCE_DIR}/audio_processing/resample/*.cc")
    # 排除 NEON 文件（仅 ARM）
    list(FILTER RESAMPLER_SRC EXCLUDE REGEX ".*_neon\\.cc$")
    target_sources(${LIB_NAME} PRIVATE
        ${RESAMPLER_SRC}
    )

    # 设置该库所需的头文件路径
    target_include_directories(${LIB_NAME} PUBLIC
        "${PROJECT_SOURCE_DIR}"
        "${PROJECT_SOURCE_DIR}/utils/"
        "${PROJECT_SOURCE_DIR}/audio_processing/resample/"
        "${PROJECT_SOURCE_DIR}/common/signal_processing/"
        "${PROJECT_SOURCE_DIR}/common/signal_processing/include/"
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
