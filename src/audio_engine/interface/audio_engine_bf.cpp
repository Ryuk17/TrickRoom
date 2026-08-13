/*
 * @Author: Ryuk
 * @Date: 2026-08-12
 * @Description: BF (Beamforming / NonlinearBeamformer) interface implementation
 */

#include "audio_engine_bf.h"
#include "audio_engine_log.h"

#include <new>
#include <vector>

#include "audio_processing/beamforming/nonlinear_beamformer.h"
#include "audio_processing/beamforming/array_util.h"
#include "utils/channel_buffer.h"
#include "utils/audio_util.h"

using namespace webrtc;


/* ============================================================================
 * Internal data container (pure data — C API operates directly on it)
 * ==========================================================================*/
class AudioEngineBf {
public:
    AudioEngineBf()
        : initialized_(false)
        , bf_(nullptr)
        , in_buf_(nullptr)
        , out_buf_(nullptr) {
        init_config_.sample_rate       = 0;
        init_config_.num_channels      = 0;
        init_config_.frame_len         = 0;
        init_config_.mic_pos           = nullptr;
        init_config_.target_azimuth    = static_cast<float>(M_PI) / 2.0f;
        init_config_.target_elevation  = 0.0f;
        rt_config_.target_azimuth      = static_cast<float>(M_PI) / 2.0f;
        rt_config_.target_elevation    = 0.0f;
    }

    ~AudioEngineBf() {
        DeinitInternal();
    }

    void DeinitInternal() {
        if (bf_) {
            delete bf_;
            bf_ = nullptr;
        }
        if (in_buf_) {
            delete in_buf_;
            in_buf_ = nullptr;
        }
        if (out_buf_) {
            delete out_buf_;
            out_buf_ = nullptr;
        }
        initialized_ = false;
    }

    bool initialized_;
    BfInitConfig init_config_;
    BfRtConfig rt_config_;

    NonlinearBeamformer* bf_;
    ChannelBuffer<float>* in_buf_;
    ChannelBuffer<float>* out_buf_;

    /* Copied mic positions — NonlinearBeamformer takes ownership-like
       reference to the vector in constructor, so we must keep a copy alive. */
    std::vector<Point> mic_positions_;
};


/* ============================================================================
 * Create / Destroy
 * ==========================================================================*/
extern "C" BfHandle AudioEngine_Bf_Create(void)
{
    LOG_DEBUG("AudioEngine_Bf_Create");

    AudioEngineBf* ptr = new (std::nothrow) AudioEngineBf();
    if (!ptr) {
        LOG_ERROR("AudioEngine_Bf_Create: memory allocation failed");
    }
    return static_cast<BfHandle>(ptr);
}

extern "C" int AudioEngine_Bf_Destroy(BfHandle handle)
{
    LOG_DEBUG("AudioEngine_Bf_Destroy, handle=%p", (void*)handle);

    if (!handle) {
        LOG_WARN("AudioEngine_Bf_Destroy: invalid handle (NULL)");
        return AUDIO_ENGINE_ERR_INVALID_HANDLE;
    }

    AudioEngineBf* ptr = static_cast<AudioEngineBf*>(handle);
    delete ptr;

    return AUDIO_ENGINE_SUCCESS;
}


/* ============================================================================
 * Init / Deinit
 * ==========================================================================*/
extern "C" int AudioEngine_Bf_Init(BfHandle handle, const BfInitConfig* init_config)
{
    LOG_DEBUG("AudioEngine_Bf_Init, handle=%p", (void*)handle);

    if (!handle) {
        LOG_WARN("AudioEngine_Bf_Init: invalid handle (NULL)");
        return AUDIO_ENGINE_ERR_INVALID_HANDLE;
    }
    if (!init_config) {
        LOG_WARN("AudioEngine_Bf_Init: init_config is NULL");
        return AUDIO_ENGINE_ERR_NULL_POINTER;
    }
    if (init_config->sample_rate <= 0) {
        LOG_WARN("AudioEngine_Bf_Init: invalid sample_rate=%d", init_config->sample_rate);
        return AUDIO_ENGINE_ERR_INVALID_PARAM;
    }
    if (init_config->num_channels < 1) {
        LOG_WARN("AudioEngine_Bf_Init: invalid num_channels=%d", init_config->num_channels);
        return AUDIO_ENGINE_ERR_INVALID_PARAM;
    }
    if (init_config->frame_len != init_config->sample_rate / 100) {
        LOG_WARN("AudioEngine_Bf_Init: frame_len=%d must equal sample_rate/100=%d (10ms constraint)",
                 init_config->frame_len, init_config->sample_rate / 100);
        return AUDIO_ENGINE_ERR_INVALID_PARAM;
    }
    if (!init_config->mic_pos) {
        LOG_WARN("AudioEngine_Bf_Init: mic_pos is NULL");
        return AUDIO_ENGINE_ERR_NULL_POINTER;
    }

    AudioEngineBf* ptr = static_cast<AudioEngineBf*>(handle);

    /* If already initialized, clean up first */
    if (ptr->initialized_) {
        LOG_DEBUG("AudioEngine_Bf_Init: already initialized, deinit first");
        ptr->DeinitInternal();
    }

    /* Copy mic positions */
    ptr->mic_positions_.clear();
    ptr->mic_positions_.reserve(init_config->num_channels);
    for (int i = 0; i < init_config->num_channels; ++i) {
        ptr->mic_positions_.push_back(
            Point(init_config->mic_pos[i].x,
                  init_config->mic_pos[i].y,
                  init_config->mic_pos[i].z));
    }

    /* Save init config */
    ptr->init_config_ = *init_config;

    /* Default runtime config */
    ptr->rt_config_.target_azimuth   = init_config->target_azimuth;
    ptr->rt_config_.target_elevation = init_config->target_elevation;

    /* Create NonlinearBeamformer */
    SphericalPointf target_dir(ptr->rt_config_.target_azimuth,
                               ptr->rt_config_.target_elevation,
                               1.0f);
    ptr->bf_ = new (std::nothrow) NonlinearBeamformer(ptr->mic_positions_, target_dir);
    if (!ptr->bf_) {
        LOG_ERROR("AudioEngine_Bf_Init: NonlinearBeamformer allocation failed");
        ptr->DeinitInternal();
        return AUDIO_ENGINE_ERR_INIT_FAILED;
    }

    /* Initialize (10ms chunks) */
    ptr->bf_->Initialize(/*chunk_size_ms=*/10, init_config->sample_rate);

    /* Create ChannelBuffers for Process */
    size_t num_frames = static_cast<size_t>(init_config->frame_len);
    size_t num_in_ch  = static_cast<size_t>(init_config->num_channels);
    ptr->in_buf_  = new (std::nothrow) ChannelBuffer<float>(num_frames, num_in_ch);
    ptr->out_buf_ = new (std::nothrow) ChannelBuffer<float>(num_frames, 1);
    if (!ptr->in_buf_ || !ptr->out_buf_) {
        LOG_ERROR("AudioEngine_Bf_Init: ChannelBuffer allocation failed");
        ptr->DeinitInternal();
        return AUDIO_ENGINE_ERR_INIT_FAILED;
    }

    ptr->initialized_ = true;

    LOG_INFO("AudioEngine_Bf_Init OK, sample_rate=%d, num_channels=%d, frame_len=%d, "
             "target_azimuth=%.3f, target_elevation=%.3f",
             init_config->sample_rate, init_config->num_channels,
             init_config->frame_len,
             ptr->rt_config_.target_azimuth, ptr->rt_config_.target_elevation);
    return AUDIO_ENGINE_SUCCESS;
}

extern "C" int AudioEngine_Bf_Deinit(BfHandle handle)
{
    LOG_DEBUG("AudioEngine_Bf_Deinit, handle=%p", (void*)handle);

    if (!handle) {
        LOG_WARN("AudioEngine_Bf_Deinit: invalid handle (NULL)");
        return AUDIO_ENGINE_ERR_INVALID_HANDLE;
    }

    AudioEngineBf* ptr = static_cast<AudioEngineBf*>(handle);
    ptr->DeinitInternal();

    LOG_DEBUG("AudioEngine_Bf_Deinit OK");
    return AUDIO_ENGINE_SUCCESS;
}


/* ============================================================================
 * SetParam / ResetParam
 * ==========================================================================*/
extern "C" int AudioEngine_Bf_SetParam(BfHandle handle, const BfRtConfig* rt_config)
{
    LOG_DEBUG("AudioEngine_Bf_SetParam, handle=%p", (void*)handle);

    if (!handle) {
        LOG_WARN("AudioEngine_Bf_SetParam: invalid handle (NULL)");
        return AUDIO_ENGINE_ERR_INVALID_HANDLE;
    }
    if (!rt_config) {
        LOG_WARN("AudioEngine_Bf_SetParam: rt_config is NULL");
        return AUDIO_ENGINE_ERR_NULL_POINTER;
    }

    AudioEngineBf* ptr = static_cast<AudioEngineBf*>(handle);

    if (!ptr->initialized_) {
        LOG_WARN("AudioEngine_Bf_SetParam: not initialized");
        return AUDIO_ENGINE_ERR_NOT_INITIALIZED;
    }

    /* Incremental update: re-aim beamformer with new target direction */
    ptr->rt_config_.target_azimuth   = rt_config->target_azimuth;
    ptr->rt_config_.target_elevation = rt_config->target_elevation;

    SphericalPointf target_dir(ptr->rt_config_.target_azimuth,
                               ptr->rt_config_.target_elevation,
                               1.0f);
    ptr->bf_->AimAt(target_dir);

    LOG_INFO("AudioEngine_Bf_SetParam OK, azimuth=%.3f, elevation=%.3f",
             rt_config->target_azimuth, rt_config->target_elevation);
    return AUDIO_ENGINE_SUCCESS;
}

extern "C" int AudioEngine_Bf_ResetParam(BfHandle handle, const BfRtConfig* rt_config)
{
    LOG_DEBUG("AudioEngine_Bf_ResetParam, handle=%p", (void*)handle);

    if (!handle) {
        LOG_WARN("AudioEngine_Bf_ResetParam: invalid handle (NULL)");
        return AUDIO_ENGINE_ERR_INVALID_HANDLE;
    }
    if (!rt_config) {
        LOG_WARN("AudioEngine_Bf_ResetParam: rt_config is NULL");
        return AUDIO_ENGINE_ERR_NULL_POINTER;
    }

    AudioEngineBf* ptr = static_cast<AudioEngineBf*>(handle);

    if (!ptr->initialized_) {
        LOG_WARN("AudioEngine_Bf_ResetParam: not initialized");
        return AUDIO_ENGINE_ERR_NOT_INITIALIZED;
    }

    /* Full reset: back to Init defaults, then apply new config */
    ptr->rt_config_.target_azimuth   = ptr->init_config_.target_azimuth;
    ptr->rt_config_.target_elevation = ptr->init_config_.target_elevation;

    ptr->rt_config_.target_azimuth   = rt_config->target_azimuth;
    ptr->rt_config_.target_elevation = rt_config->target_elevation;

    SphericalPointf target_dir(ptr->rt_config_.target_azimuth,
                               ptr->rt_config_.target_elevation,
                               1.0f);
    ptr->bf_->AimAt(target_dir);

    LOG_INFO("AudioEngine_Bf_ResetParam OK, azimuth=%.3f, elevation=%.3f",
             rt_config->target_azimuth, rt_config->target_elevation);
    return AUDIO_ENGINE_SUCCESS;
}


/* ============================================================================
 * Process
 * ==========================================================================*/
extern "C" int AudioEngine_Bf_Process(
    BfHandle handle,
    const short* audio_in,
    int in_samples,
    short* audio_out,
    int* is_target_present)
{
    if (!handle) {
        LOG_WARN("AudioEngine_Bf_Process: invalid handle (NULL)");
        return AUDIO_ENGINE_ERR_INVALID_HANDLE;
    }
    if (!audio_in) {
        LOG_WARN("AudioEngine_Bf_Process: audio_in is NULL");
        return AUDIO_ENGINE_ERR_NULL_POINTER;
    }
    if (!audio_out) {
        LOG_WARN("AudioEngine_Bf_Process: audio_out is NULL");
        return AUDIO_ENGINE_ERR_NULL_POINTER;
    }

    AudioEngineBf* ptr = static_cast<AudioEngineBf*>(handle);

    if (!ptr->initialized_) {
        LOG_WARN("AudioEngine_Bf_Process: not initialized");
        return AUDIO_ENGINE_ERR_NOT_INITIALIZED;
    }

    const int expected_samples = ptr->init_config_.frame_len;
    if (in_samples != expected_samples) {
        LOG_WARN("AudioEngine_Bf_Process: in_samples=%d, expected %d (10ms constraint)",
                 in_samples, expected_samples);
        return AUDIO_ENGINE_ERR_INVALID_PARAM;
    }

    const int num_channels  = ptr->init_config_.num_channels;
    const int num_frames    = in_samples;
    const int total_in      = num_frames * num_channels;

    /* 1. Convert interleaved short → float (in-place on temp buffer) */
    std::vector<float> float_in(total_in);
    S16ToFloat(audio_in, total_in, float_in.data());

    /* 2. Deinterleave into ChannelBuffer */
    Deinterleave(float_in.data(), num_frames, num_channels,
                 ptr->in_buf_->channels());

    /* 3. ProcessChunk: multi-channel in, single-channel out */
    ptr->bf_->ProcessChunk(*ptr->in_buf_, ptr->out_buf_);

    /* 4. Interleave the single output channel → float buffer */
    std::vector<float> float_out(num_frames);
    const float* const* out_channels = ptr->out_buf_->channels();
    for (int i = 0; i < num_frames; ++i) {
        float_out[i] = out_channels[0][i];
    }

    /* 5. Convert float → short */
    FloatToS16(float_out.data(), num_frames, audio_out);

    /* 6. Target presence (optional output) */
    if (is_target_present) {
        *is_target_present = ptr->bf_->is_target_present() ? 1 : 0;
    }

    return AUDIO_ENGINE_SUCCESS;
}


/* ============================================================================
 * Reset
 * ==========================================================================*/
extern "C" int AudioEngine_Bf_Reset(BfHandle handle)
{
    LOG_DEBUG("AudioEngine_Bf_Reset, handle=%p", (void*)handle);

    if (!handle) {
        LOG_WARN("AudioEngine_Bf_Reset: invalid handle (NULL)");
        return AUDIO_ENGINE_ERR_INVALID_HANDLE;
    }

    AudioEngineBf* ptr = static_cast<AudioEngineBf*>(handle);

    if (!ptr->initialized_) {
        LOG_WARN("AudioEngine_Bf_Reset: not initialized");
        return AUDIO_ENGINE_ERR_NOT_INITIALIZED;
    }

    /* Destroy and recreate beamformer instance; keep all config */
    if (ptr->bf_) {
        delete ptr->bf_;
        ptr->bf_ = nullptr;
    }

    SphericalPointf target_dir(ptr->rt_config_.target_azimuth,
                               ptr->rt_config_.target_elevation,
                               1.0f);
    ptr->bf_ = new (std::nothrow) NonlinearBeamformer(ptr->mic_positions_, target_dir);
    if (!ptr->bf_) {
        LOG_ERROR("AudioEngine_Bf_Reset: NonlinearBeamformer allocation failed");
        ptr->initialized_ = false;
        return AUDIO_ENGINE_ERR_PROCESS_FAILED;
    }

    ptr->bf_->Initialize(/*chunk_size_ms=*/10, ptr->init_config_.sample_rate);

    /* initialized_ stays true, configs preserved */
    LOG_INFO("AudioEngine_Bf_Reset OK, azimuth=%.3f, elevation=%.3f",
             ptr->rt_config_.target_azimuth, ptr->rt_config_.target_elevation);
    return AUDIO_ENGINE_SUCCESS;
}
