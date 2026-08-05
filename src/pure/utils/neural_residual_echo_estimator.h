/*
 * Minimal NeuralResidualEchoEstimator stub for the pure build.
 */
#ifndef API_AUDIO_NEURAL_RESIDUAL_ECHO_ESTIMATOR_H_
#define API_AUDIO_NEURAL_RESIDUAL_ECHO_ESTIMATOR_H_

#include <array>

#include "utils/array_view.h"
#include "utils/echo_canceller3_config.h"

namespace webrtc {

class AudioBuffer;

class NeuralResidualEchoEstimator {
 public:
  virtual ~NeuralResidualEchoEstimator() = default;
  virtual void Initialize(int /*sample_rate_hz*/,
                          int /*num_render_channels*/,
                          int /*num_capture_channels*/) {}
  virtual void AnalyzeRender(const AudioBuffer& /*render*/) {}
  virtual void AnalyzeCapture(const AudioBuffer& /*capture*/) {}
  virtual void ProcessCapture(AudioBuffer* /*capture*/,
                              bool /*level_change*/) {}
  virtual void Estimate(ArrayView<const float> /*x*/,
                        ArrayView<const std::array<float, 64>> /*y*/,
                        ArrayView<const std::array<float, 64>> /*e*/,
                        ArrayView<const std::array<float, 65>> /*S2*/,
                        ArrayView<const std::array<float, 65>> /*Y2*/,
                        ArrayView<const std::array<float, 65>> /*E2*/,
                        ArrayView<std::array<float, 65>> /*R2*/,
                        ArrayView<std::array<float, 65>> /*R2_unbounded*/) {}
  virtual EchoCanceller3Config GetConfiguration(bool /*multi_channel*/) const {
    return EchoCanceller3Config();
  }
};

}  // namespace webrtc

#endif  // API_AUDIO_NEURAL_RESIDUAL_ECHO_ESTIMATOR_H_
