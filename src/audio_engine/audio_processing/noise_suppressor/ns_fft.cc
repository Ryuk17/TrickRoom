/*
 *  Copyright (c) 2019 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "audio_processing/noise_suppressor/ns_fft.h"

#include <array>
#include <cstddef>
#include <cstring>

#include "utils/array_view.h"
#include "NE10_types.h"
#include "NE10_macros.h"
#include "NE10_fft.h"
#include "audio_processing/noise_suppressor/ns_common.h"

// Forward declarations of NE10 functions used directly
extern "C" {
ne10_fft_r2c_cfg_float32_t ne10_fft_alloc_r2c_float32(ne10_int32_t nfft);
void ne10_fft_r2c_1d_float32_c(ne10_fft_cpx_float32_t* fout,
                                ne10_float32_t* fin,
                                ne10_fft_r2c_cfg_float32_t cfg);
void ne10_fft_c2r_1d_float32_c(ne10_float32_t* fout,
                                ne10_fft_cpx_float32_t* fin,
                                ne10_fft_r2c_cfg_float32_t cfg);
void ne10_fft_destroy_r2c_float32(ne10_fft_r2c_cfg_float32_t cfg);
}

namespace webrtc {

NrFft::NrFft() {
  fft_cfg_ = ne10_fft_alloc_r2c_float32(kFftSize);
  fft_tmp_in_ = new float[kFftSize];
  fft_tmp_out_ = new float[(kFftSize / 2 + 1) * 2];
  // Warm-up call to initialize twiddle tables.
  std::memset(fft_tmp_in_, 0, kFftSize * sizeof(float));
  ne10_fft_r2c_1d_float32_c(
      reinterpret_cast<ne10_fft_cpx_float32_t*>(fft_tmp_out_),
      fft_tmp_in_, static_cast<ne10_fft_r2c_cfg_float32_t>(fft_cfg_));
}

NrFft::~NrFft() {
  if (fft_cfg_) {
    ne10_fft_destroy_r2c_float32(
        static_cast<ne10_fft_r2c_cfg_float32_t>(fft_cfg_));
  }
  delete[] fft_tmp_in_;
  delete[] fft_tmp_out_;
}

void NrFft::Fft(ArrayView<float, kFftSize> time_data,
                ArrayView<float, kFftSize> real,
                ArrayView<float, kFftSize> imag) {
  const size_t ncfft = kFftSize / 2;
  auto* cfg = static_cast<ne10_fft_r2c_cfg_float32_t>(fft_cfg_);
  auto* fout = reinterpret_cast<ne10_fft_cpx_float32_t*>(fft_tmp_out_);

  // Copy input and run NE10 real FFT (unscaled forward transform).
  std::memcpy(fft_tmp_in_, time_data.data(), kFftSize * sizeof(float));
  ne10_fft_r2c_1d_float32_c(fout, fft_tmp_in_, cfg);

  // Extract real/imag from NE10 packed output.
  imag[0] = 0;
  real[0] = fout[0].r;
  imag[kFftSizeBy2Plus1 - 1] = 0;
  real[kFftSizeBy2Plus1 - 1] = fout[ncfft].r;
  for (size_t i = 1; i < kFftSizeBy2Plus1 - 1; ++i) {
    real[i] = fout[i].r;
    imag[i] = fout[i].i;
  }
}

void NrFft::Ifft(ArrayView<const float> real,
                 ArrayView<const float> imag,
                 ArrayView<float> time_data) {
  const size_t ncfft = kFftSize / 2;
  auto* cfg = static_cast<ne10_fft_r2c_cfg_float32_t>(fft_cfg_);
  auto* fin = reinterpret_cast<ne10_fft_cpx_float32_t*>(fft_tmp_out_);

  // Build NE10 packed complex input from real/imag.
  fin[0].r = real[0];
  fin[0].i = 0;
  for (size_t i = 1; i < kFftSizeBy2Plus1 - 1; ++i) {
    fin[i].r = real[i];
    fin[i].i = imag[i];
  }
  fin[ncfft].r = real[kFftSizeBy2Plus1 - 1];
  fin[ncfft].i = 0;

  // Run NE10 complex-to-real IFFT. NE10 r2c/c2r are unscaled (the 1/N
  // scaling in the radix butterflies is gated behind NE10_DSP_RFFT_SCALING,
  // which is not defined), so the round trip is the identity -- same as the
  // WebRTC Ooura path (WebRtc_rdft(-1) + 2/N also yields the identity).
  ne10_fft_c2r_1d_float32_c(fft_tmp_in_, fin, cfg);
  std::memcpy(time_data.data(), fft_tmp_in_, kFftSize * sizeof(float));
}

}  // namespace webrtc
