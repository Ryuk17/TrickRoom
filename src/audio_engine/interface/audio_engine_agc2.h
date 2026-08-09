/*
 * @Author: Ryuk
 * @Date: 2026-08-09 23:50:00
 * @LastEditors: Ryuk
 * @LastEditTime: 2026-08-09 23:50:00
 * @Description: AGC2 unified C interface (AdaptiveDigitalGainController pipeline)
 */
#ifndef AUDIO_ENGINE_AGC2_H
#define AUDIO_ENGINE_AGC2_H

#include <stdint.h>

#include "audio_engine_def.h"


#ifdef __cplusplus
extern "C" {
#endif


typedef void* Agc2Handle;

typedef struct {
    int sample_rate;         /* sample rate: 8000/16000/32000/48000 */
    int num_channels;        /* 1 = mono, 2 = stereo                */
    float headroom_db;       /* target headroom, default 5.0f       */
    float max_gain_db;       /* max gain, default 50.0f             */
    float initial_gain_db;   /* initial gain, default 15.0f         */
    float max_gain_change_db_per_second; /* gain slew rate, default 6.0f */
    float max_output_noise_level_dbfs;   /* default -50.0f               */
} Agc2InitConfig;

typedef struct {
    int reserved;            /* reserved, keep 0 */
} Agc2RtConfig;


AUDIO_ENGINE_API Agc2Handle AudioEngine_Agc2_Create(void);

AUDIO_ENGINE_API int AudioEngine_Agc2_Destroy(Agc2Handle handle);

AUDIO_ENGINE_API int AudioEngine_Agc2_Init(Agc2Handle handle, const Agc2InitConfig* init_config);

AUDIO_ENGINE_API int AudioEngine_Agc2_SetParam(Agc2Handle handle, const Agc2RtConfig* rt_config);

AUDIO_ENGINE_API int AudioEngine_Agc2_ResetParam(Agc2Handle handle, const Agc2RtConfig* rt_config);

AUDIO_ENGINE_API int AudioEngine_Agc2_Process(
    Agc2Handle handle,
    const short* audio_in,
    int in_samples,
    short* audio_out,
    int max_out_samples,
    int* out_samples
);

AUDIO_ENGINE_API int AudioEngine_Agc2_Deinit(Agc2Handle handle);

AUDIO_ENGINE_API int AudioEngine_Agc2_Reset(Agc2Handle handle);


#ifdef __cplusplus
}
#endif

#endif /* AUDIO_ENGINE_AGC2_H */
