/*
 * @Author: Ryuk
 * @Date: 2026-08-09 23:50:00
 * @LastEditors: Ryuk
 * @LastEditTime: 2026-08-09 23:50:00
 * @Description: AGC2 unified C interface implementation
 */

#include "audio_engine_agc2.h"
#include "audio_engine_log.h"

#include <cmath>
#include <memory>
#include <new>

#include "utils/audio_util.h"
#include "utils/stream_config.h"
#include "utils/audio_buffer.h"
#include "utils/apm_data_dumper.h"
#include "audio_processing/automatic_gain_control2/adaptive_digital_gain_controller.h"
#include "audio_processing/automatic_gain_control2/speech_level_estimator.h"
#include "audio_processing/automatic_gain_control2/noise_level_estimator.h"
#include "audio_processing/automatic_gain_control2/vad_wrapper.h"
#include "audio_processing/automatic_gain_control2/cpu_features.h"
#include "audio_processing/automatic_gain_control2/agc2_common.h"

using namespace webrtc;

/* ============================================================================
 * AGC2 data container (pure data, C API operates directly on it)
 * ==========================================================================*/
class AudioEngineAgc2 {
public:
    AudioEngineAgc2()
        : initialized_(false),
          frame_size_(0),
          apm_data_dumper_(nullptr),
          gain_controller_(nullptr),
          speech_level_estimator_(nullptr),
          noise_level_estimator_(nullptr),
          vad_(nullptr),
          audio_buffer_(nullptr),
          stream_config_(0, 0) {
        init_config_.sample_rate = 0;
        init_config_.num_channels = 0;
        init_config_.headroom_db = 5.0f;
        init_config_.max_gain_db = 50.0f;
        init_config_.initial_gain_db = 15.0f;
        init_config_.max_gain_change_db_per_second = 6.0f;
        init_config_.max_output_noise_level_dbfs = -50.0f;
        rt_config_.reserved = 0;
    }

    ~AudioEngineAgc2() {
        DeinitInternal();
    }

    void DeinitInternal() {
        if (vad_) {
            delete vad_;
            vad_ = nullptr;
        }
        if (speech_level_estimator_) {
            delete speech_level_estimator_;
            speech_level_estimator_ = nullptr;
        }
        if (gain_controller_) {
            delete gain_controller_;
            gain_controller_ = nullptr;
        }
        noise_level_estimator_.reset();
        if (audio_buffer_) {
            delete audio_buffer_;
            audio_buffer_ = nullptr;
        }
        if (apm_data_dumper_) {
            delete apm_data_dumper_;
            apm_data_dumper_ = nullptr;
        }
        initialized_ = false;
    }

    bool initialized_;
    int frame_size_;                 /* samples per 10ms frame (all channels) */
    Agc2InitConfig init_config_;
    Agc2RtConfig rt_config_;

    ApmDataDumper* apm_data_dumper_;
    AdaptiveDigitalGainController* gain_controller_;
    SpeechLevelEstimator* speech_level_estimator_;
    std::unique_ptr<NoiseLevelEstimator> noise_level_estimator_;
    VoiceActivityDetectorWrapper* vad_;
    AudioBuffer* audio_buffer_;
    StreamConfig stream_config_;
};

/* ============================================================================
 * Create / Destroy
 * ==========================================================================*/
extern "C" Agc2Handle AudioEngine_Agc2_Create(void)
{
    LOG_DEBUG("AudioEngine_Agc2_Create()");
    AudioEngineAgc2* ctx = new (std::nothrow) AudioEngineAgc2();
    if (!ctx) {
        LOG_ERROR("AudioEngine_Agc2_Create: out of memory");
        return NULL;
    }
    return ctx;
}

extern "C" int AudioEngine_Agc2_Destroy(Agc2Handle handle)
{
    LOG_DEBUG("AudioEngine_Agc2_Destroy(handle=%p)", handle);
    if (!handle) {
        LOG_WARN("AudioEngine_Agc2_Destroy: NULL handle");
        return AUDIO_ENGINE_ERR_INVALID_HANDLE;
    }
    delete static_cast<AudioEngineAgc2*>(handle);
    return AUDIO_ENGINE_SUCCESS;
}

/* ============================================================================
 * Init
 * ==========================================================================*/
extern "C" int AudioEngine_Agc2_Init(Agc2Handle handle, const Agc2InitConfig* init_config)
{
    LOG_DEBUG("AudioEngine_Agc2_Init(handle=%p, config=%p)", handle, init_config);
    if (!handle) {
        LOG_WARN("AudioEngine_Agc2_Init: NULL handle");
        return AUDIO_ENGINE_ERR_INVALID_HANDLE;
    }
    AudioEngineAgc2* ctx = static_cast<AudioEngineAgc2*>(handle);
    if (!init_config) {
        LOG_WARN("AudioEngine_Agc2_Init: NULL init_config");
        return AUDIO_ENGINE_ERR_NULL_POINTER;
    }

    /* Validate parameters */
    if (init_config->sample_rate != 8000 &&
        init_config->sample_rate != 16000 &&
        init_config->sample_rate != 32000 &&
        init_config->sample_rate != 48000) {
        LOG_ERROR("AudioEngine_Agc2_Init: invalid sample_rate=%d", init_config->sample_rate);
        return AUDIO_ENGINE_ERR_INVALID_PARAM;
    }
    if (init_config->num_channels < 1 || init_config->num_channels > 2) {
        LOG_ERROR("AudioEngine_Agc2_Init: invalid num_channels=%d", init_config->num_channels);
        return AUDIO_ENGINE_ERR_INVALID_PARAM;
    }

    /* Deinit if already initialized (re-init allowed) */
    ctx->DeinitInternal();

    /* Save init config */
    ctx->init_config_ = *init_config;

    /* Build the AdaptiveDigital config (defaults preserved when caller passes 0) */
    AudioProcessing::Config::GainController2::AdaptiveDigital config;
    config.headroom_db = init_config->headroom_db;
    config.max_gain_db = init_config->max_gain_db;
    config.initial_gain_db = init_config->initial_gain_db;
    config.max_gain_change_db_per_second = init_config->max_gain_change_db_per_second;
    config.max_output_noise_level_dbfs = init_config->max_output_noise_level_dbfs;

    /* Create the pipeline components */
    ctx->apm_data_dumper_ = new (std::nothrow) ApmDataDumper(0);
    if (!ctx->apm_data_dumper_) {
        LOG_ERROR("AudioEngine_Agc2_Init: failed to create ApmDataDumper");
        ctx->DeinitInternal();
        return AUDIO_ENGINE_ERR_INIT_FAILED;
    }

    ctx->gain_controller_ = new (std::nothrow) AdaptiveDigitalGainController(
        ctx->apm_data_dumper_, config, kAdjacentSpeechFramesThreshold);
    if (!ctx->gain_controller_) {
        LOG_ERROR("AudioEngine_Agc2_Init: failed to create AdaptiveDigitalGainController");
        ctx->DeinitInternal();
        return AUDIO_ENGINE_ERR_INIT_FAILED;
    }

    ctx->speech_level_estimator_ = new (std::nothrow) SpeechLevelEstimator(
        ctx->apm_data_dumper_, config, kAdjacentSpeechFramesThreshold);
    if (!ctx->speech_level_estimator_) {
        LOG_ERROR("AudioEngine_Agc2_Init: failed to create SpeechLevelEstimator");
        ctx->DeinitInternal();
        return AUDIO_ENGINE_ERR_INIT_FAILED;
    }

    ctx->noise_level_estimator_ = CreateNoiseFloorEstimator(ctx->apm_data_dumper_);
    if (!ctx->noise_level_estimator_) {
        LOG_ERROR("AudioEngine_Agc2_Init: failed to create NoiseLevelEstimator");
        ctx->DeinitInternal();
        return AUDIO_ENGINE_ERR_INIT_FAILED;
    }

    ctx->vad_ = new (std::nothrow) VoiceActivityDetectorWrapper(
        NoAvailableCpuFeatures(), init_config->sample_rate);
    if (!ctx->vad_) {
        LOG_ERROR("AudioEngine_Agc2_Init: failed to create VoiceActivityDetectorWrapper");
        ctx->DeinitInternal();
        return AUDIO_ENGINE_ERR_INIT_FAILED;
    }

    int rate = init_config->sample_rate;
    size_t ch = static_cast<size_t>(init_config->num_channels);
    ctx->audio_buffer_ = new (std::nothrow) AudioBuffer(rate, ch, rate, ch, rate, ch);
    if (!ctx->audio_buffer_) {
        LOG_ERROR("AudioEngine_Agc2_Init: failed to create AudioBuffer");
        ctx->DeinitInternal();
        return AUDIO_ENGINE_ERR_INIT_FAILED;
    }

    ctx->stream_config_ = StreamConfig(rate, ch);
    ctx->frame_size_ = (rate / 100) * init_config->num_channels;
    ctx->initialized_ = true;

    LOG_DEBUG("AudioEngine_Agc2_Init OK: rate=%d ch=%d frame_size=%d",
              rate, init_config->num_channels, ctx->frame_size_);
    return AUDIO_ENGINE_SUCCESS;
}

/* ============================================================================
 * SetParam / ResetParam (AGC2 has no runtime-configurable parameters yet)
 * ==========================================================================*/
extern "C" int AudioEngine_Agc2_SetParam(Agc2Handle handle, const Agc2RtConfig* rt_config)
{
    LOG_DEBUG("AudioEngine_Agc2_SetParam(handle=%p, config=%p)", handle, rt_config);
    if (!handle) {
        LOG_WARN("AudioEngine_Agc2_SetParam: NULL handle");
        return AUDIO_ENGINE_ERR_INVALID_HANDLE;
    }
    if (!rt_config) {
        LOG_WARN("AudioEngine_Agc2_SetParam: NULL rt_config");
        return AUDIO_ENGINE_ERR_NULL_POINTER;
    }
    /* Currently all fields reserved — nothing to apply */
    static_cast<AudioEngineAgc2*>(handle)->rt_config_ = *rt_config;
    return AUDIO_ENGINE_SUCCESS;
}

extern "C" int AudioEngine_Agc2_ResetParam(Agc2Handle handle, const Agc2RtConfig* rt_config)
{
    LOG_DEBUG("AudioEngine_Agc2_ResetParam(handle=%p, config=%p)", handle, rt_config);
    if (!handle) {
        LOG_WARN("AudioEngine_Agc2_ResetParam: NULL handle");
        return AUDIO_ENGINE_ERR_INVALID_HANDLE;
    }
    if (!rt_config) {
        LOG_WARN("AudioEngine_Agc2_ResetParam: NULL rt_config");
        return AUDIO_ENGINE_ERR_NULL_POINTER;
    }
    AudioEngineAgc2* ctx = static_cast<AudioEngineAgc2*>(handle);
    ctx->rt_config_.reserved = 0;
    /* Merge caller fields (only non-sentinel values) */
    if (rt_config->reserved >= 0) {
        ctx->rt_config_.reserved = rt_config->reserved;
    }
    return AUDIO_ENGINE_SUCCESS;
}

/* ============================================================================
 * Process
 * ==========================================================================*/
extern "C" int AudioEngine_Agc2_Process(
    Agc2Handle handle,
    const short* audio_in,
    int in_samples,
    short* audio_out,
    int max_out_samples,
    int* out_samples)
{
    LOG_DEBUG("AudioEngine_Agc2_Process(handle=%p, in=%p, in_samples=%d, out=%p, max_out=%d, out_samples=%p)",
              handle, audio_in, in_samples, audio_out, max_out_samples, out_samples);
    if (!handle) {
        LOG_WARN("AudioEngine_Agc2_Process: NULL handle");
        return AUDIO_ENGINE_ERR_INVALID_HANDLE;
    }
    AudioEngineAgc2* ctx = static_cast<AudioEngineAgc2*>(handle);
    if (!ctx->initialized_) {
        LOG_WARN("AudioEngine_Agc2_Process: not initialized");
        return AUDIO_ENGINE_ERR_NOT_INITIALIZED;
    }
    if (!audio_in || !audio_out || !out_samples) {
        LOG_WARN("AudioEngine_Agc2_Process: NULL pointer");
        return AUDIO_ENGINE_ERR_NULL_POINTER;
    }
    if (in_samples != ctx->frame_size_) {
        /* Strict 10ms frame: caller must zero-pad partial final frames */
        LOG_WARN("AudioEngine_Agc2_Process: invalid in_samples=%d (frame_size=%d)",
                 in_samples, ctx->frame_size_);
        return AUDIO_ENGINE_ERR_INVALID_PARAM;
    }
    if (max_out_samples < ctx->frame_size_) {
        LOG_WARN("AudioEngine_Agc2_Process: max_out_samples=%d < frame_size=%d",
                 max_out_samples, ctx->frame_size_);
        return AUDIO_ENGINE_ERR_INVALID_PARAM;
    }

    int frame_samples = ctx->frame_size_;

    /* int16 -> AudioBuffer float */
    ctx->audio_buffer_->CopyFrom(audio_in, ctx->stream_config_);

    /* RNN VAD */
    auto view = ctx->audio_buffer_->view();
    float speech_probability = ctx->vad_->Analyze(view);

    /* Compute RMS and peak levels in dBFS for SpeechLevelEstimator */
    const float* ch = ctx->audio_buffer_->channels_const()[0];
    float sum_sq = 0.0f;
    float peak = 0.0f;
    for (int i = 0; i < frame_samples; ++i) {
        float val = ch[i];
        sum_sq += val * val;
        peak = std::max(peak, std::fabs(val));
    }
    float rms = std::sqrt(sum_sq / frame_samples);
    float rms_dbfs = FloatS16ToDbfs(rms);
    float peak_dbfs = FloatS16ToDbfs(peak);

    /* Update speech / noise level estimators */
    ctx->speech_level_estimator_->Update(rms_dbfs, peak_dbfs, speech_probability);
    float noise_rms_dbfs = ctx->noise_level_estimator_->Analyze(view);

    /* Build FrameInfo and apply adaptive digital gain */
    AdaptiveDigitalGainController::FrameInfo info;
    info.speech_probability = speech_probability;
    info.speech_level_dbfs = ctx->speech_level_estimator_->level_dbfs();
    info.speech_level_reliable = ctx->speech_level_estimator_->is_confident();
    info.noise_rms_dbfs = noise_rms_dbfs;
    info.headroom_db = ctx->init_config_.headroom_db;
    info.limiter_envelope_dbfs = -2.0f;

    ctx->gain_controller_->Process(info, view);

    /* float -> int16 */
    ctx->audio_buffer_->CopyTo(ctx->stream_config_, audio_out);
    *out_samples = frame_samples;

    return AUDIO_ENGINE_SUCCESS;
}

/* ============================================================================
 * Deinit / Reset
 * ==========================================================================*/
extern "C" int AudioEngine_Agc2_Deinit(Agc2Handle handle)
{
    LOG_DEBUG("AudioEngine_Agc2_Deinit(handle=%p)", handle);
    if (!handle) {
        LOG_WARN("AudioEngine_Agc2_Deinit: NULL handle");
        return AUDIO_ENGINE_ERR_INVALID_HANDLE;
    }
    AudioEngineAgc2* ctx = static_cast<AudioEngineAgc2*>(handle);
    ctx->DeinitInternal();
    return AUDIO_ENGINE_SUCCESS;
}

extern "C" int AudioEngine_Agc2_Reset(Agc2Handle handle)
{
    LOG_DEBUG("AudioEngine_Agc2_Reset(handle=%p)", handle);
    if (!handle) {
        LOG_WARN("AudioEngine_Agc2_Reset: NULL handle");
        return AUDIO_ENGINE_ERR_INVALID_HANDLE;
    }
    AudioEngineAgc2* ctx = static_cast<AudioEngineAgc2*>(handle);
    if (!ctx->initialized_) {
        LOG_WARN("AudioEngine_Agc2_Reset: not initialized");
        return AUDIO_ENGINE_ERR_NOT_INITIALIZED;
    }

    /* Rebuild pipeline, preserving init config */
    Agc2InitConfig cfg = ctx->init_config_;
    ctx->DeinitInternal();
    return AudioEngine_Agc2_Init(handle, &cfg);
}
