#include "dereverberation/real_fft.h"

#include <algorithm>
#include <cmath>
#include <complex>

namespace dereverberation {

RealFft::RealFft(size_t n) : n_(n), m_(n / 2 + 1), full_(n) {}

void RealFft::Fft(const double* x, std::complex<double>* X) const {
  time_in_.resize(n_);
  std::copy(x, x + n_, time_in_.begin());
  fft_.fwd(full_, time_in_);
  std::copy(full_.begin(), full_.begin() + m_, X);
}

void RealFft::Ifft(const std::complex<double>* X, double* x) const {
  // Rebuild the full conjugate-symmetric spectrum of length n_.
  full_[0] = std::complex<double>(std::real(X[0]), 0.0);  // force DC real
  for (size_t k = 1; k < m_; ++k) {
    full_[k] = X[k];
    full_[n_ - k] = std::conj(X[k]);
  }
  std::vector<std::complex<double>> out(n_);
  fft_.inv(out, full_);
  for (size_t i = 0; i < n_; ++i) {
    x[i] = std::real(out[i]);
  }
}

}  // namespace dereverberation
