/*
 * @Author: Ryuk
 * @Date: 2026-08-09 23:30:00
 * @LastEditors: Ryuk
 * @LastEditTime: 2026-08-09 23:30:00
 * @Description: Legacy AGC interface implementation
 */

#include <new>

#include "audio_engine_def.h"
#include "audio_engine_agc.h"

#include "audio_processing/agc/gain_control.h"

using namespace webrtc;


/* ============================================================================
 * Defaults (match WebRTC AgcConfig defaults and the internal test)
 * ==========================================================================*/
#define AGC_DEFAULT_COMPRESSION_GAIN_DB (9)
#define AGC_DEFAULT_LIMITER_ENABLE      (1)
#define AGC_DEFAULT_TARGET_LEVEL_DBFS   (3)

/* Sentinel: field value -1 means "keep current" for SetParam/ResetParam */
#define AGC_PARAM_UNCHANGED (-1)


/* ============================================================================
 * Internal data container
 * ==========================================================================*/
class AudioEngineAgc {
public:
    AudioEngineAgc()
        : initialized_(false)
        , agc_(nullptr)
        , mic_level_(0)
    {
        rt_config_.compression_gain_db = AGC_DEFAULT_COMPRESSION_GAIN_DB;
        rt_config_.limiter_enable      = AGC_DEFAULT_LIMITER_ENABLE;
        rt_config_.target_level_dbfs   = AGC_DEFAULT_TARGET_LEVEL_DBFS;
    }

    ~AudioEngineAgc() {
        if (agc_) {
            WebRtcAgc_Free(agc_);
            agc_ = nullptr;
        }
    }

    bool initialized_;
    AgcInitConfig init_config_;
    AgcRtConfig rt_config_;
    void* agc_;
    int mic_level_;   /* inMicLevel/outMicLevel feedback loop state */
};


/* ============================================================================
 * Helpers
 * ==========================================================================*/
static webrtc::WebRtcAgcConfig build_agc_config(const AgcRtConfig* rt_config)
{
    webrtc::WebRtcAgcConfig cfg;
    cfg.targetLevelDbfs  = (int16_t)rt_config->target_level_dbfs;
    cfg.compressionGaindB = (int16_t)rt_config->compression_gain_db;
    cfg.limiterEnable    = (uint8_t)rt_config->limiter_enable;
    return cfg;
}

/* Validate a provided (non-sentinel) runtime parameter field */
static bool validate_rt_field(int field_id, int value)
{
    switch (field_id) {
    case 0:  /* compression_gain_db: 0..90 */
        return value >= 0 && value <= 90;
    case 1:  /* limiter_enable: 0 or 1 */
        return value == 0 || value == 1;
    case 2:  /* target_level_dbfs: 0..31 */
        return value >= 0 && value <= 31;
    default:
        return false;
    }
}


/* ============================================================================
 * Create / Destroy
 * ==========================================================================*/
AUDIO_ENGINE_API AgcHandle AudioEngine_Agc_Create(void)
{
    LOG_DEBUG("AudioEngine_Agc_Create");

    AudioEngineAgc* ptr = new (std::nothrow) AudioEngineAgc();
    if (!ptr) {
        LOG_ERROR("AudioEngine_Agc_Create: memory allocation failed");
    }
    return static_cast<AgcHandle>(ptr);
}

AUDIO_ENGINE_API int AudioEngine_Agc_Destroy(AgcHandle handle)
{
    LOG_DEBUG("AudioEngine_Agc_Destroy, handle=%p", (void*)handle);

    if (!handle) {
        LOG_WARN("AudioEngine_Agc_Destroy: invalid handle (NULL)");
        return AUDIO_ENGINE_ERR_INVALID_HANDLE;
    }

    AudioEngineAgc* ptr = static_cast<AudioEngineAgc*>(handle);
    delete ptr;

    return AUDIO_ENGINE_SUCCESS;
}


/* ============================================================================
 * Init / Deinit
 * ==========================================================================*/
AUDIO_ENGINE_API int AudioEngine_Agc_Init(AgcHandle handle, const AgcInitConfig* init_config)
{
    LOG_DEBUG("AudioEngine_Agc_Init, handle=%p", (void*)handle);

    if (!handle) {
        LOG_WARN("AudioEngine_Agc_Init: invalid handle (NULL)");
        return AUDIO_ENGINE_ERR_INVALID_HANDLE;
    }
    if (!init_config) {
        LOG_WARN("AudioEngine_Agc_Init: init_config is NULL");
        return AUDIO_ENGINE_ERR_NULL_POINTER;
    }
    if (init_config->sample_rate <= 0) {
        LOG_WARN("AudioEngine_Agc_Init: invalid sample_rate=%d", init_config->sample_rate);
        return AUDIO_ENGINE_ERR_INVALID_PARAM;
    }
    if (init_config->agc_mode < 0 || init_config->agc_mode > 3) {
        LOG_WARN("AudioEngine_Agc_Init: invalid agc_mode=%d (0-3)", init_config->agc_mode);
        return AUDIO_ENGINE_ERR_INVALID_PARAM;
    }
    if (init_config->min_level < 0) {
        LOG_WARN("AudioEngine_Agc_Init: invalid min_level=%d (must be >= 0)",
                 init_config->min_level);
        return AUDIO_ENGINE_ERR_INVALID_PARAM;
    }
    if (init_config->max_level < init_config->min_level) {
        LOG_WARN("AudioEngine_Agc_Init: invalid max_level=%d (must be >= min_level=%d)",
                 init_config->max_level, init_config->min_level);
        return AUDIO_ENGINE_ERR_INVALID_PARAM;
    }

    AudioEngineAgc* ptr = static_cast<AudioEngineAgc*>(handle);

    /* If already initialized, clean up first */
    if (ptr->initialized_) {
        LOG_DEBUG("AudioEngine_Agc_Init: already initialized, deinit first");
        if (ptr->agc_) {
            WebRtcAgc_Free(ptr->agc_);
            ptr->agc_ = nullptr;
        }
    }

    ptr->init_config_ = *init_config;

    ptr->agc_ = WebRtcAgc_Create();
    if (!ptr->agc_) {
        LOG_ERROR("AudioEngine_Agc_Init: WebRtcAgc_Create failed");
        return AUDIO_ENGINE_ERR_INIT_FAILED;
    }

    int ret = WebRtcAgc_Init(ptr->agc_,
                             init_config->min_level,
                             init_config->max_level,
                             (int16_t)init_config->agc_mode,
                             (uint32_t)init_config->sample_rate);
    if (ret != 0) {
        LOG_ERROR("AudioEngine_Agc_Init: WebRtcAgc_Init failed, ret=%d", ret);
        WebRtcAgc_Free(ptr->agc_);
        ptr->agc_ = nullptr;
        return AUDIO_ENGINE_ERR_INIT_FAILED;
    }

    /* Apply default rt_config (already set by constructor) */
    webrtc::WebRtcAgcConfig cfg = build_agc_config(&ptr->rt_config_);
    ret = WebRtcAgc_set_config(ptr->agc_, cfg);
    if (ret != 0) {
        LOG_ERROR("AudioEngine_Agc_Init: WebRtcAgc_set_config failed, ret=%d", ret);
        WebRtcAgc_Free(ptr->agc_);
        ptr->agc_ = nullptr;
        return AUDIO_ENGINE_ERR_INIT_FAILED;
    }

    ptr->mic_level_ = 0;
    ptr->initialized_ = true;

    LOG_INFO("AudioEngine_Agc_Init OK, rate=%d mode=%d min=%d max=%d",
             init_config->sample_rate, init_config->agc_mode,
             init_config->min_level, init_config->max_level);
    return AUDIO_ENGINE_SUCCESS;
}

AUDIO_ENGINE_API int AudioEngine_Agc_Deinit(AgcHandle handle)
{
    LOG_DEBUG("AudioEngine_Agc_Deinit, handle=%p", (void*)handle);

    if (!handle) {
        LOG_WARN("AudioEngine_Agc_Deinit: invalid handle (NULL)");
        return AUDIO_ENGINE_ERR_INVALID_HANDLE;
    }

    AudioEngineAgc* ptr = static_cast<AudioEngineAgc*>(handle);

    if (ptr->agc_) {
        WebRtcAgc_Free(ptr->agc_);
        ptr->agc_ = nullptr;
    }
    ptr->mic_level_ = 0;
    ptr->initialized_ = false;

    LOG_DEBUG("AudioEngine_Agc_Deinit OK");
    return AUDIO_ENGINE_SUCCESS;
}


/* ============================================================================
 * SetParam / ResetParam
 * ==========================================================================*/
AUDIO_ENGINE_API int AudioEngine_Agc_SetParam(AgcHandle handle, const AgcRtConfig* rt_config)
{
    LOG_DEBUG("AudioEngine_Agc_SetParam, handle=%p", (void*)handle);

    if (!handle) {
        LOG_WARN("AudioEngine_Agc_SetParam: invalid handle (NULL)");
        return AUDIO_ENGINE_ERR_INVALID_HANDLE;
    }
    if (!rt_config) {
        LOG_WARN("AudioEngine_Agc_SetParam: rt_config is NULL");
        return AUDIO_ENGINE_ERR_NULL_POINTER;
    }

    AudioEngineAgc* ptr = static_cast<AudioEngineAgc*>(handle);

    if (!ptr->initialized_) {
        LOG_WARN("AudioEngine_Agc_SetParam: not initialized");
        return AUDIO_ENGINE_ERR_NOT_INITIALIZED;
    }

    /* Validate provided fields (sentinel -1 = keep current) */
    const int fields[] = {
        rt_config->compression_gain_db,
        rt_config->limiter_enable,
        rt_config->target_level_dbfs,
    };
    for (int i = 0; i < 3; i++) {
        if (fields[i] != AGC_PARAM_UNCHANGED && !validate_rt_field(i, fields[i])) {
            LOG_WARN("AudioEngine_Agc_SetParam: invalid field[%d]=%d", i, fields[i]);
            return AUDIO_ENGINE_ERR_INVALID_PARAM;
        }
    }

    /* Incremental merge: only overwrite provided fields */
    if (rt_config->compression_gain_db != AGC_PARAM_UNCHANGED)
        ptr->rt_config_.compression_gain_db = rt_config->compression_gain_db;
    if (rt_config->limiter_enable != AGC_PARAM_UNCHANGED)
        ptr->rt_config_.limiter_enable = rt_config->limiter_enable;
    if (rt_config->target_level_dbfs != AGC_PARAM_UNCHANGED)
        ptr->rt_config_.target_level_dbfs = rt_config->target_level_dbfs;

    webrtc::WebRtcAgcConfig cfg = build_agc_config(&ptr->rt_config_);
    int ret = WebRtcAgc_set_config(ptr->agc_, cfg);
    if (ret != 0) {
        LOG_ERROR("AudioEngine_Agc_SetParam: WebRtcAgc_set_config failed, ret=%d", ret);
        return AUDIO_ENGINE_ERR_SET_PARAM_FAILED;
    }

    LOG_INFO("AudioEngine_Agc_SetParam OK, gain=%d limiter=%d target=%d",
             ptr->rt_config_.compression_gain_db, ptr->rt_config_.limiter_enable,
             ptr->rt_config_.target_level_dbfs);
    return AUDIO_ENGINE_SUCCESS;
}

AUDIO_ENGINE_API int AudioEngine_Agc_ResetParam(AgcHandle handle, const AgcRtConfig* rt_config)
{
    LOG_DEBUG("AudioEngine_Agc_ResetParam, handle=%p", (void*)handle);

    if (!handle) {
        LOG_WARN("AudioEngine_Agc_ResetParam: invalid handle (NULL)");
        return AUDIO_ENGINE_ERR_INVALID_HANDLE;
    }
    if (!rt_config) {
        LOG_WARN("AudioEngine_Agc_ResetParam: rt_config is NULL");
        return AUDIO_ENGINE_ERR_NULL_POINTER;
    }

    AudioEngineAgc* ptr = static_cast<AudioEngineAgc*>(handle);

    if (!ptr->initialized_) {
        LOG_WARN("AudioEngine_Agc_ResetParam: not initialized");
        return AUDIO_ENGINE_ERR_NOT_INITIALIZED;
    }

    /* Validate provided fields first */
    const int fields[] = {
        rt_config->compression_gain_db,
        rt_config->limiter_enable,
        rt_config->target_level_dbfs,
    };
    for (int i = 0; i < 3; i++) {
        if (fields[i] != AGC_PARAM_UNCHANGED && !validate_rt_field(i, fields[i])) {
            LOG_WARN("AudioEngine_Agc_ResetParam: invalid field[%d]=%d", i, fields[i]);
            return AUDIO_ENGINE_ERR_INVALID_PARAM;
        }
    }

    /* Full reset: back to defaults, then apply provided fields */
    ptr->rt_config_.compression_gain_db = AGC_DEFAULT_COMPRESSION_GAIN_DB;
    ptr->rt_config_.limiter_enable      = AGC_DEFAULT_LIMITER_ENABLE;
    ptr->rt_config_.target_level_dbfs   = AGC_DEFAULT_TARGET_LEVEL_DBFS;

    if (rt_config->compression_gain_db != AGC_PARAM_UNCHANGED)
        ptr->rt_config_.compression_gain_db = rt_config->compression_gain_db;
    if (rt_config->limiter_enable != AGC_PARAM_UNCHANGED)
        ptr->rt_config_.limiter_enable = rt_config->limiter_enable;
    if (rt_config->target_level_dbfs != AGC_PARAM_UNCHANGED)
        ptr->rt_config_.target_level_dbfs = rt_config->target_level_dbfs;

    webrtc::WebRtcAgcConfig cfg = build_agc_config(&ptr->rt_config_);
    int ret = WebRtcAgc_set_config(ptr->agc_, cfg);
    if (ret != 0) {
        LOG_ERROR("AudioEngine_Agc_ResetParam: WebRtcAgc_set_config failed, ret=%d", ret);
        return AUDIO_ENGINE_ERR_SET_PARAM_FAILED;
    }

    LOG_INFO("AudioEngine_Agc_ResetParam OK, gain=%d limiter=%d target=%d",
             ptr->rt_config_.compression_gain_db, ptr->rt_config_.limiter_enable,
             ptr->rt_config_.target_level_dbfs);
    return AUDIO_ENGINE_SUCCESS;
}


/* ============================================================================
 * Process
 * ==========================================================================*/
AUDIO_ENGINE_API int AudioEngine_Agc_Process(
    AgcHandle handle,
    const short* audio_in,
    int in_samples,
    short* audio_out,
    int max_out_samples,
    int* out_samples)
{
    if (!handle) {
        LOG_WARN("AudioEngine_Agc_Process: invalid handle (NULL)");
        return AUDIO_ENGINE_ERR_INVALID_HANDLE;
    }
    if (!audio_in) {
        LOG_WARN("AudioEngine_Agc_Process: audio_in is NULL");
        return AUDIO_ENGINE_ERR_NULL_POINTER;
    }
    if (!audio_out) {
        LOG_WARN("AudioEngine_Agc_Process: audio_out is NULL");
        return AUDIO_ENGINE_ERR_NULL_POINTER;
    }
    if (!out_samples) {
        LOG_WARN("AudioEngine_Agc_Process: out_samples is NULL");
        return AUDIO_ENGINE_ERR_NULL_POINTER;
    }

    AudioEngineAgc* ptr = static_cast<AudioEngineAgc*>(handle);

    if (!ptr->initialized_) {
        LOG_WARN("AudioEngine_Agc_Process: not initialized");
        return AUDIO_ENGINE_ERR_NOT_INITIALIZED;
    }

    /* Fixed 10ms frame: in_samples must equal sample_rate / 100 */
    const int frame_len = ptr->init_config_.sample_rate / 100;
    if (in_samples != frame_len) {
        LOG_WARN("AudioEngine_Agc_Process: in_samples=%d must be %d (10ms)",
                 in_samples, frame_len);
        return AUDIO_ENGINE_ERR_INVALID_PARAM;
    }
    if (max_out_samples < in_samples) {
        LOG_WARN("AudioEngine_Agc_Process: max_out_samples=%d too small, need %d",
                 max_out_samples, in_samples);
        return AUDIO_ENGINE_ERR_INVALID_PARAM;
    }

    const int16_t* bands[] = { audio_in };
    int16_t* out_bands[]   = { audio_out };
    int16_t echo = 0;
    uint8_t saturation_warning = 1;
    int32_t gains[11] = {0};
    int32_t out_mic_level = 0;

    int ret = WebRtcAgc_Analyze(ptr->agc_, bands, 1, (size_t)in_samples,
                                ptr->mic_level_, &out_mic_level, echo,
                                &saturation_warning, gains);
    if (ret != 0) {
        LOG_ERROR("AudioEngine_Agc_Process: WebRtcAgc_Analyze failed, ret=%d", ret);
        return AUDIO_ENGINE_ERR_PROCESS_FAILED;
    }
    /* Feedback loop: feed output mic level back as input next frame */
    ptr->mic_level_ = out_mic_level;

    ret = WebRtcAgc_Process(ptr->agc_, gains, bands, 1, out_bands);
    if (ret != 0) {
        LOG_ERROR("AudioEngine_Agc_Process: WebRtcAgc_Process failed, ret=%d", ret);
        return AUDIO_ENGINE_ERR_PROCESS_FAILED;
    }

    *out_samples = in_samples;
    return AUDIO_ENGINE_SUCCESS;
}


/* ============================================================================
 * Reset
 * ==========================================================================*/
AUDIO_ENGINE_API int AudioEngine_Agc_Reset(AgcHandle handle)
{
    LOG_DEBUG("AudioEngine_Agc_Reset, handle=%p", (void*)handle);

    if (!handle) {
        LOG_WARN("AudioEngine_Agc_Reset: invalid handle (NULL)");
        return AUDIO_ENGINE_ERR_INVALID_HANDLE;
    }

    AudioEngineAgc* ptr = static_cast<AudioEngineAgc*>(handle);

    if (!ptr->initialized_) {
        LOG_WARN("AudioEngine_Agc_Reset: not initialized");
        return AUDIO_ENGINE_ERR_NOT_INITIALIZED;
    }

    /* Re-init internal state with preserved config, reset mic level */
    int ret = WebRtcAgc_Init(ptr->agc_,
                             ptr->init_config_.min_level,
                             ptr->init_config_.max_level,
                             (int16_t)ptr->init_config_.agc_mode,
                             (uint32_t)ptr->init_config_.sample_rate);
    if (ret != 0) {
        LOG_ERROR("AudioEngine_Agc_Reset: WebRtcAgc_Init failed, ret=%d", ret);
        return AUDIO_ENGINE_ERR_PROCESS_FAILED;
    }

    webrtc::WebRtcAgcConfig cfg = build_agc_config(&ptr->rt_config_);
    ret = WebRtcAgc_set_config(ptr->agc_, cfg);
    if (ret != 0) {
        LOG_ERROR("AudioEngine_Agc_Reset: WebRtcAgc_set_config failed, ret=%d", ret);
        return AUDIO_ENGINE_ERR_PROCESS_FAILED;
    }

    ptr->mic_level_ = 0;

    /* initialized_ stays true, config preserved */
    LOG_INFO("AudioEngine_Agc_Reset OK");
    return AUDIO_ENGINE_SUCCESS;
}
