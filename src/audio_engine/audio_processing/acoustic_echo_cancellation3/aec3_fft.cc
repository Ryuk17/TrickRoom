/*
 *  Copyright (c) 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "audio_processing/acoustic_echo_cancellation3/aec3_fft.h"

#include <algorithm>
#include <array>
#include <cstring>
#include <functional>
#include <iterator>

#include "utils/array_view.h"
#include "audio_processing/acoustic_echo_cancellation3/aec3_common.h"
#include "audio_processing/acoustic_echo_cancellation3/fft_data.h"
#include "utils/checks.h"

// NE10 FFT library headers
#include "NE10_types.h"
#include "NE10_macros.h"
#include "NE10_fft.h"

// Forward-declare NE10 real FFT functions (call the generic C versions directly,
// bypassing ne10_init() function-pointer dispatch).
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

namespace {

// Hanning window (64-point) for ZeroPaddedFft.
const float kHanning64[kFftLengthBy2] = {
    0.f,         0.00248461f, 0.00991376f, 0.0222136f,  0.03926189f,
    0.06088921f, 0.08688061f, 0.11697778f, 0.15088159f, 0.1882551f,
    0.22872687f, 0.27189467f, 0.31732949f, 0.36457977f, 0.41317591f,
    0.46263495f, 0.51246535f, 0.56217185f, 0.61126047f, 0.65924333f,
    0.70564355f, 0.75f,       0.79187184f, 0.83084292f, 0.86652594f,
    0.89856625f, 0.92664544f, 0.95048443f, 0.96984631f, 0.98453864f,
    0.99441541f, 0.99937846f, 0.99937846f, 0.99441541f, 0.98453864f,
    0.96984631f, 0.95048443f, 0.92664544f, 0.89856625f, 0.86652594f,
    0.83084292f, 0.79187184f, 0.75f,       0.70564355f, 0.65924333f,
    0.61126047f, 0.56217185f, 0.51246535f, 0.46263495f, 0.41317591f,
    0.36457977f, 0.31732949f, 0.27189467f, 0.22872687f, 0.1882551f,
    0.15088159f, 0.11697778f, 0.08688061f, 0.06088921f, 0.03926189f,
    0.0222136f,  0.00991376f, 0.00248461f, 0.f};

// Sqrt(Hanning) window (128-point) from Matlab: win = sqrt(hanning(128)).
const float kSqrtHanning128[kFftLength] = {
    0.00000000000000f, 0.02454122852291f, 0.04906767432742f, 0.07356456359967f,
    0.09801714032956f, 0.12241067519922f, 0.14673047445536f, 0.17096188876030f,
    0.19509032201613f, 0.21910124015687f, 0.24298017990326f, 0.26671275747490f,
    0.29028467725446f, 0.31368174039889f, 0.33688985339222f, 0.35989503653499f,
    0.38268343236509f, 0.40524131400499f, 0.42755509343028f, 0.44961132965461f,
    0.47139673682600f, 0.49289819222978f, 0.51410274419322f, 0.53499761988710f,
    0.55557023301960f, 0.57580819141785f, 0.59569930449243f, 0.61523159058063f,
    0.63439328416365f, 0.65317284295378f, 0.67155895484702f, 0.68954054473707f,
    0.70710678118655f, 0.72424708295147f, 0.74095112535496f, 0.75720884650648f,
    0.77301045336274f, 0.78834642762661f, 0.80320753148064f, 0.81758481315158f,
    0.83146961230255f, 0.84485356524971f, 0.85772861000027f, 0.87008699110871f,
    0.88192126434835f, 0.89322430119552f, 0.90398929312344f, 0.91420975570353f,
    0.92387953251129f, 0.93299279883474f, 0.94154406518302f, 0.94952818059304f,
    0.95694033573221f, 0.96377606579544f, 0.97003125319454f, 0.97570213003853f,
    0.98078528040323f, 0.98527764238894f, 0.98917650996478f, 0.99247953459871f,
    0.99518472667220f, 0.99729045667869f, 0.99879545620517f, 0.99969881869620f,
    1.00000000000000f, 0.99969881869620f, 0.99879545620517f, 0.99729045667869f,
    0.99518472667220f, 0.99247953459871f, 0.98917650996478f, 0.98527764238894f,
    0.98078528040323f, 0.97570213003853f, 0.97003125319454f, 0.96377606579544f,
    0.95694033573221f, 0.94952818059304f, 0.94154406518302f, 0.93299279883474f,
    0.92387953251129f, 0.91420975570353f, 0.90398929312344f, 0.89322430119552f,
    0.88192126434835f, 0.87008699110871f, 0.85772861000027f, 0.84485356524971f,
    0.83146961230255f, 0.81758481315158f, 0.80320753148064f, 0.78834642762661f,
    0.77301045336274f, 0.75720884650648f, 0.74095112535496f, 0.72424708295147f,
    0.70710678118655f, 0.68954054473707f, 0.67155895484702f, 0.65317284295378f,
    0.63439328416365f, 0.61523159058063f, 0.59569930449243f, 0.57580819141785f,
    0.55557023301960f, 0.53499761988710f, 0.51410274419322f, 0.49289819222978f,
    0.47139673682600f, 0.44961132965461f, 0.42755509343028f, 0.40524131400499f,
    0.38268343236509f, 0.35989503653499f, 0.33688985339222f, 0.31368174039889f,
    0.29028467725446f, 0.26671275747490f, 0.24298017990326f, 0.21910124015687f,
    0.19509032201613f, 0.17096188876030f, 0.14673047445536f, 0.12241067519922f,
    0.09801714032956f, 0.07356456359967f, 0.04906767432742f, 0.02454122852291f};

// NE10 c2r output must be scaled by kFftLengthBy2 (= 64) to match the Ooura
// InverseFft convention.  (Calibrated via fft_calibrate.cc: Ooura round-trip
// multiplies the input by 64, NE10 round-trip is identity, so NE10 c2r × 64
// equals Ooura InverseFft.)
constexpr float kIfftScale = static_cast<float>(kFftLengthBy2);

}  // namespace

Aec3Fft::Aec3Fft()
    : ne10_fft_cfg_(ne10_fft_alloc_r2c_float32(kFftLength)) {
  // Warm-up: run a zero-input FFT to initialise twiddle tables inside NE10.
  std::array<float, kFftLength> zeros{};
  ne10_fft_cpx_float32_t tmp[kFftLengthBy2Plus1];
  ne10_fft_r2c_1d_float32_c(tmp, zeros.data(),
      static_cast<ne10_fft_r2c_cfg_float32_t>(ne10_fft_cfg_));
}

Aec3Fft::~Aec3Fft() {
  if (ne10_fft_cfg_) {
    ne10_fft_destroy_r2c_float32(
        static_cast<ne10_fft_r2c_cfg_float32_t>(ne10_fft_cfg_));
  }
}

void Aec3Fft::Fft(std::array<float, kFftLength>* x, FftData* X) const {
  RTC_DCHECK(x);
  RTC_DCHECK(X);
  ne10_fft_cpx_float32_t fout[kFftLengthBy2Plus1];
  ne10_fft_r2c_1d_float32_c(fout, x->data(),
      static_cast<ne10_fft_r2c_cfg_float32_t>(ne10_fft_cfg_));
  // NE10 r2c output layout: fout[0]={DC_re, 0}, fout[1..63]={re, im},
  // fout[64]={Nyquist_re, 0}.
  // NE10 uses the standard e^(-j) DFT convention, whereas Ooura uses e^(+j).
  // To match Ooura's convention (which the rest of AEC3 expects), conjugate
  // (negate the imaginary part).
  X->re[0] = fout[0].r;
  X->im[0] = 0;
  X->re[kFftLengthBy2] = fout[kFftLengthBy2].r;
  X->im[kFftLengthBy2] = 0;
  for (size_t k = 1; k < kFftLengthBy2; ++k) {
    X->re[k] = fout[k].r;
    X->im[k] = -fout[k].i;
  }
}

void Aec3Fft::Ifft(const FftData& X, std::array<float, kFftLength>* x) const {
  RTC_DCHECK(x);
  ne10_fft_cpx_float32_t fin[kFftLengthBy2Plus1];
  fin[0].r = X.re[0];
  fin[0].i = 0;
  fin[kFftLengthBy2].r = X.re[kFftLengthBy2];
  fin[kFftLengthBy2].i = 0;
  // NE10 uses e^(-j) convention; FftData stores Ooura e^(+j) convention.
  // Conjugate (negate imag) to convert.
  for (size_t k = 1; k < kFftLengthBy2; ++k) {
    fin[k].r = X.re[k];
    fin[k].i = -X.im[k];
  }
  ne10_fft_c2r_1d_float32_c(x->data(), fin,
      static_cast<ne10_fft_r2c_cfg_float32_t>(ne10_fft_cfg_));
  // Scale NE10 c2r output to match Ooura InverseFft convention.
  for (auto& v : *x)
    v *= kIfftScale;
}

void Aec3Fft::ZeroPaddedFft(ArrayView<const float> x,
                            Window window,
                            FftData* X) const {
  RTC_DCHECK(X);
  RTC_DCHECK_EQ(kFftLengthBy2, x.size());
  std::array<float, kFftLength> fft;
  std::fill(fft.begin(), fft.begin() + kFftLengthBy2, 0.f);
  switch (window) {
    case Window::kRectangular:
      std::copy(x.begin(), x.end(), fft.begin() + kFftLengthBy2);
      break;
    case Window::kHanning:
      std::transform(x.begin(), x.end(), std::begin(kHanning64),
                     fft.begin() + kFftLengthBy2,
                     [](float a, float b) { return a * b; });
      break;
    case Window::kSqrtHanning:
      RTC_DCHECK_NOTREACHED();
      break;
    default:
      RTC_DCHECK_NOTREACHED();
  }

  Fft(&fft, X);
}

void Aec3Fft::PaddedFft(ArrayView<const float> x,
                        ArrayView<const float> x_old,
                        Window window,
                        FftData* X) const {
  RTC_DCHECK(X);
  RTC_DCHECK_EQ(kFftLengthBy2, x.size());
  RTC_DCHECK_EQ(kFftLengthBy2, x_old.size());
  std::array<float, kFftLength> fft;

  switch (window) {
    case Window::kRectangular:
      std::copy(x_old.begin(), x_old.end(), fft.begin());
      std::copy(x.begin(), x.end(), fft.begin() + x_old.size());
      break;
    case Window::kHanning:
      RTC_DCHECK_NOTREACHED();
      break;
    case Window::kSqrtHanning:
      std::transform(x_old.begin(), x_old.end(), std::begin(kSqrtHanning128),
                     fft.begin(), std::multiplies<float>());
      std::transform(x.begin(), x.end(),
                     std::begin(kSqrtHanning128) + x_old.size(),
                     fft.begin() + x_old.size(), std::multiplies<float>());
      break;
    default:
      RTC_DCHECK_NOTREACHED();
  }

  Fft(&fft, X);
}

}  // namespace webrtc
