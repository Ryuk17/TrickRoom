function(add_ae_agc2_test_module)
    # --- Test executable using the libAE_AGC2 interface ---
    set(TEST_NAME "test_ae_agc2")

    add_executable(${TEST_NAME} "${PROJECT_SOURCE_DIR}/unitest/test_ae_agc2.cc")

    # Include directories
    target_include_directories(${TEST_NAME} PRIVATE
        "${PROJECT_SOURCE_DIR}"
        "${PROJECT_SOURCE_DIR}/interface/"
        "${PROJECT_SOURCE_DIR}/utils/"
    )

    # Link statically: no dllimport symbols
    target_compile_definitions(${TEST_NAME} PRIVATE AUDIO_ENGINE_STATIC)

    if(WIN32)
        if(MINGW)
            target_link_libraries(${TEST_NAME} PRIVATE
                AE_AGC2
                absl::strings
                winmm
            )
        endif()
    else()
        target_link_libraries(${TEST_NAME} PRIVATE
            AE_AGC2
            absl::strings
        )
    endif()

    add_test(NAME ${TEST_NAME} COMMAND ${TEST_NAME})
endfunction()
