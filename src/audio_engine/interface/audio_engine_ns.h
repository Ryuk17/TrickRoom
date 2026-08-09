/*
 * @Author: Ryuk
 * @Date: 2026-08-09 23:00:00
 * @LastEditors: Ryuk
 * @LastEditTime: 2026-08-09 23:00:00
 * @Description: Noise Suppression unified C interface
 */
#ifndef AUDIO_ENGINE_NS_H
#define AUDIO_ENGINE_NS_H

#include <stdint.h>

#include "audio_engine_def.h"


#ifdef __cplusplus
extern "C" {
#endif


typedef void* NsHandle;

typedef struct {
    int sample_rate;         /* sample rate: 8000/16000/32000/48000    */
    int num_channels;        /* 1 = mono, 2 = stereo                   */
    int suppression_level;   /* 0=k6dB 1=k12dB 2=k18dB 3=k21dB (def 1) */
} NsInitConfig;

typedef struct {
    int reserved;            /* reserved, keep 0 */
} NsRtConfig;


AUDIO_ENGINE_API NsHandle AudioEngine_Ns_Create(void);

AUDIO_ENGINE_API int AudioEngine_Ns_Destroy(NsHandle handle);

AUDIO_ENGINE_API int AudioEngine_Ns_Init(NsHandle handle, const NsInitConfig* init_config);

AUDIO_ENGINE_API int AudioEngine_Ns_SetParam(NsHandle handle, const NsRtConfig* rt_config);

AUDIO_ENGINE_API int AudioEngine_Ns_ResetParam(NsHandle handle, const NsRtConfig* rt_config);

AUDIO_ENGINE_API int AudioEngine_Ns_Process(
    NsHandle handle,
    const short* audio_in,
    int in_samples,
    short* audio_out,
    int max_out_samples,
    int* out_samples
);

AUDIO_ENGINE_API int AudioEngine_Ns_Deinit(NsHandle handle);

AUDIO_ENGINE_API int AudioEngine_Ns_Reset(NsHandle handle);


#ifdef __cplusplus
}
#endif

#endif /* AUDIO_ENGINE_NS_H */
