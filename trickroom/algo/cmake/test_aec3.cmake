function(add_aec3_test_module)
    set(LIB_NAME "aec3")
    add_definitions(-DWEBRTC_APM_DEBUG_DUMP=0)

    find_package(Python3 REQUIRED COMPONENTS Interpreter)

    if(Python3_FOUND)
        set(PYTHON_EXECUTABLE ${Python3_EXECUTABLE})
    else()
        message(FATAL_ERROR "Python3 not found")
    endif()

    # Set variables for the script and output directory
    set(FT_SCRIPT_PATH "${CMAKE_SOURCE_DIR}/experiments/field_trials.py")  # Adjust the path to where the script is located
    set(FT_OUTPUT_DIR "${CMAKE_SOURCE_DIR}/experiments")
    set(FT_OUTPUT_FILE "${FT_OUTPUT_DIR}/registered_field_trials.h")

    # Add a custom command that generates the output file
    add_custom_command(
        OUTPUT ${FT_OUTPUT_FILE}
        COMMAND ${CMAKE_COMMAND} -E make_directory ${FT_OUTPUT_DIR}
        COMMAND ${CMAKE_COMMAND} -E env PYTHONIOENCODING=utf-8
                ${PYTHON_EXECUTABLE} ${FT_SCRIPT_PATH} header --output ${FT_OUTPUT_FILE}
        DEPENDS ${FT_SCRIPT_PATH}
        COMMENT "Generating registered_field_trials.h"
        VERBATIM
    )

    # Add a custom target that depends on the output file
    add_custom_target(
        GenerateFieldTrialsHeader ALL
        DEPENDS ${FT_OUTPUT_FILE}
    )


    add_library(${LIB_NAME} STATIC 
        "${PROJECT_SOURCE_DIR}/common_audio/wav_file.cc"
        "${PROJECT_SOURCE_DIR}/common_audio/wav_header.cc"
        "${PROJECT_SOURCE_DIR}/common_audio/audio_util.cc"
        "${PROJECT_SOURCE_DIR}/common_audio/real_fourier_ooura.cc"
        "${PROJECT_SOURCE_DIR}/common_audio/signal_processing/splitting_filter.c"
        "${PROJECT_SOURCE_DIR}/common_audio/ring_buffer.c"
        "${PROJECT_SOURCE_DIR}/common_audio/third_party/ooura/fft_size_128/ooura_fft_sse2.cc"
        "${PROJECT_SOURCE_DIR}/common_audio/third_party/ooura/fft_size_128/ooura_fft_neon.cc"
        "${PROJECT_SOURCE_DIR}/common_audio/third_party/ooura/fft_size_128/ooura_fft_mips.cc"
        "${PROJECT_SOURCE_DIR}/common_audio/third_party/ooura/fft_size_128/ooura_fft.cc"

        "${PROJECT_SOURCE_DIR}/api/field_trials_registry.cc"
        "${PROJECT_SOURCE_DIR}/api/audio/echo_canceller3_config.cc"
        "${PROJECT_SOURCE_DIR}/api/audio/echo_canceller3_factory.cc"
        "${PROJECT_SOURCE_DIR}/api/audio/echo_detector_creator.cc"
        "${PROJECT_SOURCE_DIR}/api/environment/environment_factory.cc"
        "${PROJECT_SOURCE_DIR}/api/environment/deprecated_global_field_trials.cc"
        "${PROJECT_SOURCE_DIR}/api/rtc_event_log/rtc_event_log.cc"
        "${PROJECT_SOURCE_DIR}/api/task_queue/default_task_queue_factory_stdlib.cc"
        "${PROJECT_SOURCE_DIR}/api/task_queue/task_queue_base.cc"

        "${PROJECT_SOURCE_DIR}/rtc_base/checks.cc"
        "${PROJECT_SOURCE_DIR}/rtc_base/logging.cc"
        "${PROJECT_SOURCE_DIR}/rtc_base/time_utils.cc"
        "${PROJECT_SOURCE_DIR}/rtc_base/cpu_info.cc"
        "${PROJECT_SOURCE_DIR}/rtc_base/event.cc"
        "${PROJECT_SOURCE_DIR}/rtc_base/platform_thread_types.cc"
        "${PROJECT_SOURCE_DIR}/rtc_base/platform_thread.cc"
        "${PROJECT_SOURCE_DIR}/rtc_base/string_encode.cc"
        "${PROJECT_SOURCE_DIR}/rtc_base/string_utils.cc"
        "${PROJECT_SOURCE_DIR}/rtc_base/race_checker.cc"
        "${PROJECT_SOURCE_DIR}/rtc_base/task_queue_stdlib.cc"
        "${PROJECT_SOURCE_DIR}/rtc_base/system_time.cc"
        "${PROJECT_SOURCE_DIR}/rtc_base/system/file_wrapper.cc"
        "${PROJECT_SOURCE_DIR}/rtc_base/strings/string_builder.cc"
        "${PROJECT_SOURCE_DIR}/rtc_base/memory/aligned_malloc.cc"
        "${PROJECT_SOURCE_DIR}/rtc_base/experiments/field_trial_parser.cc"
        "${PROJECT_SOURCE_DIR}/rtc_base/synchronization/yield_policy.cc"
        

        "${PROJECT_SOURCE_DIR}/system_wrappers/source/clock.cc"
        "${PROJECT_SOURCE_DIR}/system_wrappers/source/metrics.cc"

        "${PROJECT_SOURCE_DIR}/modules/audio_processing/audio_buffer.cc"
        "${PROJECT_SOURCE_DIR}/modules/audio_processing/splitting_filter.cc"
        "${PROJECT_SOURCE_DIR}/modules/audio_processing/three_band_filter_bank.cc"
        "${PROJECT_SOURCE_DIR}/modules/audio_processing/high_pass_filter.cc"
        "${PROJECT_SOURCE_DIR}/modules/audio_processing/utility/delay_estimator.cc"
        "${PROJECT_SOURCE_DIR}/modules/audio_processing/utility/delay_estimator_wrapper.cc"
        "${PROJECT_SOURCE_DIR}/modules/audio_processing/utility/cascaded_biquad_filter.cc"
        "${PROJECT_SOURCE_DIR}/modules/audio_processing/logging/apm_data_dumper.cc"
    )

    file(GLOB AEC3_SRC "${PROJECT_SOURCE_DIR}/modules/audio_processing/aec3/*.cc")
    file(GLOB RESAMPLE_SRC "${PROJECT_SOURCE_DIR}/common_audio/resampler/*.cc")
    target_sources(${LIB_NAME} PRIVATE 
        ${AEC3_SRC}
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
    set(TEST_NAME "test_aec3")
    
    add_executable(${TEST_NAME} "${PROJECT_SOURCE_DIR}/test/test_aec3.cc")


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