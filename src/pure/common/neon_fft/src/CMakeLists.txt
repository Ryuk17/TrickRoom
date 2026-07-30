set(FFT_BASE_SRC NE10_fft.c)

if(${CMAKE_SYSTEM_PROCESSOR} MATCHES arm)
list(APPEND FFT_BASE_SRC 
  NE10_fft_float32.c 
  NE10_fft_generic_float32.c
  NE10_fft_float32.neon.c
  NE10_fft_float32.neonintrinsic.c
)
elseif(${CMAKE_SYSTEM_PROCESSOR} MATCHES aarch64)
list(APPEND FFT_BASE_SRC 
  NE10_fft_float32.c 
  NE10_fft_generic_float32.c
  NE10_rfft_float32.c
  NE10_rfft_float32.neonintrinsic.c
)
else()
list(APPEND FFT_BASE_SRC 
  NE10_fft_float32.c 
  NE10_fft_generic_float32.c
)
endif()

add_library(NeFFT STATIC ${FFT_BASE_SRC})

target_include_directories(NeFFT
    PUBLIC 
        ${PROJECT_SOURCE_DIR}/include
)
