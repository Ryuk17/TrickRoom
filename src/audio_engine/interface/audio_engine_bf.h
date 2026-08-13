/*
 * @Author: Ryuk
 * @Date: 2026-08-12
 * @Description: BF (Beamforming / NonlinearBeamformer) unified C interface
 *
 * Multi-channel microphone array beamforming — enhances sound from the target
 * direction while suppressing interference from other directions.
 * Input: N-channel interleaved PCM  →  Output: 1-channel beamformed PCM.
 */
#ifndef AUDIO_ENGINE_BF_H
#define AUDIO_ENGINE_BF_H

#include <stdint.h>

#include "audio_engine_def.h"


#ifdef __cplusplus
extern "C" {
#endif


typedef void* BfHandle;

/** Microphone position in Cartesian coordinates (meters).
 *  Convention:
 *    x: horizontal, positive to the right from the array's perspective
 *    y: depth,      positive forward
 *    z: vertical,   positive upwards
 */
typedef struct {
    float x;
    float y;
    float z;
} BfMicPosition;

typedef struct {
    int sample_rate;           /* 8000 / 16000 / 32000 / 48000 Hz                  */
    int num_channels;          /* number of microphones (= input channels)         */
    int frame_len;             /* samples per channel per frame,
                                  must equal sample_rate / 100 (10 ms constraint) */
    BfMicPosition* mic_pos;    /* array of num_channels microphone positions (m),
                                  internally copied — caller may free after Init  */
    float target_azimuth;      /* target azimuth in radians, default M_PI / 2     */
    float target_elevation;    /* target elevation in radians, default 0          */
} BfInitConfig;

typedef struct {
    float target_azimuth;      /* runtime re-aim azimuth (radians)  */
    float target_elevation;    /* runtime re-aim elevation (radians) */
} BfRtConfig;


/** Create a BF handle (opaque pointer). Returns NULL on allocation failure. */
AUDIO_ENGINE_API BfHandle AudioEngine_Bf_Create(void);

/** Destroy a BF handle and free all resources. */
AUDIO_ENGINE_API int AudioEngine_Bf_Destroy(BfHandle handle);

/** Initialize the beamformer. Must be called before Process.
 *  mic_pos array is copied internally — safe to free after return. */
AUDIO_ENGINE_API int AudioEngine_Bf_Init(BfHandle handle, const BfInitConfig* init_config);

/** Incremental runtime parameter update (re-aim beamformer).
 *  Only fields explicitly set are applied; others keep current value. */
AUDIO_ENGINE_API int AudioEngine_Bf_SetParam(BfHandle handle, const BfRtConfig* rt_config);

/** Full runtime parameter reset: restore defaults, then apply new config. */
AUDIO_ENGINE_API int AudioEngine_Bf_ResetParam(BfHandle handle, const BfRtConfig* rt_config);

/** Process one frame of multi-channel audio.
 *
 *  @param handle             BF handle (must be initialized)
 *  @param audio_in           interleaved multi-channel input PCM;
 *                            total elements = in_samples * num_channels
 *  @param in_samples         samples per channel (must equal frame_len from Init)
 *  @param audio_out          single-channel beamformed output PCM;
 *                            must hold at least in_samples elements
 *  @param is_target_present  [out] 1 if target signal detected, 0 otherwise;
 *                            may be NULL if caller does not need this info
 *
 *  @return AUDIO_ENGINE_SUCCESS on success, error code otherwise.
 */
AUDIO_ENGINE_API int AudioEngine_Bf_Process(
    BfHandle handle,
    const short* audio_in,
    int in_samples,
    short* audio_out,
    int* is_target_present);

/** Deinitialize: free internal beamformer instance, keep handle alive. */
AUDIO_ENGINE_API int AudioEngine_Bf_Deinit(BfHandle handle);

/** Reset internal state: destroy and recreate beamformer instance,
 *  preserving Init and runtime configuration. */
AUDIO_ENGINE_API int AudioEngine_Bf_Reset(BfHandle handle);


#ifdef __cplusplus
}
#endif

#endif /* AUDIO_ENGINE_BF_H */
