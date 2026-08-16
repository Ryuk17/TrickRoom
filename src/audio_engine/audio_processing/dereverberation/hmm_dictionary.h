/*
 * HMM speech model dictionary used by the dereverberation algorithm:
 * state means, per-state covariances, transition probabilities and the
 * observation noise vector. The data itself is auto-generated from
 * voicebox v_spendred.m into hmm_dictionary_data.h.
 *
 * The MATLAB algorithm permits 2..6 states (algo_params.cl); the dictionary
 * is pruned to the requested number of states with transition probabilities
 * renormalized exactly as in v_spendred.m.
 */

#ifndef DEREEVERBERATION_HMM_DICTIONARY_H_
#define DEREEVERBERATION_HMM_DICTIONARY_H_

#include <vector>

#include <Eigen/Dense>

namespace dereverberation {

class HmmDictionary {
 public:
  // num_states must be in [2, 6].
  explicit HmmDictionary(int num_states);

  int num_states() const { return K_; }

  // State means, 25 mel bands x K states.
  const Eigen::MatrixXd& m_states() const { return m_states_; }
  // K state covariances, each 25x25.
  const std::vector<Eigen::MatrixXd>& cov_states() const { return cov_states_; }
  // State transition probabilities, K x K (used linearly as in MATLAB).
  const Eigen::MatrixXd& trans_probs() const { return trans_probs_; }
  // Scaled observation noise kappa_s = ((10/log(10))^2)*(kappa_raw+0.1), 25x1.
  const Eigen::VectorXd& kappa_s() const { return kappa_s_; }

 private:
  int K_;
  Eigen::MatrixXd m_states_;
  std::vector<Eigen::MatrixXd> cov_states_;
  Eigen::MatrixXd trans_probs_;
  Eigen::VectorXd kappa_s_;
};

}  // namespace dereverberation

#endif  // DEREEVERBERATION_HMM_DICTIONARY_H_
