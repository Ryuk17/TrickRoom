/*
 * Minimal FFT type declaration needed by WebRTC iSAC VAD code.
 * The iSAC structs reference FFTstr but never call any FFT functions;
 * this minimal stub satisfies the type dependency.
 */
#ifndef COMMON_NEON_FFT_FFT_TYPES_H_
#define COMMON_NEON_FFT_FFT_TYPES_H_

typedef struct FFTstr {
  int dummy; /* Unused by iSAC VAD code */
} FFTstr;

#endif  // COMMON_NEON_FFT_FFT_TYPES_H_
