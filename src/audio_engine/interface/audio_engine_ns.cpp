/*
 * @Author: Ryuk
 * @Date: 2026-08-09 23:00:00
 * @LastEditors: Ryuk
 * @LastEditTime: 2026-08-09 23:00:00
 * @Description: Noise Suppression interface implementation
 */

#include <new>

#include "audio_engine_def.h"
#include "audio_engine_ns.h"

#include "utils/audio_buffer.h"
#include "utils/stream_config.h"
#include "audio_processing/ns/noise_suppressor.h"


/* ============================================================================
 * Internal data container
 * ==========================================================================*/
class AudioEngineNs {
public:
    AudioEngineNs()
        : initialized_(false)
        , ns_(nullptr)
        , audio_buffer_(nullptr)
    {
        init_config_.suppression_level = 1;   /* default k12dB */
    }

    ~AudioEngineNs() {
        if (ns_) {
            delete ns_;
            ns_ = nullptr;
        }
        if (audio_buffer_) {
            delete audio_buffer_;
            audio_buffer_ = nullptr;
        }
    }

    bool initialized_;
    NsInitConfig init_config_;
    webrtc::NoiseSuppressor* ns_;
    webrtc::AudioBuffer* audio_buffer_;
    webrtc::StreamConfig stream_config_;
};


/* Map interface suppression_level (0-3) to webrtc NsConfig level */
static webrtc::NsConfig::SuppressionLevel map_suppression_level(int level)
{
    switch (level) {
    case 0:
        return webrtc::NsConfig::SuppressionLevel::k6dB;
    case 1:
        return webrtc::NsConfig::SuppressionLevel::k12dB;
    case 2:
        return webrtc::NsConfig::SuppressionLevel::k18dB;
    case 3:
    default:
        return webrtc::NsConfig::SuppressionLevel::k21dB;
    }
}


/* ============================================================================
 * Create / Destroy
 * ==========================================================================*/
AUDIO_ENGINE_API NsHandle AudioEngine_Ns_Create(void)
{
    LOG_DEBUG("AudioEngine_Ns_Create");

    AudioEngineNs* ptr = new (std::nothrow) AudioEngineNs();
    if (!ptr) {
        LOG_ERROR("AudioEngine_Ns_Create: memory allocation failed");
    }
    return static_cast<NsHandle>(ptr);
}

AUDIO_ENGINE_API int AudioEngine_Ns_Destroy(NsHandle handle)
{
    LOG_DEBUG("AudioEngine_Ns_Destroy, handle=%p", (void*)handle);

    if (!handle) {
        LOG_WARN("AudioEngine_Ns_Destroy: invalid handle (NULL)");
        return AUDIO_ENGINE_ERR_INVALID_HANDLE;
    }

    AudioEngineNs* ptr = static_cast<AudioEngineNs*>(handle);
    delete ptr;

    return AUDIO_ENGINE_SUCCESS;
}


/* ============================================================================
 * Init / Deinit
 * ==========================================================================*/
AUDIO_ENGINE_API int AudioEngine_Ns_Init(NsHandle handle, const NsInitConfig* init_config)
{
    LOG_DEBUG("AudioEngine_Ns_Init, handle=%p", (void*)handle);

    if (!handle) {
        LOG_WARN("AudioEngine_Ns_Init: invalid handle (NULL)");
        return AUDIO_ENGINE_ERR_INVALID_HANDLE;
    }
    if (!init_config) {
        LOG_WARN("AudioEngine_Ns_Init: init_config is NULL");
        return AUDIO_ENGINE_ERR_NULL_POINTER;
    }
    if (init_config->sample_rate <= 0) {
        LOG_WARN("AudioEngine_Ns_Init: invalid sample_rate=%d", init_config->sample_rate);
        return AUDIO_ENGINE_ERR_INVALID_PARAM;
    }
    if (init_config->num_channels != 1 && init_config->num_channels != 2) {
        LOG_WARN("AudioEngine_Ns_Init: invalid num_channels=%d (only 1 or 2 supported)",
                 init_config->num_channels);
        return AUDIO_ENGINE_ERR_INVALID_PARAM;
    }
    if (init_config->suppression_level < 0 || init_config->suppression_level > 3) {
        LOG_WARN("AudioEngine_Ns_Init: invalid suppression_level=%d (0-3)",
                 init_config->suppression_level);
        return AUDIO_ENGINE_ERR_INVALID_PARAM;
    }

    AudioEngineNs* ptr = static_cast<AudioEngineNs*>(handle);

    /* If already initialized, clean up first */
    if (ptr->initialized_) {
        LOG_DEBUG("AudioEngine_Ns_Init: already initialized, deinit first");
        if (ptr->ns_) {
            delete ptr->ns_;
            ptr->ns_ = nullptr;
        }
        if (ptr->audio_buffer_) {
            delete ptr->audio_buffer_;
            ptr->audio_buffer_ = nullptr;
        }
    }

    ptr->init_config_ = *init_config;

    size_t rate = static_cast<size_t>(init_config->sample_rate);
    size_t channels = static_cast<size_t>(init_config->num_channels);

    ptr->audio_buffer_ = new (std::nothrow) webrtc::AudioBuffer(
        rate, channels, rate, channels, rate, channels);
    if (!ptr->audio_buffer_) {
        LOG_ERROR("AudioEngine_Ns_Init: AudioBuffer allocation failed");
        return AUDIO_ENGINE_ERR_INIT_FAILED;
    }

    webrtc::NsConfig ns_config;
    ns_config.target_level = map_suppression_level(init_config->suppression_level);

    ptr->ns_ = new (std::nothrow) webrtc::NoiseSuppressor(ns_config, rate, channels);
    if (!ptr->ns_) {
        LOG_ERROR("AudioEngine_Ns_Init: NoiseSuppressor allocation failed");
        delete ptr->audio_buffer_;
        ptr->audio_buffer_ = nullptr;
        return AUDIO_ENGINE_ERR_INIT_FAILED;
    }

    ptr->stream_config_ = webrtc::StreamConfig(init_config->sample_rate, channels);
    ptr->initialized_ = true;

    LOG_INFO("AudioEngine_Ns_Init OK, rate=%d channels=%d level=%d",
             init_config->sample_rate, init_config->num_channels,
             init_config->suppression_level);
    return AUDIO_ENGINE_SUCCESS;
}

AUDIO_ENGINE_API int AudioEngine_Ns_Deinit(NsHandle handle)
{
    LOG_DEBUG("AudioEngine_Ns_Deinit, handle=%p", (void*)handle);

    if (!handle) {
        LOG_WARN("AudioEngine_Ns_Deinit: invalid handle (NULL)");
        return AUDIO_ENGINE_ERR_INVALID_HANDLE;
    }

    AudioEngineNs* ptr = static_cast<AudioEngineNs*>(handle);

    if (ptr->ns_) {
        delete ptr->ns_;
        ptr->ns_ = nullptr;
    }
    if (ptr->audio_buffer_) {
        delete ptr->audio_buffer_;
        ptr->audio_buffer_ = nullptr;
    }
    ptr->initialized_ = false;

    LOG_DEBUG("AudioEngine_Ns_Deinit OK");
    return AUDIO_ENGINE_SUCCESS;
}


/* ============================================================================
 * SetParam / ResetParam
 * ==========================================================================*/
AUDIO_ENGINE_API int AudioEngine_Ns_SetParam(NsHandle handle, const NsRtConfig* rt_config)
{
    LOG_DEBUG("AudioEngine_Ns_SetParam, handle=%p", (void*)handle);

    if (!handle) {
        LOG_WARN("AudioEngine_Ns_SetParam: invalid handle (NULL)");
        return AUDIO_ENGINE_ERR_INVALID_HANDLE;
    }
    if (!rt_config) {
        LOG_WARN("AudioEngine_Ns_SetParam: rt_config is NULL");
        return AUDIO_ENGINE_ERR_NULL_POINTER;
    }
    if (rt_config->reserved != 0) {
        LOG_WARN("AudioEngine_Ns_SetParam: reserved must be 0, got %d", rt_config->reserved);
        return AUDIO_ENGINE_ERR_SET_PARAM_FAILED;
    }

    /* No runtime parameters yet; suppression level is fixed at Init */
    LOG_INFO("AudioEngine_Ns_SetParam OK (no-op)");
    return AUDIO_ENGINE_SUCCESS;
}

AUDIO_ENGINE_API int AudioEngine_Ns_ResetParam(NsHandle handle, const NsRtConfig* rt_config)
{
    LOG_DEBUG("AudioEngine_Ns_ResetParam, handle=%p", (void*)handle);

    if (!handle) {
        LOG_WARN("AudioEngine_Ns_ResetParam: invalid handle (NULL)");
        return AUDIO_ENGINE_ERR_INVALID_HANDLE;
    }
    if (!rt_config) {
        LOG_WARN("AudioEngine_Ns_ResetParam: rt_config is NULL");
        return AUDIO_ENGINE_ERR_NULL_POINTER;
    }
    if (rt_config->reserved != 0) {
        LOG_WARN("AudioEngine_Ns_ResetParam: reserved must be 0, got %d", rt_config->reserved);
        return AUDIO_ENGINE_ERR_SET_PARAM_FAILED;
    }

    /* No runtime parameters yet; nothing to reset */
    LOG_INFO("AudioEngine_Ns_ResetParam OK (no-op)");
    return AUDIO_ENGINE_SUCCESS;
}


/* ============================================================================
 * Process
 * ==========================================================================*/
AUDIO_ENGINE_API int AudioEngine_Ns_Process(
    NsHandle handle,
    const short* audio_in,
    int in_samples,
    short* audio_out,
    int max_out_samples,
    int* out_samples)
{
    if (!handle) {
        LOG_WARN("AudioEngine_Ns_Process: invalid handle (NULL)");
        return AUDIO_ENGINE_ERR_INVALID_HANDLE;
    }
    if (!audio_in) {
        LOG_WARN("AudioEngine_Ns_Process: audio_in is NULL");
        return AUDIO_ENGINE_ERR_NULL_POINTER;
    }
    if (!audio_out) {
        LOG_WARN("AudioEngine_Ns_Process: audio_out is NULL");
        return AUDIO_ENGINE_ERR_NULL_POINTER;
    }
    if (!out_samples) {
        LOG_WARN("AudioEngine_Ns_Process: out_samples is NULL");
        return AUDIO_ENGINE_ERR_NULL_POINTER;
    }

    AudioEngineNs* ptr = static_cast<AudioEngineNs*>(handle);

    if (!ptr->initialized_) {
        LOG_WARN("AudioEngine_Ns_Process: not initialized");
        return AUDIO_ENGINE_ERR_NOT_INITIALIZED;
    }

    /* Fixed 10ms frame: in_samples must equal sample_rate / 100 */
    const int frame_len = ptr->init_config_.sample_rate / 100;
    if (in_samples != frame_len) {
        LOG_WARN("AudioEngine_Ns_Process: in_samples=%d must be %d (10ms)",
                 in_samples, frame_len);
        return AUDIO_ENGINE_ERR_INVALID_PARAM;
    }
    if (max_out_samples < in_samples * ptr->init_config_.num_channels) {
        LOG_WARN("AudioEngine_Ns_Process: max_out_samples=%d too small, need %d",
                 max_out_samples, in_samples * ptr->init_config_.num_channels);
        return AUDIO_ENGINE_ERR_INVALID_PARAM;
    }

    const int rate = ptr->init_config_.sample_rate;

    if (rate > 16000) {
        ptr->audio_buffer_->SplitIntoFrequencyBands();
    }

    ptr->audio_buffer_->CopyFrom(audio_in, ptr->stream_config_);

    ptr->ns_->Analyze(*ptr->audio_buffer_);
    ptr->ns_->Process(ptr->audio_buffer_);

    if (rate > 16000) {
        ptr->audio_buffer_->MergeFrequencyBands();
    }

    ptr->audio_buffer_->CopyTo(ptr->stream_config_, audio_out);

    *out_samples = in_samples;
    return AUDIO_ENGINE_SUCCESS;
}


/* ============================================================================
 * Reset
 * ==========================================================================*/
AUDIO_ENGINE_API int AudioEngine_Ns_Reset(NsHandle handle)
{
    LOG_DEBUG("AudioEngine_Ns_Reset, handle=%p", (void*)handle);

    if (!handle) {
        LOG_WARN("AudioEngine_Ns_Reset: invalid handle (NULL)");
        return AUDIO_ENGINE_ERR_INVALID_HANDLE;
    }

    AudioEngineNs* ptr = static_cast<AudioEngineNs*>(handle);

    if (!ptr->initialized_) {
        LOG_WARN("AudioEngine_Ns_Reset: not initialized");
        return AUDIO_ENGINE_ERR_NOT_INITIALIZED;
    }

    /* Rebuild NoiseSuppressor with preserved config; AudioBuffer is stateless */
    if (ptr->ns_) {
        delete ptr->ns_;
        ptr->ns_ = nullptr;
    }

    webrtc::NsConfig ns_config;
    ns_config.target_level = map_suppression_level(ptr->init_config_.suppression_level);

    ptr->ns_ = new (std::nothrow) webrtc::NoiseSuppressor(
        ns_config,
        static_cast<size_t>(ptr->init_config_.sample_rate),
        static_cast<size_t>(ptr->init_config_.num_channels));
    if (!ptr->ns_) {
        LOG_ERROR("AudioEngine_Ns_Reset: NoiseSuppressor allocation failed");
        ptr->initialized_ = false;
        return AUDIO_ENGINE_ERR_PROCESS_FAILED;
    }

    /* initialized_ stays true, config preserved */
    LOG_INFO("AudioEngine_Ns_Reset OK");
    return AUDIO_ENGINE_SUCCESS;
}
