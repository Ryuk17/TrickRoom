/*
 * Minimal AudioFrame stub for the pure build.
 * Only provides the constants needed by AGC2.
 */
#ifndef API_AUDIO_AUDIO_FRAME_H_
#define API_AUDIO_AUDIO_FRAME_H_

#include <cstddef>

namespace webrtc {

// Number of audio buffers (10 ms each) per second.
constexpr size_t kDefaultAudioBuffersPerSec = 100;

}  // namespace webrtc

#endif  // API_AUDIO_AUDIO_FRAME_H_
