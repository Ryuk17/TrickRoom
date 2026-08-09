function(add_ae_ns_test_module)
    # --- Test executable using the libAE_NS interface ---
    set(TEST_NAME "test_ae_ns")

    add_executable(${TEST_NAME} "${PROJECT_SOURCE_DIR}/unitest/test_ae_ns.cc")

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
                AE_NS
                absl::strings
                winmm
            )
        endif()
    else()
        target_link_libraries(${TEST_NAME} PRIVATE
            AE_NS
            absl::strings
        )
    endif()

    add_test(NAME ${TEST_NAME} COMMAND ${TEST_NAME})
endfunction()
