/*
 * Minimal EchoControl interface for the pure build.
 */
#ifndef API_AUDIO_ECHO_CONTROL_H_
#define API_AUDIO_ECHO_CONTROL_H_

#include <memory>

#include "audio_processing/aec3/neural_residual_echo_estimator.h"

namespace webrtc {

class AudioBuffer;

class EchoControl {
 public:
  virtual void AnalyzeRender(AudioBuffer* render) = 0;
  virtual void AnalyzeCapture(AudioBuffer* capture) = 0;
  virtual void ProcessCapture(AudioBuffer* capture, bool level_change) = 0;
  virtual void ProcessCapture(AudioBuffer* capture,
                              AudioBuffer* linear_output,
                              bool level_change) = 0;

  struct Metrics {
    double echo_return_loss;
    double echo_return_loss_enhancement;
    int delay_ms;
  };

  virtual Metrics GetMetrics() const = 0;
  virtual void SetAudioBufferDelay(int delay_ms) = 0;
  virtual void SetCaptureOutputUsage(bool /* capture_output_used */) {}
  virtual bool ActiveProcessing() const = 0;

  virtual ~EchoControl() {}
};

class EchoControlFactory {
 public:
  virtual ~EchoControlFactory() = default;

  virtual std::unique_ptr<EchoControl> Create(
      int sample_rate_hz,
      int num_render_channels,
      int num_capture_channels) = 0;

  virtual std::unique_ptr<EchoControl> Create(
      int sample_rate_hz,
      int num_render_channels,
      int num_capture_channels,
      NeuralResidualEchoEstimator* /*neural_residual_echo_estimator*/) {
    return Create(sample_rate_hz, num_render_channels,
                  num_capture_channels);
  }
};

}  // namespace webrtc

#endif  // API_AUDIO_ECHO_CONTROL_H_
