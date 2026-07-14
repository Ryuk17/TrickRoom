/*
 * Minimal stub for the WebRTC FFT module.
 * The compiled iSAC VAD files do not actually call any FFT functions,
 * they only need the FFTstr type definition for struct declarations.
 */
#ifndef MODULES_THIRD_PARTY_FFT_FFT_H_
#define MODULES_THIRD_PARTY_FFT_FFT_H_

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct FFTstr {
  int dummy; /* Unused by iSAC VAD code */
} FFTstr;

#ifdef __cplusplus
}
#endif

#endif /* MODULES_THIRD_PARTY_FFT_FFT_H_ */
