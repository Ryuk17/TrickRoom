#include "dereverberation/gaussian_likelihood.h"

#include <cmath>
#include <stdexcept>

namespace dereverberation {

namespace {
constexpr double kLog2Pi = 1.8378770664093453;  // log(2*pi)
}

double LogMultivariateNormal(const Eigen::VectorXd& y, const Eigen::VectorXd& m,
                             const Eigen::Ref<const Eigen::MatrixXd>& S) {
  const int q = static_cast<int>(y.size());
  Eigen::LLT<Eigen::MatrixXd> llt(S);
  if (llt.info() != Eigen::Success) {
    throw std::runtime_error(
        "Covariance matrix not positive definite - To avoid this, try a "
        "higher energy floor (e.g. algo_params.ef=-50)");
  }
  const Eigen::MatrixXd& L = llt.matrixL();
  // -0.5*log|S| = -sum(log(diag(L)))
  double logdet_half = 0.;
  for (int i = 0; i < q; ++i) {
    logdet_half += std::log(L(i, i));
  }
  const Eigen::VectorXd d = y - m;
  const Eigen::VectorXd z = L.triangularView<Eigen::Lower>().solve(d);
  const double quad = z.squaredNorm();  // (y-m)'*inv(S)*(y-m)
  return -logdet_half - 0.5 * quad - 0.5 * static_cast<double>(q) * kLog2Pi;
}

}  // namespace dereverberation
