/*
 * @Author: Ryuk
 * @Date: 2026-08-09 22:00:00
 * @LastEditors: Ryuk
 * @LastEditTime: 2026-08-09 22:00:00
 * @Description: Resample interface implementation
 */

#include <new>

#include "audio_engine_log.h"
#include "audio_engine_src.h"

#include "audio_processing/sample_rate_conversion/resampler.h"


/* ============================================================================
 * Internal data container
 * ==========================================================================*/
class AudioEngineResample {
public:
    AudioEngineResample()
        : initialized_(false)
        , resampler_(nullptr) {}

    ~AudioEngineResample() {
        if (resampler_) {
            delete resampler_;
            resampler_ = nullptr;
        }
    }

    bool initialized_;
    ResampleInitConfig init_config_;
    webrtc::Resampler* resampler_;
};


/* ============================================================================
 * Create / Destroy
 * ==========================================================================*/
AUDIO_ENGINE_API ResampleHandle AudioEngine_Resample_Create(void)
{
    LOG_DEBUG("AudioEngine_Resample_Create");

    AudioEngineResample* ptr = new (std::nothrow) AudioEngineResample();
    if (!ptr) {
        LOG_ERROR("AudioEngine_Resample_Create: memory allocation failed");
    }
    return static_cast<ResampleHandle>(ptr);
}

AUDIO_ENGINE_API int AudioEngine_Resample_Destroy(ResampleHandle handle)
{
    LOG_DEBUG("AudioEngine_Resample_Destroy, handle=%p", (void*)handle);

    if (!handle) {
        LOG_WARN("AudioEngine_Resample_Destroy: invalid handle (NULL)");
        return AUDIO_ENGINE_ERR_INVALID_HANDLE;
    }

    AudioEngineResample* ptr = static_cast<AudioEngineResample*>(handle);
    delete ptr;

    return AUDIO_ENGINE_SUCCESS;
}


/* ============================================================================
 * Init / Deinit
 * ==========================================================================*/
AUDIO_ENGINE_API int AudioEngine_Resample_Init(ResampleHandle handle, const ResampleInitConfig* init_config)
{
    LOG_DEBUG("AudioEngine_Resample_Init, handle=%p", (void*)handle);

    if (!handle) {
        LOG_WARN("AudioEngine_Resample_Init: invalid handle (NULL)");
        return AUDIO_ENGINE_ERR_INVALID_HANDLE;
    }
    if (!init_config) {
        LOG_WARN("AudioEngine_Resample_Init: init_config is NULL");
        return AUDIO_ENGINE_ERR_NULL_POINTER;
    }
    if (init_config->src_sample_rate <= 0 || init_config->dst_sample_rate <= 0) {
        LOG_WARN("AudioEngine_Resample_Init: invalid rates src=%d dst=%d",
                 init_config->src_sample_rate, init_config->dst_sample_rate);
        return AUDIO_ENGINE_ERR_INVALID_PARAM;
    }
    if (init_config->num_channels != 1 && init_config->num_channels != 2) {
        LOG_WARN("AudioEngine_Resample_Init: invalid num_channels=%d (only 1 or 2 supported)",
                 init_config->num_channels);
        return AUDIO_ENGINE_ERR_INVALID_PARAM;
    }

    AudioEngineResample* ptr = static_cast<AudioEngineResample*>(handle);

    /* If already initialized, clean up first */
    if (ptr->initialized_) {
        LOG_DEBUG("AudioEngine_Resample_Init: already initialized, deinit first");
        if (ptr->resampler_) {
            delete ptr->resampler_;
            ptr->resampler_ = nullptr;
        }
    }

    ptr->init_config_ = *init_config;

    ptr->resampler_ = new (std::nothrow) webrtc::Resampler(
        init_config->src_sample_rate, init_config->dst_sample_rate,
        static_cast<size_t>(init_config->num_channels));
    if (!ptr->resampler_) {
        LOG_ERROR("AudioEngine_Resample_Init: Resampler allocation failed");
        return AUDIO_ENGINE_ERR_INIT_FAILED;
    }

    ptr->initialized_ = true;

    LOG_INFO("AudioEngine_Resample_Init OK, src=%d dst=%d channels=%d",
             init_config->src_sample_rate, init_config->dst_sample_rate,
             init_config->num_channels);
    return AUDIO_ENGINE_SUCCESS;
}

AUDIO_ENGINE_API int AudioEngine_Resample_Deinit(ResampleHandle handle)
{
    LOG_DEBUG("AudioEngine_Resample_Deinit, handle=%p", (void*)handle);

    if (!handle) {
        LOG_WARN("AudioEngine_Resample_Deinit: invalid handle (NULL)");
        return AUDIO_ENGINE_ERR_INVALID_HANDLE;
    }

    AudioEngineResample* ptr = static_cast<AudioEngineResample*>(handle);

    if (ptr->resampler_) {
        delete ptr->resampler_;
        ptr->resampler_ = nullptr;
    }
    ptr->initialized_ = false;

    LOG_DEBUG("AudioEngine_Resample_Deinit OK");
    return AUDIO_ENGINE_SUCCESS;
}


/* ============================================================================
 * SetParam / ResetParam
 * ==========================================================================*/
AUDIO_ENGINE_API int AudioEngine_Resample_SetParam(ResampleHandle handle, const ResampleRtConfig* rt_config)
{
    LOG_DEBUG("AudioEngine_Resample_SetParam, handle=%p", (void*)handle);

    if (!handle) {
        LOG_WARN("AudioEngine_Resample_SetParam: invalid handle (NULL)");
        return AUDIO_ENGINE_ERR_INVALID_HANDLE;
    }
    if (!rt_config) {
        LOG_WARN("AudioEngine_Resample_SetParam: rt_config is NULL");
        return AUDIO_ENGINE_ERR_NULL_POINTER;
    }
    if (rt_config->reserved != 0) {
        LOG_WARN("AudioEngine_Resample_SetParam: reserved must be 0, got %d", rt_config->reserved);
        return AUDIO_ENGINE_ERR_SET_PARAM_FAILED;
    }

    /* No runtime parameters yet; config is fully determined at Init */
    LOG_INFO("AudioEngine_Resample_SetParam OK (no-op)");
    return AUDIO_ENGINE_SUCCESS;
}

AUDIO_ENGINE_API int AudioEngine_Resample_ResetParam(ResampleHandle handle, const ResampleRtConfig* rt_config)
{
    LOG_DEBUG("AudioEngine_Resample_ResetParam, handle=%p", (void*)handle);

    if (!handle) {
        LOG_WARN("AudioEngine_Resample_ResetParam: invalid handle (NULL)");
        return AUDIO_ENGINE_ERR_INVALID_HANDLE;
    }
    if (!rt_config) {
        LOG_WARN("AudioEngine_Resample_ResetParam: rt_config is NULL");
        return AUDIO_ENGINE_ERR_NULL_POINTER;
    }
    if (rt_config->reserved != 0) {
        LOG_WARN("AudioEngine_Resample_ResetParam: reserved must be 0, got %d", rt_config->reserved);
        return AUDIO_ENGINE_ERR_SET_PARAM_FAILED;
    }

    /* No runtime parameters yet; nothing to reset */
    LOG_INFO("AudioEngine_Resample_ResetParam OK (no-op)");
    return AUDIO_ENGINE_SUCCESS;
}


/* ============================================================================
 * Process
 * ==========================================================================*/
AUDIO_ENGINE_API int AudioEngine_Resample_Process(
    ResampleHandle handle,
    const short* in,
    int in_samples,
    short* out,
    int max_out_samples,
    int* out_samples)
{
    if (!handle) {
        LOG_WARN("AudioEngine_Resample_Process: invalid handle (NULL)");
        return AUDIO_ENGINE_ERR_INVALID_HANDLE;
    }
    if (!in) {
        LOG_WARN("AudioEngine_Resample_Process: in is NULL");
        return AUDIO_ENGINE_ERR_NULL_POINTER;
    }
    if (!out) {
        LOG_WARN("AudioEngine_Resample_Process: out is NULL");
        return AUDIO_ENGINE_ERR_NULL_POINTER;
    }
    if (!out_samples) {
        LOG_WARN("AudioEngine_Resample_Process: out_samples is NULL");
        return AUDIO_ENGINE_ERR_NULL_POINTER;
    }
    if (in_samples <= 0) {
        LOG_WARN("AudioEngine_Resample_Process: in_samples=%d must be > 0", in_samples);
        return AUDIO_ENGINE_ERR_INVALID_PARAM;
    }
    if (max_out_samples <= 0) {
        LOG_WARN("AudioEngine_Resample_Process: max_out_samples=%d must be > 0", max_out_samples);
        return AUDIO_ENGINE_ERR_INVALID_PARAM;
    }

    AudioEngineResample* ptr = static_cast<AudioEngineResample*>(handle);

    if (!ptr->initialized_) {
        LOG_WARN("AudioEngine_Resample_Process: not initialized");
        return AUDIO_ENGINE_ERR_NOT_INITIALIZED;
    }

    size_t resampled_samples = 0;
    int ret = ptr->resampler_->Push(in, static_cast<size_t>(in_samples),
                                    out, static_cast<size_t>(max_out_samples),
                                    resampled_samples);
    if (ret != 0) {
        LOG_ERROR("AudioEngine_Resample_Process: Resampler::Push failed, ret=%d", ret);
        return AUDIO_ENGINE_ERR_PROCESS_FAILED;
    }

    *out_samples = static_cast<int>(resampled_samples);
    return AUDIO_ENGINE_SUCCESS;
}


/* ============================================================================
 * Reset
 * ==========================================================================*/
AUDIO_ENGINE_API int AudioEngine_Resample_Reset(ResampleHandle handle)
{
    LOG_DEBUG("AudioEngine_Resample_Reset, handle=%p", (void*)handle);

    if (!handle) {
        LOG_WARN("AudioEngine_Resample_Reset: invalid handle (NULL)");
        return AUDIO_ENGINE_ERR_INVALID_HANDLE;
    }

    AudioEngineResample* ptr = static_cast<AudioEngineResample*>(handle);

    if (!ptr->initialized_) {
        LOG_WARN("AudioEngine_Resample_Reset: not initialized");
        return AUDIO_ENGINE_ERR_NOT_INITIALIZED;
    }

    /* Destroy and recreate resampler; keep init_config_ */
    if (ptr->resampler_) {
        delete ptr->resampler_;
        ptr->resampler_ = nullptr;
    }

    ptr->resampler_ = new (std::nothrow) webrtc::Resampler(
        ptr->init_config_.src_sample_rate, ptr->init_config_.dst_sample_rate,
        static_cast<size_t>(ptr->init_config_.num_channels));
    if (!ptr->resampler_) {
        LOG_ERROR("AudioEngine_Resample_Reset: Resampler allocation failed");
        ptr->initialized_ = false;
        return AUDIO_ENGINE_ERR_PROCESS_FAILED;
    }

    /* initialized_ stays true, config preserved */
    LOG_INFO("AudioEngine_Resample_Reset OK");
    return AUDIO_ENGINE_SUCCESS;
}