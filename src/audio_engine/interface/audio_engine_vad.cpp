/*
 * @Author: Ryuk
 * @Date: 2026-08-09 18:44:06
 * @LastEditors: Ryuk
 * @LastEditTime: 2026-08-09 21:00:00
 * @Description: VAD interface implementation
 */

#include <new>

#include "audio_engine_log.h"
#include "audio_engine_vad.h"

#include "audio_processing/voice_activity_detection/voice_activity_detector.h"


/* ============================================================================
 * Internal data container
 * ==========================================================================*/
class AudioEngineVad {
public:
    AudioEngineVad()
        : initialized_(false)
        , vad_(nullptr) {
        rt_config_.threshold = 0.5f;
    }

    ~AudioEngineVad() {
        if (vad_) {
            delete vad_;
            vad_ = nullptr;
        }
    }

    bool initialized_;
    VadInitConfig init_config_;
    VadRtConfig rt_config_;
    webrtc::VoiceActivityDetector* vad_;
};


/* ============================================================================
 * Create / Destroy
 * ==========================================================================*/
AUDIO_ENGINE_API VadHandle AudioEngine_Vad_Create(void)
{
    LOG_DEBUG("AudioEngine_Vad_Create");

    AudioEngineVad* ptr = new (std::nothrow) AudioEngineVad();
    if (!ptr) {
        LOG_ERROR("AudioEngine_Vad_Create: memory allocation failed");
    }
    return static_cast<VadHandle>(ptr);
}

AUDIO_ENGINE_API int AudioEngine_Vad_Destroy(VadHandle handle)
{
    LOG_DEBUG("AudioEngine_Vad_Destroy, handle=%p", (void*)handle);

    if (!handle) {
        LOG_WARN("AudioEngine_Vad_Destroy: invalid handle (NULL)");
        return AUDIO_ENGINE_ERR_INVALID_HANDLE;
    }

    AudioEngineVad* ptr = static_cast<AudioEngineVad*>(handle);
    delete ptr;

    return AUDIO_ENGINE_SUCCESS;
}


/* ============================================================================
 * Init / Deinit
 * ==========================================================================*/
AUDIO_ENGINE_API int AudioEngine_Vad_Init(VadHandle handle, const VadInitConfig* init_config)
{
    LOG_DEBUG("AudioEngine_Vad_Init, handle=%p", (void*)handle);

    if (!handle) {
        LOG_WARN("AudioEngine_Vad_Init: invalid handle (NULL)");
        return AUDIO_ENGINE_ERR_INVALID_HANDLE;
    }
    if (!init_config) {
        LOG_WARN("AudioEngine_Vad_Init: init_config is NULL");
        return AUDIO_ENGINE_ERR_NULL_POINTER;
    }
    if (init_config->sample_rate <= 0) {
        LOG_WARN("AudioEngine_Vad_Init: invalid sample_rate=%d", init_config->sample_rate);
        return AUDIO_ENGINE_ERR_INVALID_PARAM;
    }
    if (init_config->frame_len != init_config->sample_rate / 100) {
        LOG_WARN("AudioEngine_Vad_Init: frame_len=%d must equal sample_rate/100=%d (10ms constraint)",
                 init_config->frame_len, init_config->sample_rate / 100);
        return AUDIO_ENGINE_ERR_INVALID_PARAM;
    }

    AudioEngineVad* ptr = static_cast<AudioEngineVad*>(handle);

    /* If already initialized, clean up first */
    if (ptr->initialized_) {
        LOG_DEBUG("AudioEngine_Vad_Init: already initialized, deinit first");
        if (ptr->vad_) {
            delete ptr->vad_;
            ptr->vad_ = nullptr;
        }
    }

    ptr->init_config_ = *init_config;

    /* Set runtime config defaults — prevent uninitialized values if user
       skips SetParam before Process */
    ptr->rt_config_.threshold = 0.5f;

    ptr->vad_ = new (std::nothrow) webrtc::VoiceActivityDetector();
    if (!ptr->vad_) {
        LOG_ERROR("AudioEngine_Vad_Init: VoiceActivityDetector allocation failed");
        return AUDIO_ENGINE_ERR_INIT_FAILED;
    }

    ptr->initialized_ = true;

    LOG_INFO("AudioEngine_Vad_Init OK, sample_rate=%d, frame_len=%d, threshold=%.2f",
             init_config->sample_rate, init_config->frame_len, ptr->rt_config_.threshold);
    return AUDIO_ENGINE_SUCCESS;
}

AUDIO_ENGINE_API int AudioEngine_Vad_Deinit(VadHandle handle)
{
    LOG_DEBUG("AudioEngine_Vad_Deinit, handle=%p", (void*)handle);

    if (!handle) {
        LOG_WARN("AudioEngine_Vad_Deinit: invalid handle (NULL)");
        return AUDIO_ENGINE_ERR_INVALID_HANDLE;
    }

    AudioEngineVad* ptr = static_cast<AudioEngineVad*>(handle);

    if (ptr->vad_) {
        delete ptr->vad_;
        ptr->vad_ = nullptr;
    }
    ptr->initialized_ = false;

    LOG_DEBUG("AudioEngine_Vad_Deinit OK");
    return AUDIO_ENGINE_SUCCESS;
}


/* ============================================================================
 * SetParam / ResetParam
 * ==========================================================================*/
AUDIO_ENGINE_API int AudioEngine_Vad_SetParam(VadHandle handle, const VadRtConfig* rt_config)
{
    LOG_DEBUG("AudioEngine_Vad_SetParam, handle=%p", (void*)handle);

    if (!handle) {
        LOG_WARN("AudioEngine_Vad_SetParam: invalid handle (NULL)");
        return AUDIO_ENGINE_ERR_INVALID_HANDLE;
    }
    if (!rt_config) {
        LOG_WARN("AudioEngine_Vad_SetParam: rt_config is NULL");
        return AUDIO_ENGINE_ERR_NULL_POINTER;
    }
    if (rt_config->threshold <= 0.0f || rt_config->threshold > 1.0f) {
        LOG_WARN("AudioEngine_Vad_SetParam: threshold=%.2f out of range (0.0, 1.0]",
                 rt_config->threshold);
        return AUDIO_ENGINE_ERR_INVALID_PARAM;
    }

    AudioEngineVad* ptr = static_cast<AudioEngineVad*>(handle);

    /* Incremental update: only overwrite passed-in fields */
    ptr->rt_config_.threshold = rt_config->threshold;

    LOG_INFO("AudioEngine_Vad_SetParam OK, threshold=%.2f", rt_config->threshold);
    return AUDIO_ENGINE_SUCCESS;
}

AUDIO_ENGINE_API int AudioEngine_Vad_ResetParam(VadHandle handle, const VadRtConfig* rt_config)
{
    LOG_DEBUG("AudioEngine_Vad_ResetParam, handle=%p", (void*)handle);

    if (!handle) {
        LOG_WARN("AudioEngine_Vad_ResetParam: invalid handle (NULL)");
        return AUDIO_ENGINE_ERR_INVALID_HANDLE;
    }
    if (!rt_config) {
        LOG_WARN("AudioEngine_Vad_ResetParam: rt_config is NULL");
        return AUDIO_ENGINE_ERR_NULL_POINTER;
    }
    if (rt_config->threshold <= 0.0f || rt_config->threshold > 1.0f) {
        LOG_WARN("AudioEngine_Vad_ResetParam: threshold=%.2f out of range (0.0, 1.0]",
                 rt_config->threshold);
        return AUDIO_ENGINE_ERR_INVALID_PARAM;
    }

    AudioEngineVad* ptr = static_cast<AudioEngineVad*>(handle);

    /* Full reset: restore factory defaults first, then apply new config */
    ptr->rt_config_.threshold = 0.5f;

    ptr->rt_config_.threshold = rt_config->threshold;

    LOG_INFO("AudioEngine_Vad_ResetParam OK, threshold=%.2f", rt_config->threshold);
    return AUDIO_ENGINE_SUCCESS;
}


/* ============================================================================
 * Process
 * ==========================================================================*/
AUDIO_ENGINE_API int AudioEngine_Vad_Process(
    VadHandle handle,
    const short* audio_in,
    int samples,
    int *vad_flag)
{
    if (!handle) {
        LOG_WARN("AudioEngine_Vad_Process: invalid handle (NULL)");
        return AUDIO_ENGINE_ERR_INVALID_HANDLE;
    }
    if (!audio_in) {
        LOG_WARN("AudioEngine_Vad_Process: audio_in is NULL");
        return AUDIO_ENGINE_ERR_NULL_POINTER;
    }
    if (!vad_flag) {
        LOG_WARN("AudioEngine_Vad_Process: vad_flag is NULL");
        return AUDIO_ENGINE_ERR_NULL_POINTER;
    }

    AudioEngineVad* ptr = static_cast<AudioEngineVad*>(handle);

    if (!ptr->initialized_) {
        LOG_WARN("AudioEngine_Vad_Process: not initialized");
        return AUDIO_ENGINE_ERR_NOT_INITIALIZED;
    }

    const int expected_samples = ptr->init_config_.sample_rate / 100;
    if (samples != expected_samples) {
        LOG_WARN("AudioEngine_Vad_Process: samples=%d, expected %d (10ms constraint)",
                 samples, expected_samples);
        return AUDIO_ENGINE_ERR_INVALID_PARAM;
    }

    ptr->vad_->ProcessChunk(audio_in, static_cast<size_t>(samples),
                            ptr->init_config_.sample_rate);

    float prob = ptr->vad_->last_voice_probability();
    /* Strict comparison matches internal test semantics (`p > threshold`).
       WebRTC StandaloneVad emits exactly 0.5 for active frames, so `>=`
       would wrongly classify those neutral frames as voice. */
    *vad_flag = (prob > ptr->rt_config_.threshold) ? 1 : 0;

    return AUDIO_ENGINE_SUCCESS;
}


/* ============================================================================
 * Reset
 * ==========================================================================*/
AUDIO_ENGINE_API int AudioEngine_Vad_Reset(VadHandle handle)
{
    LOG_DEBUG("AudioEngine_Vad_Reset, handle=%p", (void*)handle);

    if (!handle) {
        LOG_WARN("AudioEngine_Vad_Reset: invalid handle (NULL)");
        return AUDIO_ENGINE_ERR_INVALID_HANDLE;
    }

    AudioEngineVad* ptr = static_cast<AudioEngineVad*>(handle);

    if (!ptr->initialized_) {
        LOG_WARN("AudioEngine_Vad_Reset: not initialized");
        return AUDIO_ENGINE_ERR_NOT_INITIALIZED;
    }

    /* Destroy and recreate VAD instance; keep init_config_ and rt_config_ */
    if (ptr->vad_) {
        delete ptr->vad_;
        ptr->vad_ = nullptr;
    }

    ptr->vad_ = new (std::nothrow) webrtc::VoiceActivityDetector();
    if (!ptr->vad_) {
        LOG_ERROR("AudioEngine_Vad_Reset: VoiceActivityDetector allocation failed");
        ptr->initialized_ = false;
        return AUDIO_ENGINE_ERR_PROCESS_FAILED;
    }

    /* initialized_ stays true, configs preserved */
    LOG_INFO("AudioEngine_Vad_Reset OK, threshold=%.2f", ptr->rt_config_.threshold);
    return AUDIO_ENGINE_SUCCESS;
}