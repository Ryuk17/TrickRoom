/*
 * Configuration for the DeReverberation class, mirroring algo_params of
 * v_spendred.m.
 */

#ifndef DEREEVERBERATION_DEREEVERBERATION_CONFIG_H_
#define DEREEVERBERATION_DEREEVERBERATION_CONFIG_H_

namespace dereverberation {

struct DeReverberationConfig {
  int overlap_factor = 6;            // of : FFT length / frame increment
  double frame_increment_s = 5e-3;   // ti : desired frame increment in seconds
  bool round_frame_increment = true; // ri : round ni to nearest power of 2
  // sg : spectral gain type: 1=Wiener, 2=power spectral subtraction,
  // 3=MMSE speech estimate
  int spectral_gain_type = 1;
  double gain_smoothing = 0.95;   // sc : spectral gain smoothing constant
  double gain_floor = 1e-5;       // sf : floor for the spectral gain
  double oversubtraction = 2.;    // os : interference oversubtraction factor
  int num_states = 6;             // cl : number of HMM states (2..6)
  int posterior_mode = 1;         // ds : 1=max track, 2=weighted sum
  double energy_floor_db = -60.;  // ef : energy floor in dB
  // MATLAB clips the log-power observation to [global max + ef, ...] where
  // the global max is the maximum over the WHOLE signal (batch processing).
  // The streaming port cannot know this in advance: by default it uses the
  // running maximum. Set clip_reference_db to the signal-wide maximum to
  // reproduce the MATLAB batch behavior exactly (e.g. in the test program).
  double clip_reference_db = -1e300;  // -inf sentinel: use running max
};

}  // namespace dereverberation

#endif  // DEREEVERBERATION_DEREEVERBERATION_CONFIG_H_
