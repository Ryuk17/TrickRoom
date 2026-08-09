/*
 * @Author: Ryuk
 * @Date: 2026-08-10 00:10:00
 * @LastEditors: Ryuk
 * @LastEditTime: 2026-08-10 00:10:00
 * @Description: AECM (Acoustic Echo Canceller Mobile) unified C interface
 */
#ifndef AUDIO_ENGINE_AECM_H
#define AUDIO_ENGINE_AECM_H

#include <stdint.h>

#include "audio_engine_def.h"


#ifdef __cplusplus
extern "C" {
#endif


typedef void* AecmHandle;

typedef struct {
    int sample_rate;     /* 8000/16000 (10ms -> 80/160 samples)     */
    int cng_mode;        /* comfort noise: 0=OFF 1=ON (default 1)   */
    int echo_mode;       /* 0..4 echo suppression (default 3)       */
} AecmInitConfig;

typedef struct {
    int reserved;        /* reserved, keep 0 */
} AecmRtConfig;


AUDIO_ENGINE_API AecmHandle AudioEngine_Aecm_Create(void);

AUDIO_ENGINE_API int AudioEngine_Aecm_Destroy(AecmHandle handle);

AUDIO_ENGINE_API int AudioEngine_Aecm_Init(AecmHandle handle, const AecmInitConfig* init_config);

AUDIO_ENGINE_API int AudioEngine_Aecm_SetParam(AecmHandle handle, const AecmRtConfig* rt_config);

AUDIO_ENGINE_API int AudioEngine_Aecm_ResetParam(AecmHandle handle, const AecmRtConfig* rt_config);

AUDIO_ENGINE_API int AudioEngine_Aecm_Process(
    AecmHandle handle,
    const short* nearend_in,   /* mic/capture signal (with echo)   */
    const short* farend_in,    /* far-end render/reference signal */
    int in_samples,
    short* audio_out,
    int max_out_samples,
    int* out_samples
);

AUDIO_ENGINE_API int AudioEngine_Aecm_Deinit(AecmHandle handle);

AUDIO_ENGINE_API int AudioEngine_Aecm_Reset(AecmHandle handle);


#ifdef __cplusplus
}
#endif

#endif /* AUDIO_ENGINE_AECM_H */
