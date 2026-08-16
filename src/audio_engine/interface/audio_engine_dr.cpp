/*
 * @Author: Ryuk
 * @Date: 2026-08-15
 * @Description: DR (Dereverberation) interface implementation
 */

#include <algorithm>
#include <new>
#include <stdexcept>
#include <vector>

#include "audio_engine_dr.h"
#include "audio_engine_log.h"

#include "audio_processing/dereverberation/dereverberation.h"


/* ============================================================================
 * Internal data container (pure data — C API operates directly on it)
 * ==========================================================================*/
class AudioEngineDr {
public:
    AudioEngineDr()
        : initialized_(false)
        , frame_len_(0)
        , dr_(nullptr) {
    }

    ~AudioEngineDr() {
        if (dr_) {
            delete dr_;
            dr_ = nullptr;
        }
    }

    bool initialized_;
    int frame_len_;
    DrInitConfig init_config_;
    dereverberation::DeReverberation* dr_;
};


/* ============================================================================
 * Config helpers
 * ==========================================================================*/
/* Fill the algorithm config from the C struct. Fields set to 0 (or outside
   the documented valid domain) fall back to the C++ defaults. */
static void BuildDrConfig(const DrInitConfig* ic,
                          dereverberation::DeReverberationConfig* cfg)
{
    if (ic->overlap_factor >= 2) {
        cfg->overlap_factor = ic->overlap_factor;
    }
    if (ic->frame_increment_s > 0.) {
        cfg->frame_increment_s = ic->frame_increment_s;
    }
    if (ic->round_frame_increment != 0) {
        cfg->round_frame_increment = true;
    }
    if (ic->spectral_gain_type >= 1 && ic->spectral_gain_type <= 3) {
        cfg->spectral_gain_type = ic->spectral_gain_type;
    }
    if (ic->gain_smoothing > 0. && ic->gain_smoothing <= 1.) {
        cfg->gain_smoothing = ic->gain_smoothing;
    }
    if (ic->gain_floor > 0.) {
        cfg->gain_floor = ic->gain_floor;
    }
    if (ic->oversubtraction >= 0.) {
        cfg->oversubtraction = ic->oversubtraction;
    }
    if (ic->num_states >= 2 && ic->num_states <= 6) {
        cfg->num_states = ic->num_states;
    }
    if (ic->posterior_mode >= 1 && ic->posterior_mode <= 2) {
        cfg->posterior_mode = ic->posterior_mode;
    }
    if (ic->energy_floor_db >= -200. && ic->energy_floor_db < 0.) {
        cfg->energy_floor_db = ic->energy_floor_db;
    }
    if (ic->clip_reference_db != 0.) {
        cfg->clip_reference_db = ic->clip_reference_db;
    }
}

static dereverberation::DeReverberation* CreateDrInstance(
    const DrInitConfig* init_config)
{
    dereverberation::DeReverberationConfig cfg;
    BuildDrConfig(init_config, &cfg);
    /* Constructor throws on config violating its hard constraints
       (sample_rate != 16000, frame_len * overlap_factor != 384). */
    return new (std::nothrow) dereverberation::DeReverberation(
        cfg, static_cast<size_t>(init_config->sample_rate));
}


/* ============================================================================
 * Create / Destroy
 * ==========================================================================*/
extern "C" DrHandle AudioEngine_Dr_Create(void)
{
    LOG_DEBUG("AudioEngine_Dr_Create");

    AudioEngineDr* ptr = new (std::nothrow) AudioEngineDr();
    if (!ptr) {
        LOG_ERROR("AudioEngine_Dr_Create: memory allocation failed");
    }
    return static_cast<DrHandle>(ptr);
}

extern "C" int AudioEngine_Dr_Destroy(DrHandle handle)
{
    LOG_DEBUG("AudioEngine_Dr_Destroy, handle=%p", (void*)handle);

    if (!handle) {
        LOG_WARN("AudioEngine_Dr_Destroy: invalid handle (NULL)");
        return AUDIO_ENGINE_ERR_INVALID_HANDLE;
    }

    AudioEngineDr* ptr = static_cast<AudioEngineDr*>(handle);
    delete ptr;

    return AUDIO_ENGINE_SUCCESS;
}


/* ============================================================================
 * Init / Deinit
 * ==========================================================================*/
extern "C" int AudioEngine_Dr_Init(DrHandle handle, const DrInitConfig* init_config)
{
    LOG_DEBUG("AudioEngine_Dr_Init, handle=%p", (void*)handle);

    if (!handle) {
        LOG_WARN("AudioEngine_Dr_Init: invalid handle (NULL)");
        return AUDIO_ENGINE_ERR_INVALID_HANDLE;
    }
    if (!init_config) {
        LOG_WARN("AudioEngine_Dr_Init: init_config is NULL");
        return AUDIO_ENGINE_ERR_NULL_POINTER;
    }
    if (init_config->sample_rate != 16000) {
        LOG_WARN("AudioEngine_Dr_Init: sample_rate=%d, only 16000 Hz supported",
                 init_config->sample_rate);
        return AUDIO_ENGINE_ERR_INVALID_PARAM;
    }

    AudioEngineDr* ptr = static_cast<AudioEngineDr*>(handle);

    /* If already initialized, clean up first */
    if (ptr->initialized_) {
        LOG_DEBUG("AudioEngine_Dr_Init: already initialized, deinit first");
        if (ptr->dr_) {
            delete ptr->dr_;
            ptr->dr_ = nullptr;
        }
    }

    ptr->init_config_ = *init_config;

    try {
        ptr->dr_ = CreateDrInstance(&ptr->init_config_);
    } catch (const std::exception& e) {
        LOG_ERROR("AudioEngine_Dr_Init: DeReverberation construction failed: %s",
                  e.what());
        ptr->dr_ = nullptr;
        ptr->initialized_ = false;
        return AUDIO_ENGINE_ERR_INVALID_PARAM;
    }
    if (!ptr->dr_) {
        LOG_ERROR("AudioEngine_Dr_Init: DeReverberation allocation failed");
        ptr->initialized_ = false;
        return AUDIO_ENGINE_ERR_INIT_FAILED;
    }

    ptr->frame_len_ = static_cast<int>(ptr->dr_->frame_increment());
    ptr->initialized_ = true;

    LOG_INFO("AudioEngine_Dr_Init OK, sample_rate=%d, frame_len=%d, "
             "overlap=%d, num_states=%d, gain_type=%d",
             init_config->sample_rate, ptr->frame_len_,
             ptr->init_config_.overlap_factor, ptr->init_config_.num_states,
             ptr->init_config_.spectral_gain_type);
    return AUDIO_ENGINE_SUCCESS;
}

extern "C" int AudioEngine_Dr_Deinit(DrHandle handle)
{
    LOG_DEBUG("AudioEngine_Dr_Deinit, handle=%p", (void*)handle);

    if (!handle) {
        LOG_WARN("AudioEngine_Dr_Deinit: invalid handle (NULL)");
        return AUDIO_ENGINE_ERR_INVALID_HANDLE;
    }

    AudioEngineDr* ptr = static_cast<AudioEngineDr*>(handle);

    if (ptr->dr_) {
        delete ptr->dr_;
        ptr->dr_ = nullptr;
    }
    ptr->initialized_ = false;

    LOG_DEBUG("AudioEngine_Dr_Deinit OK");
    return AUDIO_ENGINE_SUCCESS;
}


/* ============================================================================
 * Process
 * ==========================================================================*/
extern "C" int AudioEngine_Dr_Process(
    DrHandle handle,
    const short* audio_in,
    int in_samples,
    short* audio_out)
{
    if (!handle) {
        LOG_WARN("AudioEngine_Dr_Process: invalid handle (NULL)");
        return AUDIO_ENGINE_ERR_INVALID_HANDLE;
    }
    if (!audio_in) {
        LOG_WARN("AudioEngine_Dr_Process: audio_in is NULL");
        return AUDIO_ENGINE_ERR_NULL_POINTER;
    }
    if (!audio_out) {
        LOG_WARN("AudioEngine_Dr_Process: audio_out is NULL");
        return AUDIO_ENGINE_ERR_NULL_POINTER;
    }

    AudioEngineDr* ptr = static_cast<AudioEngineDr*>(handle);

    if (!ptr->initialized_) {
        LOG_WARN("AudioEngine_Dr_Process: not initialized");
        return AUDIO_ENGINE_ERR_NOT_INITIALIZED;
    }

    if (in_samples != ptr->frame_len_) {
        LOG_WARN("AudioEngine_Dr_Process: in_samples=%d, expected frame_len=%d",
                 in_samples, ptr->frame_len_);
        return AUDIO_ENGINE_ERR_INVALID_PARAM;
    }

    /* short -> float, matching the internal test exactly (/32768.f) */
    std::vector<float> in_f(static_cast<size_t>(in_samples));
    for (int i = 0; i < in_samples; ++i) {
        in_f[i] = static_cast<float>(audio_in[i]) / 32768.f;
    }

    std::vector<float> out_f(static_cast<size_t>(in_samples));
    ptr->dr_->Process(in_f.data(), out_f.data());

    /* float -> short, matching the internal test exactly (clamp + cast) */
    for (int i = 0; i < in_samples; ++i) {
        const float v = out_f[i] * 32768.f;
        audio_out[i] = static_cast<short>(
            std::max(-32768.f, std::min(32767.f, v)));
    }

    return AUDIO_ENGINE_SUCCESS;
}


/* ============================================================================
 * Reset
 * ==========================================================================*/
extern "C" int AudioEngine_Dr_Reset(DrHandle handle)
{
    LOG_DEBUG("AudioEngine_Dr_Reset, handle=%p", (void*)handle);

    if (!handle) {
        LOG_WARN("AudioEngine_Dr_Reset: invalid handle (NULL)");
        return AUDIO_ENGINE_ERR_INVALID_HANDLE;
    }

    AudioEngineDr* ptr = static_cast<AudioEngineDr*>(handle);

    if (!ptr->initialized_) {
        LOG_WARN("AudioEngine_Dr_Reset: not initialized");
        return AUDIO_ENGINE_ERR_NOT_INITIALIZED;
    }

    /* Destroy and recreate the dereverberator; keep init config */
    if (ptr->dr_) {
        delete ptr->dr_;
        ptr->dr_ = nullptr;
    }

    try {
        ptr->dr_ = CreateDrInstance(&ptr->init_config_);
    } catch (const std::exception& e) {
        LOG_ERROR("AudioEngine_Dr_Reset: DeReverberation construction failed: %s",
                  e.what());
        ptr->dr_ = nullptr;
        ptr->initialized_ = false;
        return AUDIO_ENGINE_ERR_PROCESS_FAILED;
    }
    if (!ptr->dr_) {
        LOG_ERROR("AudioEngine_Dr_Reset: DeReverberation allocation failed");
        ptr->initialized_ = false;
        return AUDIO_ENGINE_ERR_PROCESS_FAILED;
    }

    ptr->frame_len_ = static_cast<int>(ptr->dr_->frame_increment());

    /* initialized_ stays true, config preserved */
    LOG_INFO("AudioEngine_Dr_Reset OK, frame_len=%d", ptr->frame_len_);
    return AUDIO_ENGINE_SUCCESS;
}
