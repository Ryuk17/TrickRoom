function(add_agc_legacy_test_module)
    set(LIB_NAME "agc_legacy_test_lib")

    add_library(${LIB_NAME} STATIC
        # WAV I/O utilities
        "${PROJECT_SOURCE_DIR}/utils/dr_wav.cc"
        "${PROJECT_SOURCE_DIR}/utils/audio_util.cc"

        # Signal processing library
        "${PROJECT_SOURCE_DIR}/common/signal_processing/dot_product_with_scale.cc"
        "${PROJECT_SOURCE_DIR}/common/signal_processing/copy_set_operations.c"
        "${PROJECT_SOURCE_DIR}/common/signal_processing/division_operations.c"
        "${PROJECT_SOURCE_DIR}/common/signal_processing/energy.c"
        "${PROJECT_SOURCE_DIR}/common/signal_processing/get_scaling_square.c"
        "${PROJECT_SOURCE_DIR}/common/signal_processing/min_max_operations.c"
        "${PROJECT_SOURCE_DIR}/common/signal_processing/resample.c"
        "${PROJECT_SOURCE_DIR}/common/signal_processing/resample_48khz.c"
        "${PROJECT_SOURCE_DIR}/common/signal_processing/resample_by_2.c"
        "${PROJECT_SOURCE_DIR}/common/signal_processing/resample_by_2_internal.c"
        "${PROJECT_SOURCE_DIR}/common/signal_processing/resample_fractional.c"
        "${PROJECT_SOURCE_DIR}/common/signal_processing/spl_init.c"
        "${PROJECT_SOURCE_DIR}/common/signal_processing/spl_inl.c"
        "${PROJECT_SOURCE_DIR}/common/signal_processing/spl_sqrt.c"
        "${PROJECT_SOURCE_DIR}/common/signal_processing/spl_sqrt_floor.c"
        "${PROJECT_SOURCE_DIR}/common/signal_processing/vector_operations.c"
        "${PROJECT_SOURCE_DIR}/common/signal_processing/vector_scaling_operations.c"
    )

    # AGC legacy sources
    file(GLOB AGC_SRC "${PROJECT_SOURCE_DIR}/audio_processing/agc_legacy/*.cc")
    target_sources(${LIB_NAME} PRIVATE ${AGC_SRC})

    # Include directories
    target_include_directories(${LIB_NAME} PUBLIC
        "${PROJECT_SOURCE_DIR}"
        "${PROJECT_SOURCE_DIR}/utils/"
        "${PROJECT_SOURCE_DIR}/common/signal_processing/"
        "${PROJECT_SOURCE_DIR}/common/signal_processing/include/"
        "${CMAKE_PREFIX_PATH}/include/"
    )

    # Link abseil for header access
    target_link_libraries(${LIB_NAME} PUBLIC absl::strings)

    # --- Test executable ---
    set(TEST_NAME "test_agc_legacy")

    add_executable(${TEST_NAME} "${PROJECT_SOURCE_DIR}/test/test_agc_legacy.cc")

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
