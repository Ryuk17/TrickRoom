/*
 * Mel-scale filterbank matrices used by the dereverberation algorithm.
 *
 * Ports of two voicebox routines:
 *   ComputeMelFilterBank    : v_filtbankm(p, n, fs, [], [], 'm') with all
 *                             default options (forward transform)
 *   ComputeMelInterpMatrix  : interpofiltbankm(p, n, fs) from v_spendred.m
 *                             (interpolation of a mel-band gain back to the
 *                             full STFT spectrum)
 *
 * Both are constant for a fixed (p, n, fs) and are computed once at
 * construction time.
 */

#ifndef DEREEVERBERATION_MEL_FILTERBANK_H_
#define DEREEVERBERATION_MEL_FILTERBANK_H_

#include <Eigen/Dense>

namespace dereverberation {

// mel = sign(f) * ln(1 + |f|/700) * k, with k = 1000/ln(1+1000/700)
constexpr double kMelScale = 1127.01048;

inline double HzToMel(double f) {
  return std::copysign(1.0, f) * std::log1p(std::fabs(f) / 700.0) * kMelScale;
}

inline double MelToHz(double m) {
  return 700.0 * std::copysign(1.0, m) * (std::exp(std::fabs(m) / kMelScale) - 1.0);
}

// Forward mel filterbank matrix: p x (1+floor(n/2)).
Eigen::MatrixXd ComputeMelFilterBank(int p, int n, double fs);

// Interpolation matrix from mel bands to STFT bins: (1+floor(n/2)) x p.
Eigen::MatrixXd ComputeMelInterpMatrix(int p, int n, double fs);

}  // namespace dereverberation

#endif  // DEREEVERBERATION_MEL_FILTERBANK_H_
