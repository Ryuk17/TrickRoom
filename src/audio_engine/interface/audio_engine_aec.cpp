/*
 * @Author: Ryuk
 * @Date: 2026-08-10 01:30:00
 * @LastEditors: Ryuk
 * @LastEditTime: 2026-08-10 01:30:00
 * @Description: AEC3 (EchoCanceller3) unified C interface implementation
 */

#include "audio_engine_aec.h"

#include <new>
#include <optional>

#include "utils/stream_config.h"
#include "utils/audio_buffer.h"
#include "audio_processing/aec3/echo_canceller3.h"
#include "audio_processing/aec3/echo_canceller3_config.h"

using namespace webrtc;

/* ============================================================================
 * AEC3 data container (pure data, C API operates directly on it)
 * ==========================================================================*/
class AudioEngineAec {
public:
    AudioEngineAec()
        : initialized_(false),
          frame_size_(0),
          sample_rate_(0),
          aec3_(nullptr),
          render_buffer_(nullptr),
          capture_buffer_(nullptr) {
        init_config_.sample_rate = 0;
        init_config_.num_render_channels = 1;
        init_config_.num_capture_channels = 1;
        rt_config_.delay_ms = -1;
    }

    ~AudioEngineAec() {
        DeinitInternal();
    }

    void DeinitInternal() {
        if (aec3_) {
            delete aec3_;
            aec3_ = nullptr;
        }
        if (render_buffer_) {
            delete render_buffer_;
            render_buffer_ = nullptr;
        }
        if (capture_buffer_) {
            delete capture_buffer_;
            capture_buffer_ = nullptr;
        }
        initialized_ = false;
    }

    bool initialized_;
    int frame_size_;                 /* samples per 10ms frame (all channels) */
    int sample_rate_;
    AecInitConfig init_config_;
    AecRtConfig rt_config_;

    EchoCanceller3* aec3_;
    AudioBuffer* render_buffer_;
    AudioBuffer* capture_buffer_;
    StreamConfig stream_config_;
};

/* ============================================================================
 * Create / Destroy
 * ==========================================================================*/
extern "C" AecHandle AudioEngine_Aec_Create(void)
{
    LOG_DEBUG("AudioEngine_Aec_Create()");
    AudioEngineAec* ctx = new (std::nothrow) AudioEngineAec();
    if (!ctx) {
        LOG_ERROR("AudioEngine_Aec_Create: out of memory");
        return NULL;
    }
    return ctx;
}

extern "C" int AudioEngine_Aec_Destroy(AecHandle handle)
{
    LOG_DEBUG("AudioEngine_Aec_Destroy(handle=%p)", handle);
    if (!handle) {
        LOG_WARN("AudioEngine_Aec_Destroy: NULL handle");
        return AUDIO_ENGINE_ERR_INVALID_HANDLE;
    }
    delete static_cast<AudioEngineAec*>(handle);
    return AUDIO_ENGINE_SUCCESS;
}

/* ============================================================================
 * Init
 * ==========================================================================*/
extern "C" int AudioEngine_Aec_Init(AecHandle handle, const AecInitConfig* init_config)
{
    LOG_DEBUG("AudioEngine_Aec_Init(handle=%p, config=%p)", handle, init_config);
    if (!handle) {
        LOG_WARN("AudioEngine_Aec_Init: NULL handle");
        return AUDIO_ENGINE_ERR_INVALID_HANDLE;
    }
    AudioEngineAec* ctx = static_cast<AudioEngineAec*>(handle);
    if (!init_config) {
        LOG_WARN("AudioEngine_Aec_Init: NULL init_config");
        return AUDIO_ENGINE_ERR_NULL_POINTER;
    }

    /* Validate parameters (EchoCanceller3 supports 16k/32k/48k full-band) */
    if (init_config->sample_rate != 16000 &&
        init_config->sample_rate != 32000 &&
        init_config->sample_rate != 48000) {
        LOG_ERROR("AudioEngine_Aec_Init: invalid sample_rate=%d", init_config->sample_rate);
        return AUDIO_ENGINE_ERR_INVALID_PARAM;
    }
    if (init_config->num_render_channels < 1 || init_config->num_render_channels > 2) {
        LOG_ERROR("AudioEngine_Aec_Init: invalid num_render_channels=%d",
                  init_config->num_render_channels);
        return AUDIO_ENGINE_ERR_INVALID_PARAM;
    }
    if (init_config->num_capture_channels < 1 || init_config->num_capture_channels > 2) {
        LOG_ERROR("AudioEngine_Aec_Init: invalid num_capture_channels=%d",
                  init_config->num_capture_channels);
        return AUDIO_ENGINE_ERR_INVALID_PARAM;
    }

    /* Deinit if already initialized (re-init allowed) */
    ctx->DeinitInternal();

    /* Save init config */
    ctx->init_config_ = *init_config;
    ctx->sample_rate_ = init_config->sample_rate;

    /* Default config, matching the internal test */
    EchoCanceller3Config config;

    /* Construct EchoCanceller3 directly (no factory, same as internal test:
       default config, no multichannel config, no neural residual echo estimator) */
    ctx->aec3_ = new (std::nothrow) EchoCanceller3(
        config,
        std::nullopt,
        /*neural_residual_echo_estimator=*/nullptr,
        init_config->sample_rate,
        static_cast<size_t>(init_config->num_render_channels),
        static_cast<size_t>(init_config->num_capture_channels));
    if (!ctx->aec3_) {
        LOG_ERROR("AudioEngine_Aec_Init: failed to create EchoCanceller3");
        ctx->DeinitInternal();
        return AUDIO_ENGINE_ERR_INIT_FAILED;
    }

    /* AudioBuffer pipeline (render + capture), same shape as internal test */
    int rate = init_config->sample_rate;
    size_t rch = static_cast<size_t>(init_config->num_render_channels);
    size_t cch = static_cast<size_t>(init_config->num_capture_channels);
    ctx->render_buffer_ = new (std::nothrow) AudioBuffer(rate, rch, rate, rch, rate, rch);
    if (!ctx->render_buffer_) {
        LOG_ERROR("AudioEngine_Aec_Init: failed to create render AudioBuffer");
        ctx->DeinitInternal();
        return AUDIO_ENGINE_ERR_INIT_FAILED;
    }
    ctx->capture_buffer_ = new (std::nothrow) AudioBuffer(rate, cch, rate, cch, rate, cch);
    if (!ctx->capture_buffer_) {
        LOG_ERROR("AudioEngine_Aec_Init: failed to create capture AudioBuffer");
        ctx->DeinitInternal();
        return AUDIO_ENGINE_ERR_INIT_FAILED;
    }

    ctx->stream_config_ = StreamConfig(rate, cch);
    ctx->frame_size_ = (rate / 100) * init_config->num_capture_channels;
    ctx->rt_config_.delay_ms = -1;   /* default: no external delay */
    ctx->initialized_ = true;

    LOG_DEBUG("AudioEngine_Aec_Init OK: rate=%d render_ch=%d capture_ch=%d frame_size=%d",
              rate, init_config->num_render_channels,
              init_config->num_capture_channels, ctx->frame_size_);
    return AUDIO_ENGINE_SUCCESS;
}

/* ============================================================================
 * SetParam (incremental) / ResetParam (full reset)
 * ==========================================================================*/
extern "C" int AudioEngine_Aec_SetParam(AecHandle handle, const AecRtConfig* rt_config)
{
    LOG_DEBUG("AudioEngine_Aec_SetParam(handle=%p, config=%p)", handle, rt_config);
    if (!handle) {
        LOG_WARN("AudioEngine_Aec_SetParam: NULL handle");
        return AUDIO_ENGINE_ERR_INVALID_HANDLE;
    }
    AudioEngineAec* ctx = static_cast<AudioEngineAec*>(handle);
    if (!ctx->initialized_) {
        LOG_WARN("AudioEngine_Aec_SetParam: not initialized");
        return AUDIO_ENGINE_ERR_NOT_INITIALIZED;
    }
    if (!rt_config) {
        LOG_WARN("AudioEngine_Aec_SetParam: NULL rt_config");
        return AUDIO_ENGINE_ERR_NULL_POINTER;
    }
    /* Incremental: only apply non-sentinel fields */
    if (rt_config->delay_ms >= 0) {
        ctx->aec3_->SetAudioBufferDelay(rt_config->delay_ms);
    }
    ctx->rt_config_.delay_ms = rt_config->delay_ms;
    return AUDIO_ENGINE_SUCCESS;
}

extern "C" int AudioEngine_Aec_ResetParam(AecHandle handle, const AecRtConfig* rt_config)
{
    LOG_DEBUG("AudioEngine_Aec_ResetParam(handle=%p, config=%p)", handle, rt_config);
    if (!handle) {
        LOG_WARN("AudioEngine_Aec_ResetParam: NULL handle");
        return AUDIO_ENGINE_ERR_INVALID_HANDLE;
    }
    AudioEngineAec* ctx = static_cast<AudioEngineAec*>(handle);
    if (!ctx->initialized_) {
        LOG_WARN("AudioEngine_Aec_ResetParam: not initialized");
        return AUDIO_ENGINE_ERR_NOT_INITIALIZED;
    }
    if (!rt_config) {
        LOG_WARN("AudioEngine_Aec_ResetParam: NULL rt_config");
        return AUDIO_ENGINE_ERR_NULL_POINTER;
    }
    /* Full reset: back to default, then merge caller fields */
    ctx->rt_config_.delay_ms = -1;
    if (rt_config->delay_ms >= 0) {
        ctx->aec3_->SetAudioBufferDelay(rt_config->delay_ms);
        ctx->rt_config_.delay_ms = rt_config->delay_ms;
    }
    return AUDIO_ENGINE_SUCCESS;
}

/* ============================================================================
 * Process — dual input: near-end (mic) + far-end (render)
 * ==========================================================================*/
extern "C" int AudioEngine_Aec_Process(
    AecHandle handle,
    const short* nearend_in,
    const short* farend_in,
    int in_samples,
    short* audio_out,
    int max_out_samples,
    int* out_samples)
{
    LOG_DEBUG("AudioEngine_Aec_Process(handle=%p, nearend=%p, farend=%p, in_samples=%d, out=%p, max_out=%d, out_samples=%p)",
              handle, nearend_in, farend_in, in_samples, audio_out, max_out_samples, out_samples);
    if (!handle) {
        LOG_WARN("AudioEngine_Aec_Process: NULL handle");
        return AUDIO_ENGINE_ERR_INVALID_HANDLE;
    }
    AudioEngineAec* ctx = static_cast<AudioEngineAec*>(handle);
    if (!ctx->initialized_) {
        LOG_WARN("AudioEngine_Aec_Process: not initialized");
        return AUDIO_ENGINE_ERR_NOT_INITIALIZED;
    }
    if (!nearend_in || !farend_in || !audio_out || !out_samples) {
        LOG_WARN("AudioEngine_Aec_Process: NULL pointer");
        return AUDIO_ENGINE_ERR_NULL_POINTER;
    }
    if (in_samples != ctx->frame_size_) {
        /* Strict 10ms frame: caller must zero-pad partial final frames */
        LOG_WARN("AudioEngine_Aec_Process: invalid in_samples=%d (frame_size=%d)",
                 in_samples, ctx->frame_size_);
        return AUDIO_ENGINE_ERR_INVALID_PARAM;
    }
    if (max_out_samples < ctx->frame_size_) {
        LOG_WARN("AudioEngine_Aec_Process: max_out_samples=%d < frame_size=%d",
                 max_out_samples, ctx->frame_size_);
        return AUDIO_ENGINE_ERR_INVALID_PARAM;
    }

    /* Same per-frame call order as the internal test:
       1. AnalyzeCapture (detect saturation)
       2. Copy farend into render buffer, nearend into capture buffer
       3. Split into frequency bands when rate > 16kHz
       4. AnalyzeRender (store render)
       5. ProcessCapture (cancel echo, in-place on capture buffer)
       6. Copy out to int16 */
    ctx->aec3_->AnalyzeCapture(ctx->capture_buffer_);

    ctx->render_buffer_->CopyFrom(farend_in, ctx->stream_config_);
    ctx->capture_buffer_->CopyFrom(nearend_in, ctx->stream_config_);
    if (ctx->sample_rate_ > 16000) {
        ctx->render_buffer_->SplitIntoFrequencyBands();
        ctx->capture_buffer_->SplitIntoFrequencyBands();
    }

    ctx->aec3_->AnalyzeRender(ctx->render_buffer_);
    ctx->aec3_->ProcessCapture(ctx->capture_buffer_, /*level_change=*/false);

    ctx->capture_buffer_->CopyTo(ctx->stream_config_, audio_out);
    *out_samples = in_samples;

    return AUDIO_ENGINE_SUCCESS;
}

/* ============================================================================
 * Deinit / Reset
 * ==========================================================================*/
extern "C" int AudioEngine_Aec_Deinit(AecHandle handle)
{
    LOG_DEBUG("AudioEngine_Aec_Deinit(handle=%p)", handle);
    if (!handle) {
        LOG_WARN("AudioEngine_Aec_Deinit: NULL handle");
        return AUDIO_ENGINE_ERR_INVALID_HANDLE;
    }
    AudioEngineAec* ctx = static_cast<AudioEngineAec*>(handle);
    ctx->DeinitInternal();
    return AUDIO_ENGINE_SUCCESS;
}

extern "C" int AudioEngine_Aec_Reset(AecHandle handle)
{
    LOG_DEBUG("AudioEngine_Aec_Reset(handle=%p)", handle);
    if (!handle) {
        LOG_WARN("AudioEngine_Aec_Reset: NULL handle");
        return AUDIO_ENGINE_ERR_INVALID_HANDLE;
    }
    AudioEngineAec* ctx = static_cast<AudioEngineAec*>(handle);
    if (!ctx->initialized_) {
        LOG_WARN("AudioEngine_Aec_Reset: not initialized");
        return AUDIO_ENGINE_ERR_NOT_INITIALIZED;
    }

    /* Rebuild instance, preserving init config */
    AecInitConfig cfg = ctx->init_config_;
    ctx->DeinitInternal();
    return AudioEngine_Aec_Init(handle, &cfg);
}
