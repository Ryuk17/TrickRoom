cmake_minimum_required(VERSION 3.15)

project(NeFFT VERSION 1.0)

if(CMAKE_TOOLCHAIN_FILE)
    get_filename_component(CMAKE_TOOLCHAIN_FILE_NAME ${CMAKE_TOOLCHAIN_FILE} NAME)
    find_file(CMAKE_TOOLCHAIN_FILE ${CMAKE_TOOLCHAIN_FILE_NAME} PATHS ${CMAKE_SOURCE_DIR} NO_DEFAULT_PATH)
    message(STATUS "CMAKE_TOOLCHAIN_FILE = ${CMAKE_TOOLCHAIN_FILE}")
endif()


set(EXECUTABLE_OUTPUT_PATH ${PROJECT_SOURCE_DIR}/bin)
set(LIBRARY_OUTPUT_PATH ${PROJECT_SOURCE_DIR}/lib)
set(CMAKE_BUILD_TYPE RELEASE)
set(CMAKE_C_FLAGS "-Wall -O3 -lm")
set(CMAKE_CXX_FLAGS "-Wall -O3 -lm")

add_subdirectory(src)

add_executable(test_fft test/test_fft.c)
target_link_libraries(test_fft PUBLIC NeFFT m)