/*
 *  Copyright (c) 2019 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef MODULES_AUDIO_PROCESSING_NS_NS_FFT_H_
#define MODULES_AUDIO_PROCESSING_NS_NS_FFT_H_

#include <cstddef>

#include "utils/array_view.h"
#include "audio_processing/noise_suppressor/ns_common.h"

namespace webrtc {

// Wrapper class providing 256 point FFT functionality.
// Uses the NE10 real FFT implementation.
class NrFft {
 public:
  NrFft();
  ~NrFft();
  NrFft(const NrFft&) = delete;
  NrFft& operator=(const NrFft&) = delete;

  // Transforms the signal from time to frequency domain.
  void Fft(ArrayView<float, kFftSize> time_data,
           ArrayView<float, kFftSize> real,
           ArrayView<float, kFftSize> imag);

  // Transforms the signal from frequency to time domain.
  void Ifft(ArrayView<const float> real,
            ArrayView<const float> imag,
            ArrayView<float> time_data);

 private:
  // NE10 FFT state
  void* fft_cfg_;       // ne10_fft_r2c_cfg_float32_t
  float* fft_tmp_in_;   // kFftSize floats
  float* fft_tmp_out_;  // (kFftSize/2 + 1) complex elements
};

}  // namespace webrtc

#endif  // MODULES_AUDIO_PROCESSING_NS_NS_FFT_H_
