/*
 * Speech enhancement and dereverberation (Doire et al.), C++ port of
 * voicebox v_spendred.m (fast mode only, algo_params.mo == 1).
 *
 * Real-time streaming interface: Process() consumes one frame of
 * kFrameIncrement (64) samples and produces one frame of output.
 *
 * The algorithm needs the mel spectra of the first 12 frames for
 * initialization, so the first 11 output frames are silent and frame 12
 * onward matches the MATLAB batch output (there is no output delay beyond
 * those startup frames).
 *
 * Reference:
 * [1] C. S. J. Doire et al., "Single-channel online enhancement of speech
 *     corrupted by reverberation and noise", IEEE Trans. ASLP 25(3), 2017.
 */

#ifndef DEREEVERBERATION_DEREEVERBERATION_H_
#define DEREEVERBERATION_DEREEVERBERATION_H_

#include <array>
#include <complex>
#include <vector>

#include <Eigen/Dense>

#include "dereverberation/dereverberation_config.h"
#include "dereverberation/gaussian_likelihood.h"
#include "dereverberation/hmm_dictionary.h"
#include "dereverberation/hmm_dictionary_data.h"
#include "dereverberation/mel_filterbank.h"
#include "dereverberation/real_fft.h"

namespace dereverberation {

class DeReverberation {
 public:
  // sample_rate_hz must be 16000.
  DeReverberation(const DeReverberationConfig& config, size_t sample_rate_hz);
  DeReverberation(const DeReverberation&) = delete;
  DeReverberation& operator=(const DeReverberation&) = delete;

  // Processes one frame of kFrameIncrement samples (16 kHz) and writes one
  // frame of enhanced samples to |output_frame|. In-place operation is
  // supported.
  void Process(const float* input_frame, float* output_frame);

  size_t frame_increment() const { return ni_; }

 private:
#ifdef DR_DEBUG_EXPOSE
 public:
#endif
  static constexpr int kNfc = kNumMelBands;  // number of mel bands
  static constexpr int kInitFrames = 12;     // frames needed for initialization

  void Initialize();
  // Runs one main-loop iteration observing the given mel log-power spectrum.
  void RunIteration(const Eigen::VectorXd& obs);
  // Synthesizes the output frame using the given spectral gain and spectrum.
  void Synthesize(const Eigen::VectorXd& spec_gain,
                  const std::complex<double>* spectrum, float* output_frame);
  // Computes the log-power mel spectrum of the current STFT frame.
  void ComputeObservation();
  // Handles non-positive-definite Sk by diagonal loading (with retries).
  double LikelihoodWithDl(const Eigen::VectorXd& y, const Eigen::VectorXd& m,
                          Eigen::Ref<Eigen::MatrixXd> S);

  // ----- configuration-derived constants -----
  int ni_;         // frame increment in samples (64)
  int nf_;         // FFT length (384)
  int no_;         // overlap factor (6)
  int ncols_;      // number of FFT bins (nf/2 + 1)
  double tinc_;    // true frame increment in seconds
  int K_;          // number of HMM states
  DeReverberationConfig cfg_;

  // ----- precomputed quantities -----
  RealFft fft_;
  Eigen::VectorXd w_;           // analysis/synthesis window (nf)
  Eigen::MatrixXd ThO_Tr_;      // forward mel filterbank (25 x 193)
  Eigen::MatrixXd reco_mat_;    // mel -> STFT interpolation (193 x 25)
  HmmDictionary dict_;
  Eigen::MatrixXd pdm_;         // 100 x 126 prediction matrix
  Eigen::MatrixXd pdm3_;        // 75 x 76 observation matrix
  Eigen::MatrixXd stat_mat_;    // 25 x K^2 stacked state means
  Eigen::VectorXd pdm2_;        // 100: reshape to 25x4, cols 2/4 are +1
  Eigen::MatrixXd Qx_;          // 51 x 51 state noise
  Eigen::MatrixXd Qr_;          // 50 x 50 reverb parameter noise
  Eigen::MatrixXd Covx_init_;   // 51 x 51 initial state covariance

  // ----- streaming state -----
  std::vector<float> frame_buffer_;  // nf-sample sliding window
  std::vector<std::complex<double>> spectrum_;  // nf/2+1 bins
  std::array<float, 320> overlap_;   // overlap-add memory
  Eigen::VectorXd cur_mel_;          // current frame mel log-power (25)
  Eigen::MatrixXd init_mel_;         // 25 x kInitFrames collected mel spectra
  // Full STFT spectra of the initialization frames (for synthesis replay).
  std::vector<std::complex<double>> init_spectra_;
  double running_max_mel_ = -1e300;  // running max of the mel log-power
  int frame_count_ = 0;              // frames consumed (0-based)
  bool initialized_ = false;
  bool replayed_ = false;

  // ----- algorithm state (names mirror v_spendred.m) -----
  Eigen::MatrixXd X_;                // 51 x K state
  std::vector<Eigen::MatrixXd> Cov_;         // K of 51x51
  std::vector<Eigen::MatrixXd> Cov_speech_;  // K of 25x25
  Eigen::MatrixXd M_speech_;         // 25 x K
  Eigen::VectorXd probs_;            // K (log probabilities)
  Eigen::VectorXd Rp_;               // 50 (2x25 reverb parameters)
  Eigen::MatrixXd Covr_;             // 50x50
  Eigen::VectorXd X_rev_;            // 51
  Eigen::VectorXd Speech_rev_;       // 25
  Eigen::VectorXd SpecGain_prev_;    // 193
  Eigen::VectorXd Xi_;               // 25 (MMSE prior SNR, sg == 3)
  Eigen::VectorXd Energy_prev_;      // 25 (previous frame energy, sg == 3)
};

}  // namespace dereverberation

#endif  // DEREEVERBERATION_DEREEVERBERATION_H_
