/*
 *  Copyright (c) 2014 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

// Adapted for audio_engine: uses NE10 R2C/C2R FFT directly (no Ooura/OpenMAX).

#include "signal_processing/real_fourier.h"

#include <cstdlib>

#include "NE10_dsp.h"
#include "utils/checks.h"
#include "signal_processing/spl_inl.h"

namespace webrtc {

using std::complex;

const size_t RealFourier::kFftBufferAlignment = 32;

// ---------------------------------------------------------------------------
// Internal NE10-based RealFourier implementation (no separate header)
// ---------------------------------------------------------------------------
class RealFourierImpl : public RealFourier {
 public:
  explicit RealFourierImpl(int fft_order)
      : order_(fft_order)
      , fft_size_(1 << fft_order)
      , complex_length_(fft_size_ / 2 + 1)
      , cfg_(nullptr) {
    RTC_CHECK_GE(fft_order, 1);
    cfg_ = ne10_fft_alloc_r2c_float32(fft_size_);
    RTC_CHECK(cfg_) << "ne10_fft_alloc_r2c_float32 failed for size " << fft_size_;
  }

  ~RealFourierImpl() override {
    if (cfg_) {
      ne10_fft_destroy_r2c_float32(cfg_);
    }
  }

  void Forward(const float* src, complex<float>* dest) const override {
    // NE10 R2C FFT: real input → complex output.
    // ne10_fft_cpx_float32_t has same layout as std::complex<float>.
    ne10_fft_r2c_1d_float32_c(
        reinterpret_cast<ne10_fft_cpx_float32_t*>(dest),
        const_cast<ne10_float32_t*>(src),
        cfg_);
  }

  void Inverse(const complex<float>* src, float* dest) const override {
    // NE10 C2R IFFT: complex input → real output.
    ne10_fft_c2r_1d_float32_c(
        dest,
        const_cast<ne10_fft_cpx_float32_t*>(
            reinterpret_cast<const ne10_fft_cpx_float32_t*>(src)),
        cfg_);
  }

  int order() const override { return order_; }

 private:
  int order_;
  int fft_size_;
  int complex_length_;
  ne10_fft_r2c_cfg_float32_t cfg_;
};

// ---------------------------------------------------------------------------
// Factory / static helpers
// ---------------------------------------------------------------------------
rtc::scoped_ptr<RealFourier> RealFourier::Create(int fft_order) {
  return rtc::scoped_ptr<RealFourier>(new RealFourierImpl(fft_order));
}

int RealFourier::FftOrder(size_t length) {
  RTC_CHECK_GT(length, 0U);
  return WebRtcSpl_GetSizeInBits(static_cast<uint32_t>(length - 1));
}

size_t RealFourier::FftLength(int order) {
  RTC_CHECK_GE(order, 0);
  return static_cast<size_t>(1 << order);
}

size_t RealFourier::ComplexLength(int order) {
  return FftLength(order) / 2 + 1;
}

RealFourier::fft_real_scoper RealFourier::AllocRealBuffer(int count) {
  return fft_real_scoper(static_cast<float*>(
      AlignedMalloc(sizeof(float) * count, kFftBufferAlignment)));
}

RealFourier::fft_cplx_scoper RealFourier::AllocCplxBuffer(int count) {
  return fft_cplx_scoper(static_cast<complex<float>*>(
      AlignedMalloc(sizeof(complex<float>) * count, kFftBufferAlignment)));
}

}  // namespace webrtc
