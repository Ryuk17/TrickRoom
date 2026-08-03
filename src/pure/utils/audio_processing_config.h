/*
 * Minimal GainController2::AdaptiveDigital config for the pure build.
 * Extracted from api/audio/audio_processing.h to avoid heavy dependencies.
 */
#ifndef API_AUDIO_AUDIO_PROCESSING_CONFIG_H_
#define API_AUDIO_AUDIO_PROCESSING_CONFIG_H_

namespace webrtc {
namespace AudioProcessing {
namespace Config {
namespace GainController2 {

struct AdaptiveDigital {
  bool enabled = false;
  float headroom_db = 5.0f;
  float max_gain_db = 50.0f;
  float initial_gain_db = 15.0f;
  float max_gain_change_db_per_second = 6.0f;
  float max_output_noise_level_dbfs = -50.0f;
};

}  // namespace GainController2
}  // namespace Config
}  // namespace AudioProcessing
}  // namespace webrtc

#endif  // API_AUDIO_AUDIO_PROCESSING_CONFIG_H_
