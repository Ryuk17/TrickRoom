/*
 * @Author: Ryuk
 * @Date: 2026-08-09 23:30:00
 * @LastEditors: Ryuk
 * @LastEditTime: 2026-08-09 23:30:00
 * @Description: Legacy AGC unified C interface
 */
#ifndef AUDIO_ENGINE_AGC_H
#define AUDIO_ENGINE_AGC_H

#include <stdint.h>

#include "audio_engine_def.h"


#ifdef __cplusplus
extern "C" {
#endif


typedef void* AgcHandle;

typedef struct {
    int sample_rate;       /* sample rate: 8000/16000/32000/48000     */
    int agc_mode;          /* 0=Unchanged 1=AdaptiveAnalog             */
                           /* 2=AdaptiveDigital 3=FixedDigital         */
    int min_level;         /* mic level min (typical 0)                */
    int max_level;         /* mic level max (typical 255)              */
} AgcInitConfig;

typedef struct {
    int compression_gain_db;  /* 0..90, -1 = keep current             */
    int limiter_enable;       /* 0/1,   -1 = keep current             */
    int target_level_dbfs;    /* 0..31, -1 = keep current             */
} AgcRtConfig;


AUDIO_ENGINE_API AgcHandle AudioEngine_Agc_Create(void);

AUDIO_ENGINE_API int AudioEngine_Agc_Destroy(AgcHandle handle);

AUDIO_ENGINE_API int AudioEngine_Agc_Init(AgcHandle handle, const AgcInitConfig* init_config);

AUDIO_ENGINE_API int AudioEngine_Agc_SetParam(AgcHandle handle, const AgcRtConfig* rt_config);

AUDIO_ENGINE_API int AudioEngine_Agc_ResetParam(AgcHandle handle, const AgcRtConfig* rt_config);

AUDIO_ENGINE_API int AudioEngine_Agc_Process(
    AgcHandle handle,
    const short* audio_in,
    int in_samples,
    short* audio_out,
    int max_out_samples,
    int* out_samples
);

AUDIO_ENGINE_API int AudioEngine_Agc_Deinit(AgcHandle handle);

AUDIO_ENGINE_API int AudioEngine_Agc_Reset(AgcHandle handle);


#ifdef __cplusplus
}
#endif

#endif /* AUDIO_ENGINE_AGC_H */
