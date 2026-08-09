/*
 * @Author: Ryuk
 * @Date: 2026-08-09 18:44:06
 * @LastEditors: Ryuk
 * @LastEditTime: 2026-08-09 20:09:12
 * @Description: First create
 */
#ifndef AUDIO_ENGINE_AEC_H
#define AUDIO_ENGINE_AEC_H

#include <stdint.h>


#ifdef __cplusplus
extern "C" {
#endif


typedef void* AecHandle;

typedef struct {
    int sample_rate;       
    int far_end_channels;          
    int near_end_channels;  

    int ace_mode;
    int delay;
} AecInitConfig;

typedef struct {
    int nlp_aggressiveness;
} AecRtConfig;


AecHandle AudioEngine_Aec_Create(void);

int AudioEngine_Aec_Destroy(AecHandle handle);

int AudioEngine_Aec_Init(AecHandle handle, const AecInitConfig* init_config);

int AudioEngine_Aec_SetParam(AecHandle handle, const AecRtConfig* rt_config);

int AudioEngine_Aec_ResetParam(AecHandle handle, const AecRtConfig* rt_config);

int AudioEngine_Aec_Process(
    AecHandle handle,
    const short* mic_in,
    const short* ref_in,
    short* out,
    int samples
);

int AudioEngine_Aec_Deinit(AecHandle handle);






#ifdef __cplusplus
}
#endif

#endif /* AUDIO_ENGINE_AEC_H */