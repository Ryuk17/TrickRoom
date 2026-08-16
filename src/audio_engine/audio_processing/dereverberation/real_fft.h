/*
 * Real FFT wrapper providing the one-sided spectrum convention used by
 * voicebox v_rfft/v_irfft (same as MATLAB fft/ifft):
 *   - forward : n real samples -> n/2+1 complex bins (DC .. Nyquist),
 *               unnormalized, identical to MATLAB fft output
 *   - inverse : n/2+1 complex bins -> n real samples, scaled by 1/n,
 *               identical to MATLAB ifft output
 *
 * Double precision (Eigen unsupported FFT module, pocketfft backend). The
 * dereverberation EKF is sensitive to observation precision; a float FFT
 * (e.g. pffft) makes the filter state drift measurably away from the
 * MATLAB double-precision reference.
 */

#ifndef DEREEVERBERATION_REAL_FFT_H_
#define DEREEVERBERATION_REAL_FFT_H_

#include <complex>
#include <cstddef>
#include <vector>

#include <unsupported/Eigen/FFT>

namespace dereverberation {

class RealFft {
 public:
  // n may be any size supported by the backend (pocketfft: mixed radix).
  explicit RealFft(size_t n);
  RealFft(const RealFft&) = delete;
  RealFft& operator=(const RealFft&) = delete;

  // Forward transform: n real samples -> n/2+1 complex one-sided spectrum.
  // X[0] is DC (real), X[n/2] is Nyquist (real for even n).
  void Fft(const double* x, std::complex<double>* X) const;

  // Inverse transform: n/2+1 complex one-sided spectrum -> n real samples.
  // Non-DC/Nyquist bins are assumed conjugate-symmetric.
  void Ifft(const std::complex<double>* X, double* x) const;

 private:
  size_t n_;
  size_t m_;  // n/2 + 1
  mutable Eigen::FFT<double> fft_;
  // Scratch for the inverse transform (avoids per-call allocation).
  mutable std::vector<std::complex<double>> full_;
  mutable std::vector<double> time_in_;
};

}  // namespace dereverberation

#endif  // DEREEVERBERATION_REAL_FFT_H_
