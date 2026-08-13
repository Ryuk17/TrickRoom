/*
 * @Author: Ryuk
 * @Date: 2026-08-12
 * @Description: IE (Intelligibility Enhancer) interface implementation
 */

#include "audio_engine_ie.h"
#include "audio_engine_log.h"

#include <new>
#include <vector>

#include "audio_processing/intelligibility_enhancer/intelligibility_enhancer.h"
#include "utils/channel_buffer.h"
#include "utils/audio_util.h"

using namespace webrtc;


/* ============================================================================
 * Internal data container
 * ==========================================================================*/
class AudioEngineIe {
public:
    AudioEngineIe()
        : initialized_(false)
        , noise_valid_(false)
        , enhancer_(nullptr)
        , in_buf_(nullptr) {
        init_config_.sample_rate       = 16000;
        init_config_.frame_len         = 160;
        init_config_.num_channels      = 1;
        init_config_.decay_rate        = 0.9f;
        init_config_.analysis_rate     = 60;
        init_config_.gain_change_limit = 0.1f;
        init_config_.rho               = 0.02f;
    }

    ~AudioEngineIe() {
        DeinitInternal();
    }

    void DeinitInternal() {
        if (enhancer_) {
            delete enhancer_;
            enhancer_ = nullptr;
        }
        if (in_buf_) {
            delete in_buf_;
            in_buf_ = nullptr;
        }
        noise_valid_ = false;
        initialized_ = false;
    }

    bool initialized_;
    bool noise_valid_;
    IeInitConfig init_config_;
    IntelligibilityEnhancer* enhancer_;
    ChannelBuffer<float>* in_buf_;
};


/* ============================================================================
 * Create / Destroy
 * ==========================================================================*/
extern "C" IeHandle AudioEngine_Ie_Create(void)
{
    LOG_DEBUG("AudioEngine_Ie_Create");

    AudioEngineIe* ptr = new (std::nothrow) AudioEngineIe();
    if (!ptr) {
        LOG_ERROR("AudioEngine_Ie_Create: memory allocation failed");
    }
    return static_cast<IeHandle>(ptr);
}

extern "C" int AudioEngine_Ie_Destroy(IeHandle handle)
{
    LOG_DEBUG("AudioEngine_Ie_Destroy, handle=%p", (void*)handle);

    if (!handle) {
        LOG_WARN("AudioEngine_Ie_Destroy: invalid handle (NULL)");
        return AUDIO_ENGINE_ERR_INVALID_HANDLE;
    }

    AudioEngineIe* ptr = static_cast<AudioEngineIe*>(handle);
    delete ptr;

    return AUDIO_ENGINE_SUCCESS;
}


/* ============================================================================
 * Init / Deinit
 * ==========================================================================*/
extern "C" int AudioEngine_Ie_Init(IeHandle handle, const IeInitConfig* init_config)
{
    LOG_DEBUG("AudioEngine_Ie_Init, handle=%p", (void*)handle);

    if (!handle) {
        LOG_WARN("AudioEngine_Ie_Init: invalid handle (NULL)");
        return AUDIO_ENGINE_ERR_INVALID_HANDLE;
    }
    if (!init_config) {
        LOG_WARN("AudioEngine_Ie_Init: init_config is NULL");
        return AUDIO_ENGINE_ERR_NULL_POINTER;
    }
    if (init_config->sample_rate <= 0) {
        LOG_WARN("AudioEngine_Ie_Init: invalid sample_rate=%d", init_config->sample_rate);
        return AUDIO_ENGINE_ERR_INVALID_PARAM;
    }
    if (init_config->frame_len != init_config->sample_rate / 100) {
        LOG_WARN("AudioEngine_Ie_Init: frame_len=%d must equal sample_rate/100=%d (10ms constraint)",
                 init_config->frame_len, init_config->sample_rate / 100);
        return AUDIO_ENGINE_ERR_INVALID_PARAM;
    }
    if (init_config->num_channels < 1) {
        LOG_WARN("AudioEngine_Ie_Init: invalid num_channels=%d", init_config->num_channels);
        return AUDIO_ENGINE_ERR_INVALID_PARAM;
    }

    AudioEngineIe* ptr = static_cast<AudioEngineIe*>(handle);

    /* If already initialized, clean up first */
    if (ptr->initialized_) {
        LOG_DEBUG("AudioEngine_Ie_Init: already initialized, deinit first");
        ptr->DeinitInternal();
    }

    ptr->init_config_ = *init_config;

    /* Build IE Config */
    IntelligibilityEnhancer::Config cfg;
    cfg.sample_rate_hz     = init_config->sample_rate;
    cfg.num_capture_channels = 1;
    cfg.num_render_channels  = static_cast<size_t>(init_config->num_channels);
    cfg.decay_rate         = init_config->decay_rate;
    cfg.analysis_rate      = init_config->analysis_rate;
    cfg.gain_change_limit  = init_config->gain_change_limit;
    cfg.rho                = init_config->rho;

    ptr->enhancer_ = new (std::nothrow) IntelligibilityEnhancer(cfg);
    if (!ptr->enhancer_) {
        LOG_ERROR("AudioEngine_Ie_Init: IntelligibilityEnhancer allocation failed");
        return AUDIO_ENGINE_ERR_INIT_FAILED;
    }

    /* Create ChannelBuffer for Process (deinterleaved float audio) */
    size_t num_frames = static_cast<size_t>(init_config->frame_len);
    size_t num_ch     = static_cast<size_t>(init_config->num_channels);
    ptr->in_buf_ = new (std::nothrow) ChannelBuffer<float>(num_frames, num_ch);
    if (!ptr->in_buf_) {
        LOG_ERROR("AudioEngine_Ie_Init: ChannelBuffer allocation failed");
        ptr->DeinitInternal();
        return AUDIO_ENGINE_ERR_INIT_FAILED;
    }

    ptr->noise_valid_ = false;
    ptr->initialized_ = true;

    LOG_INFO("AudioEngine_Ie_Init OK, sample_rate=%d, frame_len=%d, num_channels=%d, "
             "decay=%.2f, analysis_rate=%d",
             init_config->sample_rate, init_config->frame_len,
             init_config->num_channels,
             init_config->decay_rate, init_config->analysis_rate);
    return AUDIO_ENGINE_SUCCESS;
}

extern "C" int AudioEngine_Ie_Deinit(IeHandle handle)
{
    LOG_DEBUG("AudioEngine_Ie_Deinit, handle=%p", (void*)handle);

    if (!handle) {
        LOG_WARN("AudioEngine_Ie_Deinit: invalid handle (NULL)");
        return AUDIO_ENGINE_ERR_INVALID_HANDLE;
    }

    AudioEngineIe* ptr = static_cast<AudioEngineIe*>(handle);
    ptr->DeinitInternal();

    LOG_DEBUG("AudioEngine_Ie_Deinit OK");
    return AUDIO_ENGINE_SUCCESS;
}


/* ============================================================================
 * SetNoiseEstimate
 * ==========================================================================*/
extern "C" int AudioEngine_Ie_SetNoiseEstimate(
    IeHandle handle,
    const float* noise_spectrum,
    int num_freqs)
{
    LOG_DEBUG("AudioEngine_Ie_SetNoiseEstimate, handle=%p, num_freqs=%d",
              (void*)handle, num_freqs);

    if (!handle) {
        LOG_WARN("AudioEngine_Ie_SetNoiseEstimate: invalid handle (NULL)");
        return AUDIO_ENGINE_ERR_INVALID_HANDLE;
    }
    if (!noise_spectrum) {
        LOG_WARN("AudioEngine_Ie_SetNoiseEstimate: noise_spectrum is NULL");
        return AUDIO_ENGINE_ERR_NULL_POINTER;
    }
    if (num_freqs <= 0) {
        LOG_WARN("AudioEngine_Ie_SetNoiseEstimate: num_freqs=%d must be > 0", num_freqs);
        return AUDIO_ENGINE_ERR_INVALID_PARAM;
    }

    AudioEngineIe* ptr = static_cast<AudioEngineIe*>(handle);

    if (!ptr->initialized_) {
        LOG_WARN("AudioEngine_Ie_SetNoiseEstimate: not initialized");
        return AUDIO_ENGINE_ERR_NOT_INITIALIZED;
    }

    std::vector<float> noise(noise_spectrum, noise_spectrum + num_freqs);
    ptr->enhancer_->SetCaptureNoiseEstimate(noise);
    ptr->noise_valid_ = true;

    return AUDIO_ENGINE_SUCCESS;
}


/* ============================================================================
 * Process
 * ==========================================================================*/
extern "C" int AudioEngine_Ie_Process(
    IeHandle handle,
    const short* audio_in,
    int in_samples,
    short* audio_out)
{
    if (!handle) {
        LOG_WARN("AudioEngine_Ie_Process: invalid handle (NULL)");
        return AUDIO_ENGINE_ERR_INVALID_HANDLE;
    }
    if (!audio_in) {
        LOG_WARN("AudioEngine_Ie_Process: audio_in is NULL");
        return AUDIO_ENGINE_ERR_NULL_POINTER;
    }
    if (!audio_out) {
        LOG_WARN("AudioEngine_Ie_Process: audio_out is NULL");
        return AUDIO_ENGINE_ERR_NULL_POINTER;
    }

    AudioEngineIe* ptr = static_cast<AudioEngineIe*>(handle);

    if (!ptr->initialized_) {
        LOG_WARN("AudioEngine_Ie_Process: not initialized");
        return AUDIO_ENGINE_ERR_NOT_INITIALIZED;
    }

    const int expected_samples = ptr->init_config_.frame_len;
    if (in_samples != expected_samples) {
        LOG_WARN("AudioEngine_Ie_Process: in_samples=%d, expected %d (10ms constraint)",
                 in_samples, expected_samples);
        return AUDIO_ENGINE_ERR_INVALID_PARAM;
    }

    const int num_channels = ptr->init_config_.num_channels;
    const int num_frames   = in_samples;
    const int total_in     = num_frames * num_channels;

    /* 1. Convert interleaved short → float */
    std::vector<float> float_in(total_in);
    S16ToFloat(audio_in, total_in, float_in.data());

    /* 2. Deinterleave into ChannelBuffer */
    Deinterleave(float_in.data(), num_frames, num_channels,
                 ptr->in_buf_->channels());

    /* 3. ProcessRenderAudio (in-place on deinterleaved float) */
    ptr->enhancer_->ProcessRenderAudio(
        ptr->in_buf_->channels(),
        ptr->init_config_.sample_rate,
        static_cast<size_t>(num_channels));

    /* 4. Interleave back → float buffer */
    std::vector<float> float_out(total_in);
    const float* const* out_channels = ptr->in_buf_->channels();
    for (int c = 0; c < num_channels; ++c) {
        for (int s = 0; s < num_frames; ++s) {
            float_out[s * num_channels + c] = out_channels[c][s];
        }
    }

    /* 5. Convert float → short */
    FloatToS16(float_out.data(), total_in, audio_out);

    return AUDIO_ENGINE_SUCCESS;
}


/* ============================================================================
 * Reset
 * ==========================================================================*/
extern "C" int AudioEngine_Ie_Reset(IeHandle handle)
{
    LOG_DEBUG("AudioEngine_Ie_Reset, handle=%p", (void*)handle);

    if (!handle) {
        LOG_WARN("AudioEngine_Ie_Reset: invalid handle (NULL)");
        return AUDIO_ENGINE_ERR_INVALID_HANDLE;
    }

    AudioEngineIe* ptr = static_cast<AudioEngineIe*>(handle);

    if (!ptr->initialized_) {
        LOG_WARN("AudioEngine_Ie_Reset: not initialized");
        return AUDIO_ENGINE_ERR_NOT_INITIALIZED;
    }

    /* Destroy and recreate enhancer; keep config */
    if (ptr->enhancer_) {
        delete ptr->enhancer_;
        ptr->enhancer_ = nullptr;
    }

    IntelligibilityEnhancer::Config cfg;
    cfg.sample_rate_hz      = ptr->init_config_.sample_rate;
    cfg.num_capture_channels = 1;
    cfg.num_render_channels  = static_cast<size_t>(ptr->init_config_.num_channels);
    cfg.decay_rate          = ptr->init_config_.decay_rate;
    cfg.analysis_rate       = ptr->init_config_.analysis_rate;
    cfg.gain_change_limit   = ptr->init_config_.gain_change_limit;
    cfg.rho                 = ptr->init_config_.rho;

    ptr->enhancer_ = new (std::nothrow) IntelligibilityEnhancer(cfg);
    if (!ptr->enhancer_) {
        LOG_ERROR("AudioEngine_Ie_Reset: IntelligibilityEnhancer allocation failed");
        ptr->initialized_ = false;
        return AUDIO_ENGINE_ERR_PROCESS_FAILED;
    }

    ptr->noise_valid_ = false;

    LOG_INFO("AudioEngine_Ie_Reset OK");
    return AUDIO_ENGINE_SUCCESS;
}
