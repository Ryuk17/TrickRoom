#include "dereverberation/dereverberation.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <stdexcept>

namespace dereverberation {

namespace {

constexpr double kLog10Over10 = 0.2302585093;       // log(10)/10
constexpr double kLog10InvSq = 18.86390586;         // (10/log(10))^2
constexpr double kLog2Pi = 1.8378770664093453;      // log(2*pi)
constexpr double kPi = 3.14159265358979323846;

// MMSE estimator constants (algo_params.sg == 3).
constexpr double kMuMmse = 0.5;
constexpr double kBetaMmse = 0.5;
constexpr double kP0 = 0.5;
constexpr double kPInf = 1.;
// gammaFactor = gamma(mu+beta/2)/gamma(mu) = gamma(0.75)/gamma(0.5)
constexpr double kGammaFactor = 0.6914859868624776;
// gammaFactor^(1/beta) with beta = 0.5
constexpr double kGammaFactorPow = 0.4781529271305188;

// Average subband T60 values from measurements on a variety of RIRs
// (inaccurate at low frequencies), used to initialize the reverb parameters.
constexpr double kT60[25] = {
    0.625814328889286, 0.635814328889286, 0.559669908460684,
    0.533597692539940, 0.523597692539940, 0.519657090507566,
    0.498220803534608, 0.488220803534608, 0.470970193330601,
    0.460004032902958, 0.445158336751369, 0.436812074651546,
    0.400699712568631, 0.395830867067534, 0.399005310050930,
    0.401204573100827, 0.417815999144577, 0.417892069024374,
    0.419406546706839, 0.422573783237992, 0.417601620276530,
    0.408528394010926, 0.403943250672072, 0.354372247720777,
    0.316876109784076};

// Symmetric Hann window of length nf+1, truncated to nf samples (as in
// v_spendred.m: w_synth = hann(nf+1)'; w_synth(end) = []).
Eigen::VectorXd ComputeSynthWindow(int nf) {
  const int L = nf + 1;
  Eigen::VectorXd w(nf);
  for (int i = 0; i < nf; ++i) {
    const double v = 0.5 * (1.0 - std::cos(2.0 * kPi * i / (L - 1)));
    w(i) = v;
  }
  return w;
}

// MATLAB cov() of a matrix whose columns are the variables (each row is an
// observation): C(i,j) = sum_t (A(t,i)-mu_i)(A(t,j)-mu_j) / (T-1).
// A is T x D, the result is D x D.
Eigen::MatrixXd SampleCovariance(const Eigen::Ref<const Eigen::MatrixXd>& A) {
  const int T = static_cast<int>(A.rows());  // observations
  const int D = static_cast<int>(A.cols());  // variables
  Eigen::VectorXd mu = A.colwise().mean();
  Eigen::MatrixXd C(D, D);
  for (int i = 0; i < D; ++i) {
    for (int j = 0; j < D; ++j) {
      double s = 0.;
      for (int t = 0; t < T; ++t) {
        s += (A(t, i) - mu(i)) * (A(t, j) - mu(j));
      }
      C(i, j) = s / static_cast<double>(T - 1);
    }
  }
  return C;
}

}  // namespace

DeReverberation::DeReverberation(const DeReverberationConfig& config,
                                 size_t sample_rate_hz)
    : cfg_(config), K_(config.num_states), fft_(384), dict_(config.num_states) {
  if (sample_rate_hz != 16000) {
    throw std::runtime_error("DeReverberation only supports 16 kHz input");
  }
  const double fs = static_cast<double>(sample_rate_hz);

  // Frame increment rounded to the nearest power of 2 (ri == 1): with
  // ti=5ms and fs=16kHz this gives ni = 64.
  ni_ = cfg_.round_frame_increment
            ? static_cast<int>(std::pow(2., std::ceil(std::log2(
                                                  cfg_.frame_increment_s * fs *
                                                  std::sqrt(0.5)))))
            : static_cast<int>(std::round(cfg_.frame_increment_s * fs));
  no_ = std::max(1, static_cast<int>(std::round(
                        static_cast<double>(cfg_.overlap_factor))));
  nf_ = ni_ * no_;
  tinc_ = static_cast<double>(ni_) / fs;
  ncols_ = nf_ / 2 + 1;
  if (nf_ != 384) {
    throw std::runtime_error(
        "DeReverberation currently only supports nf == 384");
  }

  // Analysis/synthesis window: w = w_synth / sqrt(sum(w_synth(1:ni:nf).^2))
  Eigen::VectorXd w_synth = ComputeSynthWindow(nf_);
  double s2 = 0.;
  for (int i = 0; i < nf_; i += ni_) s2 += w_synth(i) * w_synth(i);
  w_ = w_synth / std::sqrt(s2);

  ThO_Tr_ = ComputeMelFilterBank(kNfc, nf_, fs);
  reco_mat_ = ComputeMelInterpMatrix(kNfc, nf_, fs);

  // Prediction matrix pdm (100 x 126): maps [X(51); Rp(50); M_speech(25)]
  // to the four numerator/denominator terms of the reverb power prediction.
  // Column layout: [X(1); X(2:nfc+1); X(nfc+2:2nfc+1); Rp(1,:); Rp(2,:);
  // M_speech]. MATLAB blocks: row1 = X(2:nfc+1)+Rp(1,:), row2 = Rp(1,:),
  // row3 = X(1)+Rp(2,:)+M_speech, row4 = Rp(2,:).
  pdm_ = Eigen::MatrixXd::Zero(4 * kNfc, 5 * kNfc + 1);
  pdm_.block(0, 1, kNfc, kNfc).setIdentity();          // X(2:nfc+1)
  pdm_.block(0, 2 * kNfc + 1, kNfc, kNfc).setIdentity();  // Rp(1,:)
  pdm_.block(kNfc, 2 * kNfc + 1, kNfc, kNfc).setIdentity();  // Rp(1,:)
  pdm_.block(2 * kNfc, 0, kNfc, 1).setOnes();          // X(1)
  pdm_.block(2 * kNfc, 3 * kNfc + 1, kNfc, kNfc).setIdentity();  // Rp(2,:)
  pdm_.block(2 * kNfc, 4 * kNfc + 1, kNfc, kNfc).setIdentity();  // M_speech
  pdm_.block(3 * kNfc, 3 * kNfc + 1, kNfc, kNfc).setIdentity();  // Rp(2,:)

  // pdm2 (100x1): +1 on the denominators (cols 2 and 4 after reshape).
  pdm2_ = Eigen::VectorXd::Zero(4 * kNfc);
  pdm2_.segment(kNfc, kNfc).setOnes();
  pdm2_.segment(3 * kNfc, kNfc).setOnes();

  // Observation matrix pdm3 (75 x 76): maps [X; M_speech] to gain*clean,
  // reverb and noise power terms.
  pdm3_ = Eigen::MatrixXd::Zero(3 * kNfc, 3 * kNfc + 1);
  pdm3_.block(0, 0, kNfc, 1).setOnes();                // X(1)
  pdm3_.block(0, 2 * kNfc + 1, kNfc, kNfc).setIdentity();  // M_speech
  pdm3_.block(kNfc, 1, kNfc, kNfc).setIdentity();      // X(2:nfc+1)
  pdm3_.block(2 * kNfc, kNfc + 1, kNfc, kNfc).setIdentity();  // X(nfc+2:end)

  // stat_mat (25 x K^2): stacked state means. MATLAB:
  // stat_mat = reshape(repmat(mStates, K, 1), nfc, K^2) fills column-major, so
  // column c holds state floor(c/K) (the CURRENT state k2), while repmat(X,1,K)
  // makes the X part of column c come from state mod(c,K) (previous state k1).
  stat_mat_.resize(kNfc, K_ * K_);
  for (int c = 0; c < K_ * K_; ++c) {
    stat_mat_.col(c) = dict_.m_states().col(c / K_);
  }

  // Process noise covariances (time update).
  Qx_ = Eigen::MatrixXd::Zero(2 * kNfc + 1, 2 * kNfc + 1);
  Qx_(0, 0) = 1. / 350000.;
  Qx_.block(1, 1, kNfc, kNfc).diagonal().setConstant(1. / 1500.);
  Qx_.block(kNfc + 1, kNfc + 1, kNfc, kNfc).diagonal().setConstant(1. / 7550.);
  Qr_ = Eigen::MatrixXd::Zero(2 * kNfc, 2 * kNfc);
  Qr_.block(0, 0, kNfc, kNfc).diagonal().setConstant(1. / 1700.);
  Qr_.block(kNfc, kNfc, kNfc, kNfc).diagonal().setConstant(1. / 700.);
  Qx_ = 15. * Qx_ + 1e-5 * Eigen::MatrixXd::Identity(2 * kNfc + 1, 2 * kNfc + 1) *
                        Qx_.trace();
  Qr_ = 15. * Qr_ + 1e-5 * Eigen::MatrixXd::Identity(2 * kNfc, 2 * kNfc) *
                        Qr_.trace();
  Covx_init_ = Qx_ * 1.5;

  // Streaming state.
  frame_buffer_.assign(nf_, 0.);
  spectrum_.resize(ncols_);
  overlap_.fill(0.);
  init_mel_.resize(kNfc, kInitFrames);
  init_spectra_.resize(static_cast<size_t>(kInitFrames) * ncols_);
  X_.resize(2 * kNfc + 1, K_);
  M_speech_.resize(kNfc, K_);
  Cov_.resize(K_);
  Cov_speech_.resize(K_);
  probs_.resize(K_);
  Rp_.resize(2 * kNfc);
  Covr_.resize(2 * kNfc, 2 * kNfc);
  X_rev_.resize(2 * kNfc + 1);
  Speech_rev_.resize(kNfc);
  SpecGain_prev_.setZero(ncols_);
  Xi_.setZero(kNfc);
  Energy_prev_.setZero(kNfc);
  cur_mel_.resize(kNfc);
}

void DeReverberation::Initialize() {
  // Init clean speech posterior mean and covariance with priors.
  M_speech_ = dict_.m_states();
  Cov_speech_ = dict_.cov_states();

  // Initial path probabilities.
  probs_.setConstant(std::log(1. / static_cast<double>(K_)));

  // State space representation --> [gain; reverb_power_in_subbands;
  // noise_power_in_subbands].
  X_.setZero();
  X_.row(0).setConstant(-12.);
  const Eigen::VectorXd init_mean = init_mel_.rowwise().mean();
  for (int k = 0; k < K_; ++k) {
    X_.block(1, k, kNfc, 1) = init_mean;          // reverb power init
    X_.block(kNfc + 1, k, kNfc, 1) = init_mean;   // noise level init
  }

  // Initialize the reverb parameters.
  Eigen::VectorXd alpha(kNfc), f(kNfc);
  for (int b = 0; b < kNfc; ++b) {
    alpha(b) = std::pow(10., (-6. * tinc_) / kT60[b]);
    const double lin = -0.2 + (0.8 + 0.2) * static_cast<double>(b) /
                                    static_cast<double>(kNfc - 1);
    f(b) = (1. - alpha(b)) / std::pow(10., lin);
  }
  Rp_.head(kNfc) =
      (10. * (alpha.array() / (1. - alpha.array())).log10() + 1.5).matrix();
  Rp_.tail(kNfc) = (10. * (f.array() / (1. - f.array())).log10()).matrix();

  // Reverb parameter filter state.
  X_rev_ = X_.col(0);
  Speech_rev_ = M_speech_.col(0);
  Covr_ = Qr_ * 15.;

  // State space covariance, with observation noise from the first frames.
  const Eigen::MatrixXd cov_obs =
      SampleCovariance(init_mel_.leftCols(10).transpose());
  Eigen::MatrixXd Covx = Covx_init_;
  Covx.block(kNfc + 1, kNfc + 1, kNfc, kNfc) += cov_obs;
  for (int k = 0; k < K_; ++k) {
    Cov_[k] = Covx;
  }

  // Initial gain.
  SpecGain_prev_.setConstant(1e-5);

  // MMSE estimator state.
  Xi_.setConstant(1e-5);
}

void DeReverberation::ComputeObservation() {
  // Apply analysis window and FFT.
  std::vector<double> x(nf_);
  for (int i = 0; i < nf_; ++i) x[i] = static_cast<double>(frame_buffer_[i]) * w_[i];
  fft_.Fft(x.data(), spectrum_.data());

  // Power spectrum (periodogram), normalized like MATLAB:
  // gt_YP_full = |C|.^2 / (ncols * sum(w.^2)) where ncols is the number of
  // FFT bins (MATLAB: [nrows,ncols] = size(C) with C = 1695 x 193, so
  // ncols = nf/2+1 = 193, NOT the frame count).
  const double norm = 1. / (static_cast<double>(ncols_) * w_.squaredNorm());
  Eigen::VectorXd power(ncols_);
  for (int k = 0; k < ncols_; ++k) {
    power(k) = std::norm(spectrum_[k]) * norm;
  }
  Eigen::VectorXd mel = ThO_Tr_ * power;
  mel = 10. * mel.array().log10();
  // Clip to the energy floor range to avoid negative infinities. MATLAB uses
  // the maximum over the whole signal; the streaming port uses the running
  // maximum unless a reference is provided in the config.
  const double mx = mel.maxCoeff();
  running_max_mel_ = std::max(running_max_mel_, mx);
  const double ref = (cfg_.clip_reference_db > -1e200)
                         ? cfg_.clip_reference_db
                         : running_max_mel_;
  mel = mel.cwiseMax(ref + cfg_.energy_floor_db);
  cur_mel_ = mel;
}

void DeReverberation::Process(const float* input_frame, float* output_frame) {
  // Slide the FFT window by one frame increment.
  std::copy(frame_buffer_.begin() + ni_, frame_buffer_.end(),
            frame_buffer_.begin());
  std::copy(input_frame, input_frame + ni_, frame_buffer_.begin() + (nf_ - ni_));

  // The 384-sample FFT window covers samples [t*ni - 320, t*ni + 63] after
  // the t-th call, i.e. MATLAB frame t-5. The first 5 calls therefore see a
  // zero-padded (invalid) window: emit silence without collecting an
  // observation, giving a fixed 320-sample (5 frame) latency.
  if (frame_count_ < 5) {
    std::fill(output_frame, output_frame + ni_, 0.);
    ++frame_count_;
    return;
  }

  ComputeObservation();

  const int mel_idx = frame_count_ - 5;  // MATLAB frame index
  if (!initialized_) {
    init_mel_.col(mel_idx) = cur_mel_;
    std::copy(spectrum_.begin(), spectrum_.end(),
              init_spectra_.begin() + static_cast<size_t>(mel_idx) * ncols_);
    if (mel_idx < kInitFrames - 1) {
      // Still collecting initialization frames: silent output.
      std::fill(output_frame, output_frame + ni_, 0.);
      ++frame_count_;
      return;
    }
    // Twelfth observation frame: initialize and replay the first kInitFrames
    // iterations so that the output matches the MATLAB batch processing from
    // this frame on. Only the last replayed frame's output is kept.
    Initialize();
    initialized_ = true;
    for (int t = 0; t < kInitFrames; ++t) {
      RunIteration(init_mel_.col(t));
      if (t == kInitFrames - 1) {
        Synthesize(SpecGain_prev_,
                   init_spectra_.data() + static_cast<size_t>(t) * ncols_,
                   output_frame);
      }
    }
    ++frame_count_;
    return;
  }

  RunIteration(cur_mel_);
  Synthesize(SpecGain_prev_, spectrum_.data(), output_frame);
  ++frame_count_;
}

double DeReverberation::LikelihoodWithDl(const Eigen::VectorXd& y,
                                         const Eigen::VectorXd& m,
                                         Eigen::Ref<Eigen::MatrixXd> S) {
  try {
    return LogMultivariateNormal(y, m, S);
  } catch (const std::runtime_error&) {
    // The float implementation can lose positive definiteness of Sk due to
    // roundoff (the MATLAB double version stays positive definite). Retry
    // with progressively stronger diagonal loading; the loaded Sk replaces
    // the original so the posterior update uses the same matrix.
    const double s = S.diagonal().maxCoeff();
    const double lambdas[] = {1e-6, 1e-5, 1e-4, 1e-3, 1e-2, 1e-1, 1.};
    for (double lam : lambdas) {
      Eigen::MatrixXd Sd =
          S + (lam * s) * Eigen::MatrixXd::Identity(S.rows(), S.rows());
      try {
        const double lp = LogMultivariateNormal(y, m, Sd);
        S = Sd;
        return lp;
      } catch (const std::runtime_error&) {
      }
    }
    std::fprintf(stderr,
                 "DL FAIL frame %d: Sk diag [%.3e %.3e], X [%.3e %.3e], "
                 "Rp [%.3e %.3e], Cov diag [%.3e %.3e]\n",
                 frame_count_,
                 static_cast<double>(S.diagonal().minCoeff()),
                 static_cast<double>(S.diagonal().maxCoeff()),
                 static_cast<double>(X_.minCoeff()),
                 static_cast<double>(X_.maxCoeff()),
                 static_cast<double>(Rp_.minCoeff()),
                 static_cast<double>(Rp_.maxCoeff()),
                 static_cast<double>(Cov_[0].diagonal().minCoeff()),
                 static_cast<double>(Cov_[0].diagonal().maxCoeff()));
    throw std::runtime_error(
        "Covariance matrix not positive definite even after diagonal loading");
  }
}

void DeReverberation::RunIteration(const Eigen::VectorXd& obs) {
  const Eigen::VectorXd energy =
      (obs * kLog10Over10).array().exp();  // 10^(gt_YP/10)
  // ----- prediction stage for each track -----
  Eigen::MatrixXd tmpaug(5 * kNfc + 1, K_);
  tmpaug.topRows(2 * kNfc + 1) = X_;
  tmpaug.middleRows(2 * kNfc + 1, 2 * kNfc) = Rp_.replicate(1, K_);
  tmpaug.bottomRows(kNfc) = M_speech_;

  Eigen::MatrixXd tmp = (pdm_ * tmpaug) * kLog10Over10;
  tmp = tmp.array().exp() + pdm2_.replicate(1, K_).array();

  const Eigen::MatrixXd tmp_sum1 =
      tmp.topRows(kNfc).cwiseQuotient(tmp.middleRows(kNfc, kNfc));
  const Eigen::MatrixXd tmp_sum2 =
      tmp.middleRows(2 * kNfc, kNfc).cwiseQuotient(tmp.bottomRows(kNfc));
  const Eigen::MatrixXd tmp_sum = tmp_sum1 + tmp_sum2;

  // Reverb power part of the output state.
  X_.middleRows(1, kNfc) = 10. * tmp_sum.array().log10();

  // Jacobians.
  Eigen::MatrixXd Ftemp(2 * kNfc, K_);
  Ftemp.topRows(kNfc) = tmp_sum2.cwiseQuotient(tmp_sum);
  Ftemp.bottomRows(kNfc) = tmp_sum1.cwiseQuotient(tmp_sum);

  for (int k = 0; k < K_; ++k) {
    Eigen::MatrixXd Fx = Eigen::MatrixXd::Identity(2 * kNfc + 1, 2 * kNfc + 1);
    Fx.block(1, 0, kNfc, 1) = Ftemp.col(k).head(kNfc);
    Fx.block(1, 1, kNfc, kNfc).diagonal() = Ftemp.col(k).tail(kNfc);
    Eigen::MatrixXd Fu = Eigen::MatrixXd::Zero(2 * kNfc + 1, kNfc);
    Fu.block(1, 0, kNfc, kNfc).diagonal() = Ftemp.col(k).head(kNfc);
    // NOTE: replicates the MATLAB code verbatim, including its use of the
    // last state's clean-speech covariance for every track
    // (Cov_speech(:,:,i) with i == K).
    Cov_[k] = Fu * Cov_speech_[K_ - 1] * Fu.transpose() +
              Fx * Cov_[k] * Fx.transpose() + Qx_;
    // Symmetrize to counter float roundoff (the double-precision MATLAB
    // result is symmetric to machine precision).
    Cov_[k] = 0.5 * (Cov_[k] + Cov_[k].transpose()).eval();
  }
  // ----- update stage with K^2 possibilities -----
  Eigen::MatrixXd new_probs = probs_.replicate(1, K_) + dict_.trans_probs();

  Eigen::MatrixXd m_tilde(3 * kNfc + 1, K_ * K_);
  m_tilde.topRows(2 * kNfc + 1) = X_.replicate(1, K_);
  m_tilde.bottomRows(kNfc) = stat_mat_;

  Eigen::MatrixXd tmp2 = (pdm3_ * m_tilde) * kLog10Over10;
  tmp2 = tmp2.array().exp();

  const Eigen::MatrixXd tmp2_sum2 =
      tmp2.topRows(kNfc) + tmp2.middleRows(kNfc, kNfc) + tmp2.bottomRows(kNfc);
  const Eigen::MatrixXd zk = 10. * tmp2_sum2.array().log10();

  // Observation noise.
  Eigen::MatrixXd R = dict_.kappa_s().replicate(1, K_ * K_);
  R += kLog10InvSq * 2. *
       (tmp2.topRows(kNfc).cwiseProduct(tmp2.middleRows(kNfc, kNfc)) +
        tmp2.topRows(kNfc).cwiseProduct(tmp2.bottomRows(kNfc)) +
        tmp2.middleRows(kNfc, kNfc).cwiseProduct(tmp2.bottomRows(kNfc)))
           .cwiseQuotient(tmp2_sum2);

  // Jacobians.
  Eigen::MatrixXd Htemp(3 * kNfc, K_ * K_);
  Htemp.topRows(kNfc) = tmp2.topRows(kNfc).cwiseQuotient(tmp2_sum2);
  Htemp.middleRows(kNfc, kNfc) =
      tmp2.middleRows(kNfc, kNfc).cwiseQuotient(tmp2_sum2);
  Htemp.bottomRows(kNfc) = tmp2.bottomRows(kNfc).cwiseQuotient(tmp2_sum2);

  std::vector<Eigen::VectorXd> err(K_ * K_);
  std::vector<Eigen::MatrixXd> Hx(K_ * K_), Hu(K_ * K_), Sk(K_ * K_);
  Eigen::MatrixXd lkl(K_, K_);

  for (int k2 = 0; k2 < K_; ++k2) {
    for (int k1 = 0; k1 < K_; ++k1) {
      const int col = K_ * k2 + k1;
      const int idx = k1 * K_ + k2;
      Eigen::MatrixXd& Hxk = Hx[idx];
      Hxk.resize(kNfc, 2 * kNfc + 1);
      Hxk.setZero();
      Hxk.col(0) = Htemp.col(col).head(kNfc);
      Hxk.block(0, 1, kNfc, kNfc).diagonal() =
          Htemp.col(col).segment(kNfc, kNfc);
      Hxk.block(0, kNfc + 1, kNfc, kNfc).diagonal() =
          Htemp.col(col).tail(kNfc);
      Eigen::MatrixXd& Huk = Hu[idx];
      Huk = Htemp.col(col).head(kNfc).asDiagonal();

      err[idx] = Eigen::VectorXd(obs - zk.col(col));
      Sk[idx] = Hxk * Cov_[k1] * Hxk.transpose() +
                Huk * dict_.cov_states()[k2] * Huk.transpose() +
                R.col(col).asDiagonal().toDenseMatrix();
      lkl(k1, k2) = LikelihoodWithDl(obs, zk.col(col), Sk[idx]);
    }
  }
  const Eigen::MatrixXd joint_lkl = new_probs + lkl;

  // Pick the best track arriving at each HMM state.
  Eigen::VectorXd max_val(K_);
  std::vector<int> max_idx(K_);
  for (int k2 = 0; k2 < K_; ++k2) {
    int best = 0;
    double best_val = joint_lkl(0, k2);
    for (int k1 = 1; k1 < K_; ++k1) {
      if (joint_lkl(k1, k2) > best_val) {
        best_val = joint_lkl(k1, k2);
        best = k1;
      }
    }
    max_val(k2) = best_val;
    max_idx[k2] = best;
  }
  probs_ = max_val;
  // Posterior densities for the best tracks only.
  std::vector<Eigen::MatrixXd> cov_back = Cov_;
  for (int k = 0; k < K_; ++k) {
    const int k1 = max_idx[k];
    const int idx = k1 * K_ + k;
    const Eigen::MatrixXd& Skk = Sk[idx];
    Eigen::LLT<Eigen::MatrixXd> llt(Skk);
    if (llt.info() != Eigen::Success) {
      throw std::runtime_error(
          "Covariance matrix not positive definite - To avoid this, try a "
          "higher energy floor (e.g. algo_params.ef=-50)");
    }
    const Eigen::MatrixXd Kn =
        llt.solve(Hx[idx] * Cov_[k1].transpose()).transpose();
    const Eigen::MatrixXd Kn_u =
        llt.solve(Hu[idx] * dict_.cov_states()[k].transpose()).transpose();
    X_.col(k) = X_.col(k1) + Kn * err[idx];
    M_speech_.col(k) = dict_.m_states().col(k) + Kn_u * err[idx];
    // Joseph-form covariance update: mathematically identical to the MATLAB
    // P - K*S*K' form, but numerically much more robust in single precision
    // (guarantees positive semidefiniteness of the result).
    const int col = K_ * k + k1;
    const Eigen::MatrixXd& Hxk = Hx[idx];
    const Eigen::MatrixXd& Huk = Hu[idx];
    const Eigen::MatrixXd Rk =
        R.col(col).asDiagonal().toDenseMatrix() +
        Huk * dict_.cov_states()[k] * Huk.transpose();
    const Eigen::MatrixXd I51 =
        Eigen::MatrixXd::Identity(2 * kNfc + 1, 2 * kNfc + 1);
    Cov_[k] = (I51 - Kn * Hxk) * cov_back[k1] * (I51 - Kn * Hxk).transpose() +
              Kn * Rk * Kn.transpose();
    const Eigen::MatrixXd Ru =
        R.col(col).asDiagonal().toDenseMatrix() +
        Hxk * cov_back[k1] * Hxk.transpose();
    const Eigen::MatrixXd I25 = Eigen::MatrixXd::Identity(kNfc, kNfc);
    Cov_speech_[k] = (I25 - Kn_u * Huk) * dict_.cov_states()[k] *
                         (I25 - Kn_u * Huk).transpose() +
                     Kn_u * Ru * Kn_u.transpose();
    // Symmetrize to counter float roundoff.
    Cov_[k] = 0.5 * (Cov_[k] + Cov_[k].transpose()).eval();
    Cov_speech_[k] = 0.5 * (Cov_speech_[k] + Cov_speech_[k].transpose()).eval();
  }

  // Weighted sum of the tracks.
  const Eigen::VectorXd weights =
      (max_val.array() - max_val.maxCoeff()).exp();
  const double wsum = weights.sum();
  const Eigen::VectorXd weights_n = weights / wsum;
  const Eigen::VectorXd Weighted_X = X_ * weights_n;
  const Eigen::VectorXd Weighted_Speech = M_speech_ * weights_n;
  // ----- update the reverb parameters estimate -----
  Covr_ += Qr_;
  Eigen::VectorXd tmpaugpr(5 * kNfc + 1);
  tmpaugpr.head(2 * kNfc + 1) = X_rev_;
  tmpaugpr.segment(2 * kNfc + 1, 2 * kNfc) = Rp_;
  tmpaugpr.tail(kNfc) = Speech_rev_;
  Eigen::VectorXd tmppr = (pdm_ * tmpaugpr) * kLog10Over10;
  tmppr = tmppr.array().exp() + pdm2_.array();
  // tmppr reshaped to 25x4: col1/col2 + col3/col4.
  const Eigen::ArrayXd t1 = tmppr.head(kNfc).array();
  const Eigen::ArrayXd t2 = tmppr.segment(kNfc, kNfc).array();
  const Eigen::ArrayXd t3 = tmppr.segment(2 * kNfc, kNfc).array();
  const Eigen::ArrayXd t4 = tmppr.tail(kNfc).array();
  const Eigen::ArrayXd tmp_sumpr = t1 / t2 + t3 / t4;
  const Eigen::VectorXd outrev = 10. * tmp_sumpr.log10().matrix();

  Eigen::MatrixXd Gx(kNfc, 2 * kNfc);
  Gx.leftCols(kNfc) = ((t1 / t2.square()) / tmp_sumpr).matrix().asDiagonal();
  Gx.rightCols(kNfc) = ((t3 / t4.square()) / tmp_sumpr).matrix().asDiagonal();

  const Eigen::VectorXd pred_err = Weighted_X.segment(1, kNfc) - outrev;
  Eigen::MatrixXd RevSk = Gx * Covr_ * Gx.transpose() +
                          (Covr_.trace() / 5.) *
                              Eigen::MatrixXd::Identity(kNfc, kNfc);
  Eigen::LLT<Eigen::MatrixXd> llt_rev(RevSk);
  if (llt_rev.info() == Eigen::Success) {
    const Eigen::MatrixXd RevK =
        llt_rev.solve(Gx * Covr_.transpose()).transpose();
    Rp_ += RevK * pred_err;
    Covr_ -= RevK * RevSk * RevK.transpose();
  } else {
  }
  X_rev_ = Weighted_X;
  Speech_rev_ = Weighted_Speech;
  // ----- compute the gain -----
  Eigen::VectorXd GainRevNoise(2 * kNfc + 1);
  Eigen::VectorXd SpeechPost(kNfc);
  const double k = kLog10Over10 * kLog10Over10;
  if (cfg_.posterior_mode == 1) {
    // Instantaneous best track.
    int max_idx2 = 0;
    double best_val = max_val(0);
    for (int i = 1; i < K_; ++i) {
      if (max_val(i) > best_val) {
        best_val = max_val(i);
        max_idx2 = i;
      }
    }
    // NOTE: replicates the MATLAB code verbatim, including its use of the
    // last track's covariance (Cov{k} / Cov_speech(:,:,k) with k == K).
    GainRevNoise =
        ((kLog10Over10 * X_.col(max_idx2)) -
         0.5 * (k * Cov_[K_ - 1]).diagonal())
            .array()
            .exp();
    SpeechPost =
        ((kLog10Over10 * M_speech_.col(max_idx2)) -
         0.5 * (k * Cov_speech_[K_ - 1]).diagonal())
            .array()
            .exp() *
        GainRevNoise(0);
  } else {
    // Weighted sum of densities (ds == 2): weighted covariance required.
    Eigen::MatrixXd Weighted_cov =
        Eigen::MatrixXd::Zero(2 * kNfc + 1, 2 * kNfc + 1);
    Eigen::MatrixXd Weighted_cov_speech = Eigen::MatrixXd::Zero(kNfc, kNfc);
    for (int kk = 0; kk < K_; ++kk) {
      Weighted_cov += weights_n(kk) * (Cov_[kk] +
          (X_.col(kk) - Weighted_X) * (X_.col(kk) - Weighted_X).transpose());
      Weighted_cov_speech +=
          weights_n(kk) *
          (Cov_speech_[kk] + (M_speech_.col(kk) - Weighted_Speech) *
                                 (M_speech_.col(kk) - Weighted_Speech)
                                     .transpose());
    }
    GainRevNoise =
        ((kLog10Over10 * Weighted_X) - 0.5 * (k * Weighted_cov).diagonal())
            .array()
            .exp();
    SpeechPost =
        ((kLog10Over10 * Weighted_Speech) -
         0.5 * (k * Weighted_cov_speech).diagonal())
            .array()
            .exp() *
        GainRevNoise(0);
  }

  const Eigen::VectorXd interference =
      cfg_.oversubtraction *
      (GainRevNoise.segment(1, kNfc) + GainRevNoise.segment(kNfc + 1, kNfc));
  Eigen::VectorXd SpecGain(ncols_);
  switch (cfg_.spectral_gain_type) {
    case 1: {
      // Wiener gain.
      SpecGain = cfg_.gain_smoothing * SpecGain_prev_ +
                 (1. - cfg_.gain_smoothing) *
                     (reco_mat_ * (SpeechPost.cwiseQuotient(
                                       SpeechPost + interference)));
      break;
    }
    case 2: {
      // Power spectral subtraction gain.
      SpecGain = cfg_.gain_smoothing * SpecGain_prev_ +
                 (1. - cfg_.gain_smoothing) *
                     (reco_mat_ * (SpeechPost.cwiseQuotient(
                                       SpeechPost + interference)))
                         .cwiseSqrt();
      break;
    }
    case 3: {
      // MMSE estimate of clean speech.
      Xi_ = cfg_.gain_smoothing * Xi_ +
            (1. - cfg_.gain_smoothing) *
                SpeechPost.cwiseQuotient(interference);
      const Eigen::VectorXd Gamma_kl = energy.cwiseQuotient(interference);
      const Eigen::VectorXd nu_kl =
          ((Gamma_kl.array() * Xi_.array()) / (kMuMmse + Xi_.array())).matrix();
      const Eigen::VectorXd aHat0 =
          ((Xi_.array() / (kMuMmse + Xi_.array())).sqrt() * kGammaFactorPow *
           Gamma_kl.array().rsqrt())
              .matrix();
      SpecGain = reco_mat_ *
                 (((1. / (1. + nu_kl.array())).pow(kP0) * aHat0.array() +
                   (nu_kl.array() / (1. + nu_kl.array())).pow(kPInf) *
                       (Xi_.array() / (kMuMmse + Xi_.array()))))
                     .matrix();
      break;
    }
    default:
      throw std::runtime_error("Unsupported spectral gain type");
  }
  SpecGain_prev_ = SpecGain;
  Energy_prev_ = energy;
}

void DeReverberation::Synthesize(const Eigen::VectorXd& spec_gain,
                                 const std::complex<double>* spectrum,
                                 float* output_frame) {
  // FinalGain = max(SpecGain, sf)
  const Eigen::VectorXd gain = spec_gain.cwiseMax(cfg_.gain_floor);
  std::vector<std::complex<double>> filtered(ncols_);
  for (int k = 0; k < ncols_; ++k) {
    filtered[k] = spectrum[k] * gain(k);
  }
  std::vector<double> time(nf_);
  fft_.Ifft(filtered.data(), time.data());
  for (int i = 0; i < nf_; ++i) {
    time[i] = time[i] * w_[i];
  }
  // Overlap-add (MATLAB: enhanced_speech = sum of shifted windowed frames).
  const int kOverlap = nf_ - ni_;
  for (int i = 0; i < ni_; ++i) {
    output_frame[i] = static_cast<float>(overlap_[i] + time[i]);
  }
  for (int i = 0; i < kOverlap - ni_; ++i) {
    overlap_[i] = static_cast<float>(overlap_[i + ni_] + time[ni_ + i]);
  }
  for (int i = 0; i < ni_; ++i) {
    overlap_[kOverlap - ni_ + i] = static_cast<float>(time[kOverlap + i]);
  }
}

}  // namespace dereverberation
