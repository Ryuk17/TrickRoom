function(add_wav_io_test_module)
    set(LIB_NAME "wav_io")

    add_library(${LIB_NAME} STATIC 
        "${PROJECT_SOURCE_DIR}/common_audio/wav_file.cc"
        "${PROJECT_SOURCE_DIR}/common_audio/wav_header.cc"
        "${PROJECT_SOURCE_DIR}/rtc_base/checks.cc"
        "${PROJECT_SOURCE_DIR}/rtc_base/logging.cc"
        "${PROJECT_SOURCE_DIR}/rtc_base/time_utils.cc"
        "${PROJECT_SOURCE_DIR}/rtc_base/platform_thread_types.cc"
        "${PROJECT_SOURCE_DIR}/rtc_base/platform_thread.cc"
        "${PROJECT_SOURCE_DIR}/rtc_base/string_encode.cc"
        "${PROJECT_SOURCE_DIR}/rtc_base/string_utils.cc"
        "${PROJECT_SOURCE_DIR}/rtc_base/system_time.cc"
        "${PROJECT_SOURCE_DIR}/rtc_base/system/file_wrapper.cc"
        "${PROJECT_SOURCE_DIR}/rtc_base/strings/string_builder.cc"
    )

    # 设置该库所需的头文件路径
    target_include_directories(${LIB_NAME} PUBLIC 
        "${PROJECT_SOURCE_DIR}/"
        "${PROJECT_SOURCE_DIR}/rtc_base/"
        "${CMAKE_PREFIX_PATH}/include/"
    )

    # --- 2. 定义测试可执行程序 ---
    set(TEST_NAME "test_wav_io")
    
    add_executable(${TEST_NAME} "${PROJECT_SOURCE_DIR}/test/test_wav_io.cc")


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