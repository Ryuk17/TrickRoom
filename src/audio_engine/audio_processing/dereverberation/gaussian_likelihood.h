/*
 * Log probability density of a multivariate normal distribution, matching
 * v_gaussmixp(y', m', V) from voicebox with a single mixture component:
 *
 *   lp = log(w) - 0.5*sum(log(eig(V))) - 0.5*(y-m)'*inv(V)*(y-m)
 *        - 0.5*q*log(2*pi)
 *
 * (w == 1 for the calls made by v_spendred).
 */

#ifndef DEREEVERBERATION_GAUSSIAN_LIKELIHOOD_H_
#define DEREEVERBERATION_GAUSSIAN_LIKELIHOOD_H_

#include <Eigen/Dense>

namespace dereverberation {

// Returns the log probability density of y under N(m, S).
// Throws std::runtime_error if S is not positive definite.
double LogMultivariateNormal(const Eigen::VectorXd& y, const Eigen::VectorXd& m,
                             const Eigen::Ref<const Eigen::MatrixXd>& S);

}  // namespace dereverberation

#endif  // DEREEVERBERATION_GAUSSIAN_LIKELIHOOD_H_
