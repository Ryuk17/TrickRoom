/*
 * @Author: Ryuk
 * @Date: 2026-08-09 18:44:06
 * @LastEditors: Ryuk
 * @LastEditTime: 2026-08-09 21:00:00
 * @Description: VAD (Voice Activity Detection) unified C interface
 */
#ifndef AUDIO_ENGINE_VAD_H
#define AUDIO_ENGINE_VAD_H

#include <stdint.h>

#include "audio_engine_def.h"


#ifdef __cplusplus
extern "C" {
#endif


typedef void* VadHandle;

typedef struct {
    int sample_rate;     /* e.g. 8000, 16000, 32000, 48000          */
    int frame_len;       /* samples per frame, must == sample_rate/100 (10ms) */
} VadInitConfig;

typedef struct {
    float threshold;     /* voice probability threshold, default 0.5, range (0.0, 1.0) */
} VadRtConfig;


AUDIO_ENGINE_API VadHandle AudioEngine_Vad_Create(void);

AUDIO_ENGINE_API int AudioEngine_Vad_Destroy(VadHandle handle);

AUDIO_ENGINE_API int AudioEngine_Vad_Init(VadHandle handle, const VadInitConfig* init_config);

AUDIO_ENGINE_API int AudioEngine_Vad_SetParam(VadHandle handle, const VadRtConfig* rt_config);

AUDIO_ENGINE_API int AudioEngine_Vad_ResetParam(VadHandle handle, const VadRtConfig* rt_config);

AUDIO_ENGINE_API int AudioEngine_Vad_Process(
    VadHandle handle,
    const short* audio_in,
    int samples,
    int *vad_flag
);

AUDIO_ENGINE_API int AudioEngine_Vad_Deinit(VadHandle handle);

AUDIO_ENGINE_API int AudioEngine_Vad_Reset(VadHandle handle);


#ifdef __cplusplus
}
#endif

#endif /* AUDIO_ENGINE_VAD_H */