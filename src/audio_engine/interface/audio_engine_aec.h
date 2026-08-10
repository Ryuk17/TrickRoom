/*
 * @Author: Ryuk
 * @Date: 2026-08-09 18:44:06
 * @LastEditors: Ryuk
 * @LastEditTime: 2026-08-10 01:30:00
 * @Description: AEC3 (Acoustic Echo Canceller v3) unified C interface
 */
#ifndef AUDIO_ENGINE_AEC_H
#define AUDIO_ENGINE_AEC_H

#include <stdint.h>

#include "audio_engine_def.h"


#ifdef __cplusplus
extern "C" {
#endif


typedef void* AecHandle;

typedef struct {
    int sample_rate;           /* 16000/32000/48000 (10ms → rate/100 samples) */
    int num_render_channels;   /* far-end/render channels (default 1) */
    int num_capture_channels;  /* near-end/capture channels (default 1) */
} AecInitConfig;

typedef struct {
    int delay_ms;              /* audio buffer delay, -1 = keep current,
                                  >= 0 = set via SetAudioBufferDelay */
} AecRtConfig;


AUDIO_ENGINE_API AecHandle AudioEngine_Aec_Create(void);

AUDIO_ENGINE_API int AudioEngine_Aec_Destroy(AecHandle handle);

AUDIO_ENGINE_API int AudioEngine_Aec_Init(AecHandle handle, const AecInitConfig* init_config);

AUDIO_ENGINE_API int AudioEngine_Aec_SetParam(AecHandle handle, const AecRtConfig* rt_config);

AUDIO_ENGINE_API int AudioEngine_Aec_ResetParam(AecHandle handle, const AecRtConfig* rt_config);

AUDIO_ENGINE_API int AudioEngine_Aec_Process(
    AecHandle handle,
    const short* nearend_in,   /* mic/capture signal   */
    const short* farend_in,    /* render/reference     */
    int in_samples,
    short* audio_out,
    int max_out_samples,
    int* out_samples
);

AUDIO_ENGINE_API int AudioEngine_Aec_Deinit(AecHandle handle);

AUDIO_ENGINE_API int AudioEngine_Aec_Reset(AecHandle handle);


#ifdef __cplusplus
}
#endif

#endif /* AUDIO_ENGINE_AEC_H */
