/*
 * @Author: Ryuk
 * @Date: 2026-08-15
 * @Description: DR (Dereverberation) unified C interface
 *
 * Single-channel speech dereverberation and enhancement (Doire et al. 2017,
 * C++ port of voicebox v_spendred.m). Streaming: Process() consumes one
 * frame of frame_len samples (16 kHz) and produces one frame of enhanced
 * output. The first 11 output frames are silent (algorithm initialization).
 *
 * Hard constraints (algorithm level):
 *   - sample_rate must be 16000
 *   - frame_len * overlap_factor must equal 384 (FFT length);
 *     with the default config frame_len == 64 (4 ms)
 *
 * Config defaults: fields set to 0 (or outside the documented valid domain)
 * are replaced by the algorithm defaults listed below.
 */
#ifndef AUDIO_ENGINE_DR_H
#define AUDIO_ENGINE_DR_H

#include <stdint.h>

#include "audio_engine_def.h"


#ifdef __cplusplus
extern "C" {
#endif


typedef void* DrHandle;

typedef struct {
    int sample_rate;              /* must be 16000 (no default)                    */

    /* ---- algorithm config (0 / out-of-domain -> default) ---- */
    int    overlap_factor;        /* FFT length / frame len, default 6, valid >= 2 */
    double frame_increment_s;     /* desired frame increment, default 5e-3, > 0    */
    int    round_frame_increment; /* 0 -> default 1 (round to nearest power of 2);
                                     any nonzero value -> enabled                 */
    int    spectral_gain_type;    /* default 1, valid 1..3:
                                     1=Wiener, 2=power spectral subtraction,
                                     3=MMSE speech estimate                      */
    double gain_smoothing;        /* spectral gain smoothing, default 0.95, (0,1] */
    double gain_floor;            /* spectral gain floor, default 1e-5, > 0       */
    double oversubtraction;       /* interference oversubtraction, default 2.0    */
    int    num_states;            /* HMM states, default 6, valid 2..6            */
    int    posterior_mode;        /* default 1, valid 1..2: 1=max track,
                                     2=weighted sum                              */
    double energy_floor_db;       /* energy floor in dB, default -60, [-200, 0)   */
    double clip_reference_db;     /* log-power clip reference in dB,
                                     0 -> default -1e300 (running max);
                                     set to the signal-wide maximum to reproduce
                                     MATLAB batch behavior                       */
} DrInitConfig;


/** Create a DR handle (opaque pointer). Returns NULL on allocation failure. */
AUDIO_ENGINE_API DrHandle AudioEngine_Dr_Create(void);

/** Destroy a DR handle and free all resources. */
AUDIO_ENGINE_API int AudioEngine_Dr_Destroy(DrHandle handle);

/** Initialize the dereverberator. Must be called before Process. */
AUDIO_ENGINE_API int AudioEngine_Dr_Init(DrHandle handle, const DrInitConfig* init_config);

/** Process one frame of mono audio.
 *
 *  @param handle     DR handle (must be initialized)
 *  @param audio_in   single-channel input PCM
 *  @param in_samples samples in this frame; must equal the frame length
 *                    derived from the config (64 with the default config).
 *                    Shorter final frames must be zero-padded to frame length.
 *  @param audio_out  single-channel enhanced output PCM;
 *                    must hold at least in_samples elements
 *
 *  @return AUDIO_ENGINE_SUCCESS on success, error code otherwise.
 */
AUDIO_ENGINE_API int AudioEngine_Dr_Process(
    DrHandle handle,
    const short* audio_in,
    int in_samples,
    short* audio_out);

/** Deinitialize: free internal dereverberator instance, keep handle alive. */
AUDIO_ENGINE_API int AudioEngine_Dr_Deinit(DrHandle handle);

/** Reset internal state: destroy and recreate the dereverberator instance,
 *  preserving Init configuration. */
AUDIO_ENGINE_API int AudioEngine_Dr_Reset(DrHandle handle);


#ifdef __cplusplus
}
#endif

#endif /* AUDIO_ENGINE_DR_H */
