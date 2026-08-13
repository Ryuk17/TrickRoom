/*
 * @Author: Ryuk
 * @Date: 2026-08-12
 * @Description: IE (Intelligibility Enhancer) unified C interface
 *
 * Enhances speech intelligibility by applying frequency-domain gains to the
 * render (clear speech) signal, using a capture noise spectrum estimate
 * (typically from NS or AEC). Input: interleaved N-channel PCM → Output: same.
 *
 * Usage: Call SetNoiseEstimate to provide the latest noise spectrum, then
 * Process to enhance the render audio. Noise spectrum can be reused across
 * multiple Process calls if the noise environment is stable.
 */
#ifndef AUDIO_ENGINE_IE_H
#define AUDIO_ENGINE_IE_H

#include <stdint.h>

#include "audio_engine_def.h"


#ifdef __cplusplus
extern "C" {
#endif


typedef void* IeHandle;

typedef struct {
    int sample_rate;           /* 8000/16000/32000/48000 Hz                   */
    int frame_len;             /* 10ms = sample_rate / 100                    */
    int num_channels;          /* render channels, default 1                  */
    float decay_rate;          /* power forgetting factor, default 0.9        */
    int analysis_rate;         /* blocks between gain recalc, default 60      */
    float gain_change_limit;   /* max gain change per block, default 0.1      */
    float rho;                 /* SNR parameter for gains, default 0.02       */
} IeInitConfig;


/** Create an IE handle. Returns NULL on allocation failure. */
AUDIO_ENGINE_API IeHandle AudioEngine_Ie_Create(void);

/** Destroy an IE handle and free all resources. */
AUDIO_ENGINE_API int AudioEngine_Ie_Destroy(IeHandle handle);

/** Initialize the intelligibility enhancer. Must be called before Process. */
AUDIO_ENGINE_API int AudioEngine_Ie_Init(IeHandle handle, const IeInitConfig* init_config);

/** Set the capture noise magnitude spectrum estimate.
 *
 *  @param handle         IE handle (must be initialized)
 *  @param noise_spectrum noise magnitude spectrum array (from NS/AEC NoiseEstimate)
 *  @param num_freqs      number of frequency bins in the spectrum;
 *                        must match sample-rate-dependent internal size
 *                        (e.g. 129 bins at 16 kHz, 257 at 32 kHz)
 *
 *  @return AUDIO_ENGINE_SUCCESS on success.
 */
AUDIO_ENGINE_API int AudioEngine_Ie_SetNoiseEstimate(
    IeHandle handle,
    const float* noise_spectrum,
    int num_freqs);

/** Process one frame of render audio.
 *
 *  @param handle     IE handle (must be initialized)
 *  @param audio_in   interleaved N-channel render (clear speech) PCM input
 *  @param in_samples samples per channel (must equal frame_len from Init)
 *  @param audio_out  enhanced interleaved N-channel PCM output;
 *                    must hold at least in_samples * num_channels elements
 *
 *  @note SetNoiseEstimate must have been called at least once before Process.
 *  @return AUDIO_ENGINE_SUCCESS on success.
 */
AUDIO_ENGINE_API int AudioEngine_Ie_Process(
    IeHandle handle,
    const short* audio_in,
    int in_samples,
    short* audio_out);

/** Deinitialize: free internal enhancer instance, keep handle alive. */
AUDIO_ENGINE_API int AudioEngine_Ie_Deinit(IeHandle handle);

/** Reset internal state: destroy and recreate enhancer instance,
 *  preserving Init configuration. */
AUDIO_ENGINE_API int AudioEngine_Ie_Reset(IeHandle handle);


#ifdef __cplusplus
}
#endif

#endif /* AUDIO_ENGINE_IE_H */
