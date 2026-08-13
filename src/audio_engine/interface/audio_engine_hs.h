/*
 * @Author: Ryuk
 * @Date: 2026-08-12
 * @Description: HS (Howling Suppression) unified C interface
 *
 * Detects and suppresses acoustic feedback howling using Goertzel-based
 * tone detection and notch filtering. Processes single-channel PCM audio.
 */
#ifndef AUDIO_ENGINE_HS_H
#define AUDIO_ENGINE_HS_H

#include <stdint.h>

#include "audio_engine_def.h"


#ifdef __cplusplus
extern "C" {
#endif


typedef void* HsHandle;

typedef struct {
    int sample_rate;           /* operating sample rate, default 16000 Hz             */
    int frame_len;             /* samples per frame, no hard constraint, > 0          */
    float detect_threshold;    /* howling detection threshold, default 13.0           */
    int detect_block;          /* consecutive detection blocks to trigger, default 5  */
    float detect_freq_min;     /* minimum detection frequency in Hz, default 650      */
    float detect_freq_max;     /* maximum detection frequency in Hz, default 3000     */
    float detect_freq_step;    /* frequency step in Hz, default 25                    */
    int notch_persist_block;   /* notch filter persist blocks, default 5              */
    float notch_filter_Q;      /* notch filter Q factor, default 0.7071               */
} HsInitConfig;


/** Create an HS handle (opaque pointer). Returns NULL on allocation failure. */
AUDIO_ENGINE_API HsHandle AudioEngine_Hs_Create(void);

/** Destroy an HS handle and free all resources. */
AUDIO_ENGINE_API int AudioEngine_Hs_Destroy(HsHandle handle);

/** Initialize the howling suppressor. Must be called before Process. */
AUDIO_ENGINE_API int AudioEngine_Hs_Init(HsHandle handle, const HsInitConfig* init_config);

/** Process one frame of mono audio.
 *
 *  @param handle     HS handle (must be initialized)
 *  @param audio_in   single-channel input PCM
 *  @param in_samples samples in this frame (no strict constraint)
 *  @param audio_out  single-channel suppressed output PCM;
 *                    must hold at least in_samples elements
 *
 *  @return AUDIO_ENGINE_SUCCESS on success, error code otherwise.
 */
AUDIO_ENGINE_API int AudioEngine_Hs_Process(
    HsHandle handle,
    const short* audio_in,
    int in_samples,
    short* audio_out);

/** Deinitialize: free internal howling state, keep handle alive. */
AUDIO_ENGINE_API int AudioEngine_Hs_Deinit(HsHandle handle);

/** Reset internal state: destroy and recreate detection/filter state,
 *  preserving Init configuration. */
AUDIO_ENGINE_API int AudioEngine_Hs_Reset(HsHandle handle);


#ifdef __cplusplus
}
#endif

#endif /* AUDIO_ENGINE_HS_H */
