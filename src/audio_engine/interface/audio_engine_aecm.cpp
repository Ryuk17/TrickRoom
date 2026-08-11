/*
 * @Author: Ryuk
 * @Date: 2026-08-10 00:10:00
 * @LastEditors: Ryuk
 * @LastEditTime: 2026-08-10 00:10:00
 * @Description: AECM (Acoustic Echo Canceller Mobile) unified C interface implementation
 */

#include "audio_engine_aecm.h"
#include "audio_engine_log.h"

#include <new>

#include "audio_processing/aecm/echo_control_mobile.h"

using namespace webrtc;

/* ============================================================================
 * AECM data container (pure data, C API operates directly on it)
 * ==========================================================================*/
class AudioEngineAecm {
public:
    AudioEngineAecm() : initialized_(false), frame_size_(0), aecm_(nullptr) {
        init_config_.sample_rate = 0;
        init_config_.cng_mode  = 1;
        init_config_.echo_mode = 3;
        rt_config_.reserved = 0;
    }

    ~AudioEngineAecm() {
        if (aecm_) {
            WebRtcAecm_Free(aecm_);
            aecm_ = nullptr;
        }
    }

    bool initialized_;
    int frame_size_;               /* 10ms frame: 80 @8k, 160 @16k */
    AecmInitConfig init_config_;
    AecmRtConfig rt_config_;
    void* aecm_;
};

/* ============================================================================
 * Create / Destroy
 * ==========================================================================*/
extern "C" AecmHandle AudioEngine_Aecm_Create(void)
{
    LOG_DEBUG("AudioEngine_Aecm_Create()");
    AudioEngineAecm* ctx = new (std::nothrow) AudioEngineAecm();
    if (!ctx) {
        LOG_ERROR("AudioEngine_Aecm_Create: out of memory");
        return NULL;
    }
    return ctx;
}

extern "C" int AudioEngine_Aecm_Destroy(AecmHandle handle)
{
    LOG_DEBUG("AudioEngine_Aecm_Destroy(handle=%p)", handle);
    if (!handle) {
        LOG_WARN("AudioEngine_Aecm_Destroy: NULL handle");
        return AUDIO_ENGINE_ERR_INVALID_HANDLE;
    }
    delete static_cast<AudioEngineAecm*>(handle);
    return AUDIO_ENGINE_SUCCESS;
}

/* ============================================================================
 * Init
 * ==========================================================================*/
extern "C" int AudioEngine_Aecm_Init(AecmHandle handle, const AecmInitConfig* init_config)
{
    LOG_DEBUG("AudioEngine_Aecm_Init(handle=%p, config=%p)", handle, init_config);
    if (!handle) {
        LOG_WARN("AudioEngine_Aecm_Init: NULL handle");
        return AUDIO_ENGINE_ERR_INVALID_HANDLE;
    }
    AudioEngineAecm* ctx = static_cast<AudioEngineAecm*>(handle);
    if (!init_config) {
        LOG_WARN("AudioEngine_Aecm_Init: NULL init_config");
        return AUDIO_ENGINE_ERR_NULL_POINTER;
    }

    /* Validate parameters (WebRtcAecm_Init accepts 8000/16000 only) */
    if (init_config->sample_rate != 8000 && init_config->sample_rate != 16000) {
        LOG_ERROR("AudioEngine_Aecm_Init: invalid sample_rate=%d", init_config->sample_rate);
        return AUDIO_ENGINE_ERR_INVALID_PARAM;
    }
    if (init_config->cng_mode != 0 && init_config->cng_mode != 1) {
        LOG_ERROR("AudioEngine_Aecm_Init: invalid cng_mode=%d", init_config->cng_mode);
        return AUDIO_ENGINE_ERR_INVALID_PARAM;
    }
    if (init_config->echo_mode < 0 || init_config->echo_mode > 4) {
        LOG_ERROR("AudioEngine_Aecm_Init: invalid echo_mode=%d", init_config->echo_mode);
        return AUDIO_ENGINE_ERR_INVALID_PARAM;
    }

    /* Deinit if already initialized (re-init allowed) */
    if (ctx->aecm_) {
        WebRtcAecm_Free(ctx->aecm_);
        ctx->aecm_ = nullptr;
    }

    /* Save init config */
    ctx->init_config_ = *init_config;

    /* Create + init the AECM instance */
    ctx->aecm_ = WebRtcAecm_Create();
    if (!ctx->aecm_) {
        LOG_ERROR("AudioEngine_Aecm_Init: failed to create AECM instance");
        return AUDIO_ENGINE_ERR_INIT_FAILED;
    }
    if (WebRtcAecm_Init(ctx->aecm_, init_config->sample_rate) != 0) {
        LOG_ERROR("AudioEngine_Aecm_Init: WebRtcAecm_Init failed");
        WebRtcAecm_Free(ctx->aecm_);
        ctx->aecm_ = nullptr;
        return AUDIO_ENGINE_ERR_INIT_FAILED;
    }

    /* Apply default config (cng=1, echo=3, matching internal test) */
    AecmConfig config;
    config.cngMode  = (int16_t)init_config->cng_mode;
    config.echoMode = (int16_t)init_config->echo_mode;
    if (WebRtcAecm_set_config(ctx->aecm_, config) != 0) {
        LOG_ERROR("AudioEngine_Aecm_Init: WebRtcAecm_set_config failed");
        WebRtcAecm_Free(ctx->aecm_);
        ctx->aecm_ = nullptr;
        return AUDIO_ENGINE_ERR_SET_PARAM_FAILED;
    }

    ctx->frame_size_ = init_config->sample_rate / 100;
    ctx->initialized_ = true;

    LOG_DEBUG("AudioEngine_Aecm_Init OK: rate=%d frame_size=%d cng=%d echo=%d",
              init_config->sample_rate, ctx->frame_size_,
              init_config->cng_mode, init_config->echo_mode);
    return AUDIO_ENGINE_SUCCESS;
}

/* ============================================================================
 * SetParam / ResetParam
 * ==========================================================================*/
extern "C" int AudioEngine_Aecm_SetParam(AecmHandle handle, const AecmRtConfig* rt_config)
{
    LOG_DEBUG("AudioEngine_Aecm_SetParam(handle=%p, config=%p)", handle, rt_config);
    if (!handle) {
        LOG_WARN("AudioEngine_Aecm_SetParam: NULL handle");
        return AUDIO_ENGINE_ERR_INVALID_HANDLE;
    }
    if (!rt_config) {
        LOG_WARN("AudioEngine_Aecm_SetParam: NULL rt_config");
        return AUDIO_ENGINE_ERR_NULL_POINTER;
    }
    /* Currently all fields reserved — nothing to apply */
    static_cast<AudioEngineAecm*>(handle)->rt_config_ = *rt_config;
    return AUDIO_ENGINE_SUCCESS;
}

extern "C" int AudioEngine_Aecm_ResetParam(AecmHandle handle, const AecmRtConfig* rt_config)
{
    LOG_DEBUG("AudioEngine_Aecm_ResetParam(handle=%p, config=%p)", handle, rt_config);
    if (!handle) {
        LOG_WARN("AudioEngine_Aecm_ResetParam: NULL handle");
        return AUDIO_ENGINE_ERR_INVALID_HANDLE;
    }
    if (!rt_config) {
        LOG_WARN("AudioEngine_Aecm_ResetParam: NULL rt_config");
        return AUDIO_ENGINE_ERR_NULL_POINTER;
    }
    AudioEngineAecm* ctx = static_cast<AudioEngineAecm*>(handle);
    ctx->rt_config_.reserved = 0;
    /* Merge caller fields (only non-sentinel values) */
    if (rt_config->reserved >= 0) {
        ctx->rt_config_.reserved = rt_config->reserved;
    }
    return AUDIO_ENGINE_SUCCESS;
}

/* ============================================================================
 * Process — dual input: near-end (mic) + far-end (render)
 * ==========================================================================*/
extern "C" int AudioEngine_Aecm_Process(
    AecmHandle handle,
    const short* nearend_in,
    const short* farend_in,
    int in_samples,
    short* audio_out,
    int max_out_samples,
    int* out_samples)
{
    LOG_DEBUG("AudioEngine_Aecm_Process(handle=%p, nearend=%p, farend=%p, in_samples=%d, out=%p, max_out=%d, out_samples=%p)",
              handle, nearend_in, farend_in, in_samples, audio_out, max_out_samples, out_samples);
    if (!handle) {
        LOG_WARN("AudioEngine_Aecm_Process: NULL handle");
        return AUDIO_ENGINE_ERR_INVALID_HANDLE;
    }
    AudioEngineAecm* ctx = static_cast<AudioEngineAecm*>(handle);
    if (!ctx->initialized_) {
        LOG_WARN("AudioEngine_Aecm_Process: not initialized");
        return AUDIO_ENGINE_ERR_NOT_INITIALIZED;
    }
    if (!nearend_in || !farend_in || !audio_out || !out_samples) {
        LOG_WARN("AudioEngine_Aecm_Process: NULL pointer");
        return AUDIO_ENGINE_ERR_NULL_POINTER;
    }
    if (in_samples != ctx->frame_size_) {
        /* Strict 10ms frame: caller must zero-pad partial final frames */
        LOG_WARN("AudioEngine_Aecm_Process: invalid in_samples=%d (frame_size=%d)",
                 in_samples, ctx->frame_size_);
        return AUDIO_ENGINE_ERR_INVALID_PARAM;
    }
    if (max_out_samples < ctx->frame_size_) {
        LOG_WARN("AudioEngine_Aecm_Process: max_out_samples=%d < frame_size=%d",
                 max_out_samples, ctx->frame_size_);
        return AUDIO_ENGINE_ERR_INVALID_PARAM;
    }

    /* Step 1: buffer the far-end (render) reference frame */
    if (WebRtcAecm_BufferFarend(ctx->aecm_, farend_in, in_samples) != 0) {
        LOG_ERROR("AudioEngine_Aecm_Process: WebRtcAecm_BufferFarend failed");
        return AUDIO_ENGINE_ERR_PROCESS_FAILED;
    }

    /* Step 2: cancel echo on the near-end (mic) frame.
       nearendClean=NULL (no noise reduction path),
       msInSndCardBuf=0 (no sound card delay compensation) */
    if (WebRtcAecm_Process(ctx->aecm_, nearend_in,
                           nullptr, audio_out, in_samples, 0) != 0) {
        LOG_ERROR("AudioEngine_Aecm_Process: WebRtcAecm_Process failed");
        return AUDIO_ENGINE_ERR_PROCESS_FAILED;
    }

    *out_samples = in_samples;
    return AUDIO_ENGINE_SUCCESS;
}

/* ============================================================================
 * Deinit / Reset
 * ==========================================================================*/
extern "C" int AudioEngine_Aecm_Deinit(AecmHandle handle)
{
    LOG_DEBUG("AudioEngine_Aecm_Deinit(handle=%p)", handle);
    if (!handle) {
        LOG_WARN("AudioEngine_Aecm_Deinit: NULL handle");
        return AUDIO_ENGINE_ERR_INVALID_HANDLE;
    }
    AudioEngineAecm* ctx = static_cast<AudioEngineAecm*>(handle);
    if (ctx->aecm_) {
        WebRtcAecm_Free(ctx->aecm_);
        ctx->aecm_ = nullptr;
    }
    ctx->initialized_ = false;
    return AUDIO_ENGINE_SUCCESS;
}

extern "C" int AudioEngine_Aecm_Reset(AecmHandle handle)
{
    LOG_DEBUG("AudioEngine_Aecm_Reset(handle=%p)", handle);
    if (!handle) {
        LOG_WARN("AudioEngine_Aecm_Reset: NULL handle");
        return AUDIO_ENGINE_ERR_INVALID_HANDLE;
    }
    AudioEngineAecm* ctx = static_cast<AudioEngineAecm*>(handle);
    if (!ctx->initialized_) {
        LOG_WARN("AudioEngine_Aecm_Reset: not initialized");
        return AUDIO_ENGINE_ERR_NOT_INITIALIZED;
    }

    /* Rebuild instance, preserving init config */
    AecmInitConfig cfg = ctx->init_config_;
    if (ctx->aecm_) {
        WebRtcAecm_Free(ctx->aecm_);
        ctx->aecm_ = nullptr;
    }
    return AudioEngine_Aecm_Init(handle, &cfg);
}
