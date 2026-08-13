/*
 * @Author: Ryuk
 * @Date: 2026-08-12
 * @Description: TS (Transient Suppressor) unified C interface
 *
 * Detects and suppresses transients (e.g. keyboard clicks) in an audio
 * stream using a spectral restoration algorithm.
 * Input: interleaved N-channel PCM → Output: same layout.
 */
#ifndef AUDIO_ENGINE_TS_H
#define AUDIO_ENGINE_TS_H

#include <stdint.h>

#include "audio_engine_def.h"


#ifdef __cplusplus
extern "C" {
#endif


typedef void* TsHandle;

typedef struct {
    int sample_rate;      /* 8000 / 16000 / 32000 / 48000 Hz                  */
    int frame_len;        /* samples per channel, must equal sample_rate / 100 */
    int num_channels;     /* number of channels                                */
    int detector_rate;    /* sample rate of detection signal, default = sample_rate */
} TsInitConfig;


/** Create a TS handle (opaque pointer). Returns NULL on allocation failure. */
AUDIO_ENGINE_API TsHandle AudioEngine_Ts_Create(void);

/** Destroy a TS handle and free all resources. */
AUDIO_ENGINE_API int AudioEngine_Ts_Destroy(TsHandle handle);

/** Initialize the transient suppressor. Must be called before Process. */
AUDIO_ENGINE_API int AudioEngine_Ts_Init(TsHandle handle, const TsInitConfig* init_config);

/** Process one frame of audio.
 *
 *  @param handle            TS handle (must be initialized)
 *  @param audio_in          interleaved multi-channel input PCM;
 *                           total elements = in_samples * num_channels
 *  @param in_samples        samples per channel (must equal frame_len)
 *  @param detection_in      mono detection signal (e.g. high band), may be NULL
 *                           to use the input audio for detection;
 *                           length = in_samples * detector_rate / sample_rate
 *  @param reference_in      mono reference signal (e.g. keyboard microphone),
 *                           may be NULL if unavailable;
 *                           length = in_samples
 *  @param voice_probability probability of voice in this frame [0.0, 1.0];
 *                           set to 1.0 if voice information is unavailable
 *  @param key_pressed       1 if a key was pressed on this frame, 0 otherwise
 *  @param audio_out         suppressed interleaved multi-channel output PCM;
 *                           must hold at least in_samples * num_channels
 *
 *  @return AUDIO_ENGINE_SUCCESS on success, error code otherwise.
 */
AUDIO_ENGINE_API int AudioEngine_Ts_Process(
    TsHandle handle,
    const short* audio_in,
    int in_samples,
    const short* detection_in,
    const short* reference_in,
    float voice_probability,
    int key_pressed,
    short* audio_out);

/** Deinitialize: free internal suppressor instance, keep handle alive. */
AUDIO_ENGINE_API int AudioEngine_Ts_Deinit(TsHandle handle);

/** Reset internal state: destroy and recreate suppressor instance,
 *  preserving Init configuration. */
AUDIO_ENGINE_API int AudioEngine_Ts_Reset(TsHandle handle);


#ifdef __cplusplus
}
#endif

#endif /* AUDIO_ENGINE_TS_H */
