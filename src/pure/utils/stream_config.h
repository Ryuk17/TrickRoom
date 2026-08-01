/*
 * Minimal StreamConfig class for the pure build.
 * Extracted from api/audio/audio_processing.h to avoid heavy dependencies.
 */
#ifndef API_AUDIO_STREAM_CONFIG_H_
#define API_AUDIO_STREAM_CONFIG_H_

#include <stddef.h>

namespace webrtc {

class StreamConfig {
 public:
  // sample_rate_hz: The sampling rate of the stream.
  // num_channels: The number of audio channels in the stream.
  StreamConfig(int sample_rate_hz = 0,
               size_t num_channels = 0)  // NOLINT(runtime/explicit)
      : sample_rate_hz_(sample_rate_hz),
        num_channels_(num_channels),
        num_frames_(calculate_frames(sample_rate_hz)) {}

  void set_sample_rate_hz(int value) {
    sample_rate_hz_ = value;
    num_frames_ = calculate_frames(value);
  }
  void set_num_channels(size_t value) { num_channels_ = value; }

  int sample_rate_hz() const { return sample_rate_hz_; }

  // The number of channels in the stream.
  size_t num_channels() const { return num_channels_; }

  size_t num_frames() const { return num_frames_; }
  size_t num_samples() const { return num_channels_ * num_frames_; }

  bool operator==(const StreamConfig& other) const {
    return sample_rate_hz_ == other.sample_rate_hz_ &&
           num_channels_ == other.num_channels_;
  }

  bool operator!=(const StreamConfig& other) const { return !(*this == other); }

 private:
  // APM processes audio in chunks of about 10 ms.
  static size_t calculate_frames(int sample_rate_hz) {
    return static_cast<size_t>(sample_rate_hz / 100);
  }

  int sample_rate_hz_;
  size_t num_channels_;
  size_t num_frames_;
};

}  // namespace webrtc

#endif  // API_AUDIO_STREAM_CONFIG_H_
