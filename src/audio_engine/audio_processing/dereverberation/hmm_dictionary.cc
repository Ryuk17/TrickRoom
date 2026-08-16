#include "dereverberation/hmm_dictionary.h"

#include <array>
#include <cmath>
#include <stdexcept>

#include "dereverberation/hmm_dictionary_data.h"

namespace dereverberation {

namespace {
constexpr int kMaxStates = 6;
constexpr double kLog10Inv = 1. / std::log(10.0);  // 1/log(10)
}  // namespace

HmmDictionary::HmmDictionary(int num_states) : K_(num_states) {
  if (K_ < 2 || K_ > kMaxStates) {
    throw std::runtime_error(
        "The number of states you have selected is not permitted !");
  }

  // States kept after pruning, matching the MATLAB switch on algo_params.cl:
  //   case 6: keep all
  //   case 5: remove state 6
  //   case 4: remove states 6, 1
  //   case 3: remove states 6, 1, 2
  //   case 2: remove states 6, 1, 2, then the 2nd remaining (original 3)
  std::array<int, kMaxStates> keep{};
  switch (K_) {
    case 6: keep = {0, 1, 2, 3, 4, 5}; break;
    case 5: keep = {0, 1, 2, 3, 4}; break;
    case 4: keep = {1, 2, 3, 4}; break;
    case 3: keep = {2, 3, 4}; break;
    case 2: keep = {3, 4}; break;
    default: throw std::runtime_error("unreachable");
  }

  // mStates: 25 x 6 -> 25 x K
  m_states_.resize(kNumMelBands, K_);
  for (int b = 0; b < kNumMelBands; ++b) {
    for (int k = 0; k < K_; ++k) {
      m_states_(b, k) = kMStates[b][keep[k]];
    }
  }

  // covStates: 6 of 25x25 -> K of 25x25
  const double(*covs[kMaxStates])[kNumMelBands] = {
      kCovStates1, kCovStates2, kCovStates3, kCovStates4, kCovStates5,
      kCovStates6};
  cov_states_.resize(K_);
  for (int k = 0; k < K_; ++k) {
    cov_states_[k].resize(kNumMelBands, kNumMelBands);
    for (int r = 0; r < kNumMelBands; ++r) {
      for (int c = 0; c < kNumMelBands; ++c) {
        cov_states_[k](r, c) = covs[keep[k]][r][c];
      }
    }
  }

  // trans_probs: 6x6 -> K x K
  trans_probs_.resize(K_, K_);
  for (int r = 0; r < K_; ++r) {
    for (int c = 0; c < K_; ++c) {
      trans_probs_(r, c) = kTransProbs[keep[r]][keep[c]];
    }
  }
  // Renormalize as in MATLAB (change_tp == 1 branch).
  for (int i = 0; i < K_; ++i) {
    trans_probs_(i, i) = 0.5;
    // Off-diagonal entries of row i, before rescaling.
    std::vector<double> off;
    off.reserve(K_ - 1);
    for (int j = 0; j < K_; ++j) {
      if (j != i) off.push_back(trans_probs_(i, j));
    }
    double sum = 0.;
    for (double v : off) sum += v;
    int idx = 0;
    for (int j = 0; j < K_; ++j) {
      if (j != i) trans_probs_(i, j) = 0.5 * off[idx++] / sum;
    }
  }

  // kappa_s = ((10/log(10))^2) * (kappa_raw + 0.1)
  const double s = (10. * kLog10Inv) * (10. * kLog10Inv);
  kappa_s_.resize(kNumMelBands);
  for (int b = 0; b < kNumMelBands; ++b) {
    kappa_s_(b) = s * (kKappaRaw[b] + 0.1);
  }
}

}  // namespace dereverberation
