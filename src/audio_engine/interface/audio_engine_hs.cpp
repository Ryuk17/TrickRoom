/*
 * @Author: Ryuk
 * @Date: 2026-08-12
 * @Description: HS (Howling Suppression) interface implementation
 */

#include "audio_engine_hs.h"
#include "audio_engine_log.h"

#include <new>
#include <cstring>

#include "audio_processing/howling_suppressor/howling_core.h"


/* ============================================================================
 * Internal data container (pure data — C API operates directly on it)
 * ==========================================================================*/
class AudioEngineHs {
public:
    AudioEngineHs()
        : initialized_(false)
        , state_inited_(false) {
        memset(&state_, 0, sizeof(state_));
        /* Factory defaults matching howling_core.c internals */
        init_config_.sample_rate       = 16000;
        init_config_.frame_len         = 205;   /* matches DETECT_SAMPLES_PER_BLOCK */
        init_config_.detect_threshold  = 13.0f;
        init_config_.detect_block      = 5;
        init_config_.detect_freq_min   = 650.0f;
        init_config_.detect_freq_max   = 3000.0f;
        init_config_.detect_freq_step  = 25.0f;
        init_config_.notch_persist_block = 5;
        init_config_.notch_filter_Q    = 0.7071f;
    }

    ~AudioEngineHs() {
        DeinitInternal();
    }

    void DeinitInternal() {
        if (state_inited_) {
            howling_free(&state_);
            memset(&state_, 0, sizeof(state_));
            state_inited_ = false;
        }
        initialized_ = false;
    }

    bool initialized_;
    bool state_inited_;
    HsInitConfig init_config_;
    howling_state_t state_;
};


/* ============================================================================
 * Create / Destroy
 * ==========================================================================*/
extern "C" HsHandle AudioEngine_Hs_Create(void)
{
    LOG_DEBUG("AudioEngine_Hs_Create");

    AudioEngineHs* ptr = new (std::nothrow) AudioEngineHs();
    if (!ptr) {
        LOG_ERROR("AudioEngine_Hs_Create: memory allocation failed");
    }
    return static_cast<HsHandle>(ptr);
}

extern "C" int AudioEngine_Hs_Destroy(HsHandle handle)
{
    LOG_DEBUG("AudioEngine_Hs_Destroy, handle=%p", (void*)handle);

    if (!handle) {
        LOG_WARN("AudioEngine_Hs_Destroy: invalid handle (NULL)");
        return AUDIO_ENGINE_ERR_INVALID_HANDLE;
    }

    AudioEngineHs* ptr = static_cast<AudioEngineHs*>(handle);
    delete ptr;

    return AUDIO_ENGINE_SUCCESS;
}


/* ============================================================================
 * Init / Deinit
 * ==========================================================================*/
extern "C" int AudioEngine_Hs_Init(HsHandle handle, const HsInitConfig* init_config)
{
    LOG_DEBUG("AudioEngine_Hs_Init, handle=%p", (void*)handle);

    if (!handle) {
        LOG_WARN("AudioEngine_Hs_Init: invalid handle (NULL)");
        return AUDIO_ENGINE_ERR_INVALID_HANDLE;
    }
    if (!init_config) {
        LOG_WARN("AudioEngine_Hs_Init: init_config is NULL");
        return AUDIO_ENGINE_ERR_NULL_POINTER;
    }
    if (init_config->sample_rate <= 0) {
        LOG_WARN("AudioEngine_Hs_Init: invalid sample_rate=%d", init_config->sample_rate);
        return AUDIO_ENGINE_ERR_INVALID_PARAM;
    }
    if (init_config->frame_len <= 0) {
        LOG_WARN("AudioEngine_Hs_Init: invalid frame_len=%d", init_config->frame_len);
        return AUDIO_ENGINE_ERR_INVALID_PARAM;
    }

    AudioEngineHs* ptr = static_cast<AudioEngineHs*>(handle);

    /* If already initialized, clean up first */
    if (ptr->initialized_) {
        LOG_DEBUG("AudioEngine_Hs_Init: already initialized, deinit first");
        ptr->DeinitInternal();
    }

    /* Save init config */
    ptr->init_config_ = *init_config;

    /* Build howling_suppression_desp from our config */
    howling_suppression_desp_t desp;
    desp.detect_Threshold     = init_config->detect_threshold;
    desp.detect_block         = init_config->detect_block;
    desp.detect_freq_min      = init_config->detect_freq_min;
    desp.detect_freq_max      = init_config->detect_freq_max;
    desp.detect_freq_interval = init_config->detect_freq_step;
    desp.notch_last_block     = init_config->notch_persist_block;
    desp.notch_filter_Q       = init_config->notch_filter_Q;

    /* Set global config (HS uses file-level globals — single-instance only) */
    howling_open(&desp);

    /* Init per-instance state */
    howling_init(&ptr->state_);
    ptr->state_inited_ = true;
    ptr->initialized_  = true;

    LOG_INFO("AudioEngine_Hs_Init OK, sample_rate=%d, frame_len=%d, "
             "freq=[%.0f, %.0f]/%.0f, threshold=%.2f",
             init_config->sample_rate, init_config->frame_len,
             init_config->detect_freq_min, init_config->detect_freq_max,
             init_config->detect_freq_step, init_config->detect_threshold);
    return AUDIO_ENGINE_SUCCESS;
}

extern "C" int AudioEngine_Hs_Deinit(HsHandle handle)
{
    LOG_DEBUG("AudioEngine_Hs_Deinit, handle=%p", (void*)handle);

    if (!handle) {
        LOG_WARN("AudioEngine_Hs_Deinit: invalid handle (NULL)");
        return AUDIO_ENGINE_ERR_INVALID_HANDLE;
    }

    AudioEngineHs* ptr = static_cast<AudioEngineHs*>(handle);
    ptr->DeinitInternal();

    LOG_DEBUG("AudioEngine_Hs_Deinit OK");
    return AUDIO_ENGINE_SUCCESS;
}


/* ============================================================================
 * Process
 * ==========================================================================*/
extern "C" int AudioEngine_Hs_Process(
    HsHandle handle,
    const short* audio_in,
    int in_samples,
    short* audio_out)
{
    if (!handle) {
        LOG_WARN("AudioEngine_Hs_Process: invalid handle (NULL)");
        return AUDIO_ENGINE_ERR_INVALID_HANDLE;
    }
    if (!audio_in) {
        LOG_WARN("AudioEngine_Hs_Process: audio_in is NULL");
        return AUDIO_ENGINE_ERR_NULL_POINTER;
    }
    if (!audio_out) {
        LOG_WARN("AudioEngine_Hs_Process: audio_out is NULL");
        return AUDIO_ENGINE_ERR_NULL_POINTER;
    }
    if (in_samples <= 0) {
        LOG_WARN("AudioEngine_Hs_Process: in_samples=%d must be > 0", in_samples);
        return AUDIO_ENGINE_ERR_INVALID_PARAM;
    }

    AudioEngineHs* ptr = static_cast<AudioEngineHs*>(handle);

    if (!ptr->initialized_) {
        LOG_WARN("AudioEngine_Hs_Process: not initialized");
        return AUDIO_ENGINE_ERR_NOT_INITIALIZED;
    }

    /* howling_suppression processes in-place via const short*, short*
       but its signature is (state, short*, len, short*) with non-const input.
       Cast away const — the function treats input as read-only in practice. */
    howling_suppression(&ptr->state_,
                        const_cast<short*>(audio_in),
                        in_samples,
                        audio_out);

    return AUDIO_ENGINE_SUCCESS;
}


/* ============================================================================
 * Reset
 * ==========================================================================*/
extern "C" int AudioEngine_Hs_Reset(HsHandle handle)
{
    LOG_DEBUG("AudioEngine_Hs_Reset, handle=%p", (void*)handle);

    if (!handle) {
        LOG_WARN("AudioEngine_Hs_Reset: invalid handle (NULL)");
        return AUDIO_ENGINE_ERR_INVALID_HANDLE;
    }

    AudioEngineHs* ptr = static_cast<AudioEngineHs*>(handle);

    if (!ptr->initialized_) {
        LOG_WARN("AudioEngine_Hs_Reset: not initialized");
        return AUDIO_ENGINE_ERR_NOT_INITIALIZED;
    }

    /* Free and re-init internal state; keep init config */
    if (ptr->state_inited_) {
        howling_free(&ptr->state_);
        memset(&ptr->state_, 0, sizeof(ptr->state_));
    }

    /* Re-apply global config then re-init */
    howling_suppression_desp_t desp;
    desp.detect_Threshold     = ptr->init_config_.detect_threshold;
    desp.detect_block         = ptr->init_config_.detect_block;
    desp.detect_freq_min      = ptr->init_config_.detect_freq_min;
    desp.detect_freq_max      = ptr->init_config_.detect_freq_max;
    desp.detect_freq_interval = ptr->init_config_.detect_freq_step;
    desp.notch_last_block     = ptr->init_config_.notch_persist_block;
    desp.notch_filter_Q       = ptr->init_config_.notch_filter_Q;

    howling_open(&desp);
    howling_init(&ptr->state_);
    ptr->state_inited_ = true;

    /* initialized_ stays true, configs preserved */
    LOG_INFO("AudioEngine_Hs_Reset OK, threshold=%.2f, freq=[%.0f, %.0f]",
             ptr->init_config_.detect_threshold,
             ptr->init_config_.detect_freq_min,
             ptr->init_config_.detect_freq_max);
    return AUDIO_ENGINE_SUCCESS;
}
