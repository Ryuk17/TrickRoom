/*
 * @Author: Ryuk
 * @Date: 2026-08-12
 * @Description: TS (Transient Suppressor) interface implementation
 */

#include "audio_engine_ts.h"
#include "audio_engine_log.h"

#include <new>
#include <vector>

#include "audio_processing/transient_suppressor/transient_suppressor.h"
#include "utils/audio_util.h"

using namespace webrtc;


/* ============================================================================
 * Internal data container
 * ==========================================================================*/
class AudioEngineTs {
public:
    AudioEngineTs()
        : initialized_(false)
        , suppressor_(nullptr) {
        init_config_.sample_rate    = 16000;
        init_config_.frame_len      = 160;
        init_config_.num_channels   = 1;
        init_config_.detector_rate  = 16000;
    }

    ~AudioEngineTs() {
        if (suppressor_) {
            delete suppressor_;
            suppressor_ = nullptr;
        }
    }

    bool initialized_;
    TsInitConfig init_config_;
    TransientSuppressor* suppressor_;
};


/* ============================================================================
 * Create / Destroy
 * ==========================================================================*/
extern "C" TsHandle AudioEngine_Ts_Create(void)
{
    LOG_DEBUG("AudioEngine_Ts_Create");

    AudioEngineTs* ptr = new (std::nothrow) AudioEngineTs();
    if (!ptr) {
        LOG_ERROR("AudioEngine_Ts_Create: memory allocation failed");
    }
    return static_cast<TsHandle>(ptr);
}

extern "C" int AudioEngine_Ts_Destroy(TsHandle handle)
{
    LOG_DEBUG("AudioEngine_Ts_Destroy, handle=%p", (void*)handle);

    if (!handle) {
        LOG_WARN("AudioEngine_Ts_Destroy: invalid handle (NULL)");
        return AUDIO_ENGINE_ERR_INVALID_HANDLE;
    }

    AudioEngineTs* ptr = static_cast<AudioEngineTs*>(handle);
    delete ptr;

    return AUDIO_ENGINE_SUCCESS;
}


/* ============================================================================
 * Init / Deinit
 * ==========================================================================*/
extern "C" int AudioEngine_Ts_Init(TsHandle handle, const TsInitConfig* init_config)
{
    LOG_DEBUG("AudioEngine_Ts_Init, handle=%p", (void*)handle);

    if (!handle) {
        LOG_WARN("AudioEngine_Ts_Init: invalid handle (NULL)");
        return AUDIO_ENGINE_ERR_INVALID_HANDLE;
    }
    if (!init_config) {
        LOG_WARN("AudioEngine_Ts_Init: init_config is NULL");
        return AUDIO_ENGINE_ERR_NULL_POINTER;
    }
    if (init_config->sample_rate != 8000 && init_config->sample_rate != 16000 &&
        init_config->sample_rate != 32000 && init_config->sample_rate != 48000) {
        LOG_WARN("AudioEngine_Ts_Init: sample_rate=%d not supported (8/16/32/48 kHz)",
                 init_config->sample_rate);
        return AUDIO_ENGINE_ERR_INVALID_PARAM;
    }
    if (init_config->frame_len != init_config->sample_rate / 100) {
        LOG_WARN("AudioEngine_Ts_Init: frame_len=%d must equal sample_rate/100=%d (10ms constraint)",
                 init_config->frame_len, init_config->sample_rate / 100);
        return AUDIO_ENGINE_ERR_INVALID_PARAM;
    }
    if (init_config->num_channels < 1) {
        LOG_WARN("AudioEngine_Ts_Init: invalid num_channels=%d", init_config->num_channels);
        return AUDIO_ENGINE_ERR_INVALID_PARAM;
    }
    if (init_config->detector_rate <= 0) {
        LOG_WARN("AudioEngine_Ts_Init: invalid detector_rate=%d", init_config->detector_rate);
        return AUDIO_ENGINE_ERR_INVALID_PARAM;
    }

    AudioEngineTs* ptr = static_cast<AudioEngineTs*>(handle);

    /* If already initialized, clean up first */
    if (ptr->initialized_) {
        LOG_DEBUG("AudioEngine_Ts_Init: already initialized, deinit first");
        if (ptr->suppressor_) {
            delete ptr->suppressor_;
            ptr->suppressor_ = nullptr;
        }
        ptr->initialized_ = false;
    }

    ptr->init_config_ = *init_config;

    ptr->suppressor_ = new (std::nothrow) TransientSuppressor();
    if (!ptr->suppressor_) {
        LOG_ERROR("AudioEngine_Ts_Init: TransientSuppressor allocation failed");
        return AUDIO_ENGINE_ERR_INIT_FAILED;
    }

    int ret = ptr->suppressor_->Initialize(init_config->sample_rate,
                                           init_config->detector_rate,
                                           init_config->num_channels);
    if (ret != 0) {
        LOG_ERROR("AudioEngine_Ts_Init: TransientSuppressor::Initialize failed (%d)", ret);
        delete ptr->suppressor_;
        ptr->suppressor_ = nullptr;
        return AUDIO_ENGINE_ERR_INIT_FAILED;
    }

    ptr->initialized_ = true;

    LOG_INFO("AudioEngine_Ts_Init OK, sample_rate=%d, frame_len=%d, "
             "num_channels=%d, detector_rate=%d",
             init_config->sample_rate, init_config->frame_len,
             init_config->num_channels, init_config->detector_rate);
    return AUDIO_ENGINE_SUCCESS;
}

extern "C" int AudioEngine_Ts_Deinit(TsHandle handle)
{
    LOG_DEBUG("AudioEngine_Ts_Deinit, handle=%p", (void*)handle);

    if (!handle) {
        LOG_WARN("AudioEngine_Ts_Deinit: invalid handle (NULL)");
        return AUDIO_ENGINE_ERR_INVALID_HANDLE;
    }

    AudioEngineTs* ptr = static_cast<AudioEngineTs*>(handle);

    if (ptr->suppressor_) {
        delete ptr->suppressor_;
        ptr->suppressor_ = nullptr;
    }
    ptr->initialized_ = false;

    LOG_DEBUG("AudioEngine_Ts_Deinit OK");
    return AUDIO_ENGINE_SUCCESS;
}


/* ============================================================================
 * Process
 * ==========================================================================*/
extern "C" int AudioEngine_Ts_Process(
    TsHandle handle,
    const short* audio_in,
    int in_samples,
    const short* detection_in,
    const short* reference_in,
    float voice_probability,
    int key_pressed,
    short* audio_out)
{
    if (!handle) {
        LOG_WARN("AudioEngine_Ts_Process: invalid handle (NULL)");
        return AUDIO_ENGINE_ERR_INVALID_HANDLE;
    }
    if (!audio_in) {
        LOG_WARN("AudioEngine_Ts_Process: audio_in is NULL");
        return AUDIO_ENGINE_ERR_NULL_POINTER;
    }
    if (!audio_out) {
        LOG_WARN("AudioEngine_Ts_Process: audio_out is NULL");
        return AUDIO_ENGINE_ERR_NULL_POINTER;
    }
    if (voice_probability < 0.0f || voice_probability > 1.0f) {
        LOG_WARN("AudioEngine_Ts_Process: voice_probability=%.2f out of range [0,1]",
                 voice_probability);
        return AUDIO_ENGINE_ERR_INVALID_PARAM;
    }

    AudioEngineTs* ptr = static_cast<AudioEngineTs*>(handle);

    if (!ptr->initialized_) {
        LOG_WARN("AudioEngine_Ts_Process: not initialized");
        return AUDIO_ENGINE_ERR_NOT_INITIALIZED;
    }

    const int expected_samples = ptr->init_config_.frame_len;
    if (in_samples != expected_samples) {
        LOG_WARN("AudioEngine_Ts_Process: in_samples=%d, expected %d (10ms constraint)",
                 in_samples, expected_samples);
        return AUDIO_ENGINE_ERR_INVALID_PARAM;
    }

    const int num_channels = ptr->init_config_.num_channels;
    const int num_frames   = in_samples;
    const int total_in     = num_frames * num_channels;

    /* Detection length depends on detector_rate */
    const int detection_len =
        num_frames * ptr->init_config_.detector_rate / ptr->init_config_.sample_rate;

    /* 1. Convert interleaved short → float, deinterleave into per-channel blocks
       (Suppress expects channels concatenated one after the other, int16 range) */
    std::vector<float> data_deint(total_in);
    for (int c = 0; c < num_channels; ++c) {
        for (int s = 0; s < num_frames; ++s) {
            /* Direct int16 → float cast, matching internal test convention */
            data_deint[c * num_frames + s] =
                static_cast<float>(audio_in[s * num_channels + c]);
        }
    }

    /* 2. Detection data (mono, int16 range) */
    std::vector<float> detection_f;
    if (detection_in) {
        detection_f.resize(detection_len);
        for (int i = 0; i < detection_len; ++i) {
            detection_f[i] = static_cast<float>(detection_in[i]);
        }
    }

    /* 3. Reference data (mono, [-1, 1] range per internal test convention) */
    std::vector<float> reference_f;
    if (reference_in) {
        reference_f.resize(num_frames);
        S16ToFloat(reference_in, num_frames, reference_f.data());
    }

    /* 4. Suppress */
    int ret = ptr->suppressor_->Suppress(
        data_deint.data(),
        num_frames,
        num_channels,
        detection_in ? detection_f.data() : nullptr,
        detection_len,
        reference_in ? reference_f.data() : nullptr,
        num_frames,
        voice_probability,
        key_pressed != 0);
    if (ret != 0) {
        LOG_ERROR("AudioEngine_Ts_Process: TransientSuppressor::Suppress failed (%d)", ret);
        return AUDIO_ENGINE_ERR_PROCESS_FAILED;
    }

    /* 5. Convert back: int16-range float → interleave → short
       (FloatS16ToS16 does clamp + round-half-away-from-zero) */
    std::vector<float> float_out(total_in);
    for (int c = 0; c < num_channels; ++c) {
        for (int s = 0; s < num_frames; ++s) {
            float_out[s * num_channels + c] =
                data_deint[c * num_frames + s];
        }
    }
    FloatS16ToS16(float_out.data(), total_in, audio_out);

    return AUDIO_ENGINE_SUCCESS;
}


/* ============================================================================
 * Reset
 * ==========================================================================*/
extern "C" int AudioEngine_Ts_Reset(TsHandle handle)
{
    LOG_DEBUG("AudioEngine_Ts_Reset, handle=%p", (void*)handle);

    if (!handle) {
        LOG_WARN("AudioEngine_Ts_Reset: invalid handle (NULL)");
        return AUDIO_ENGINE_ERR_INVALID_HANDLE;
    }

    AudioEngineTs* ptr = static_cast<AudioEngineTs*>(handle);

    if (!ptr->initialized_) {
        LOG_WARN("AudioEngine_Ts_Reset: not initialized");
        return AUDIO_ENGINE_ERR_NOT_INITIALIZED;
    }

    /* Destroy and recreate suppressor; keep config */
    if (ptr->suppressor_) {
        delete ptr->suppressor_;
        ptr->suppressor_ = nullptr;
    }

    ptr->suppressor_ = new (std::nothrow) TransientSuppressor();
    if (!ptr->suppressor_) {
        LOG_ERROR("AudioEngine_Ts_Reset: TransientSuppressor allocation failed");
        ptr->initialized_ = false;
        return AUDIO_ENGINE_ERR_PROCESS_FAILED;
    }

    int ret = ptr->suppressor_->Initialize(ptr->init_config_.sample_rate,
                                           ptr->init_config_.detector_rate,
                                           ptr->init_config_.num_channels);
    if (ret != 0) {
        LOG_ERROR("AudioEngine_Ts_Reset: TransientSuppressor::Initialize failed (%d)", ret);
        delete ptr->suppressor_;
        ptr->suppressor_ = nullptr;
        ptr->initialized_ = false;
        return AUDIO_ENGINE_ERR_PROCESS_FAILED;
    }

    LOG_INFO("AudioEngine_Ts_Reset OK, sample_rate=%d, num_channels=%d",
             ptr->init_config_.sample_rate, ptr->init_config_.num_channels);
    return AUDIO_ENGINE_SUCCESS;
}
