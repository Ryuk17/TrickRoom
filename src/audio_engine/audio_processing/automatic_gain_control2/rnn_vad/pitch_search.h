/*
 *  Copyright (c) 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef MODULES_AUDIO_PROCESSING_AGC2_RNN_VAD_PITCH_SEARCH_H_
#define MODULES_AUDIO_PROCESSING_AGC2_RNN_VAD_PITCH_SEARCH_H_

#include <vector>

#include "utils/array_view.h"
#include "audio_processing/automatic_gain_control2/cpu_features.h"
#include "audio_processing/automatic_gain_control2/rnn_vad/auto_correlation.h"
#include "audio_processing/automatic_gain_control2/rnn_vad/common.h"
#include "audio_processing/automatic_gain_control2/rnn_vad/pitch_search_internal.h"
#include "utils/gtest_prod_util.h"

namespace webrtc {
namespace rnn_vad {

// Pitch estimator.
class PitchEstimator {
 public:
  explicit PitchEstimator(const AvailableCpuFeatures& cpu_features);
  PitchEstimator(const PitchEstimator&) = delete;
  PitchEstimator& operator=(const PitchEstimator&) = delete;
  ~PitchEstimator();
  // Returns the estimated pitch period at 48 kHz.
  int Estimate(ArrayView<const float, kBufSize24kHz> pitch_buffer);

 private:
  FRIEND_TEST_ALL_PREFIXES(RnnVadTest, PitchSearchWithinTolerance);
  float GetLastPitchStrengthForTesting() const {
    return last_pitch_48kHz_.strength;
  }

  const AvailableCpuFeatures cpu_features_;
  PitchInfo last_pitch_48kHz_{};
  AutoCorrelationCalculator auto_corr_calculator_;
  std::vector<float> y_energy_24kHz_;
  std::vector<float> pitch_buffer_12kHz_;
  std::vector<float> auto_correlation_12kHz_;
};

}  // namespace rnn_vad
}  // namespace webrtc

#endif  // MODULES_AUDIO_PROCESSING_AGC2_RNN_VAD_PITCH_SEARCH_H_
