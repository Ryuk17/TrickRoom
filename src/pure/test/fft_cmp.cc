// Compare NE10 r2c/c2r vs Ooura rdft (WebRTC) for FFT sizes 256 and 512.
#include <cstdio>
#include <cstring>
#include <cmath>
#include <cstdint>
#include <random>
#include <vector>

#include "NE10_types.h"
#include "NE10_fft.h"

extern "C" {
ne10_fft_r2c_cfg_float32_t ne10_fft_alloc_r2c_float32(ne10_int32_t nfft);
void ne10_fft_r2c_1d_float32_c(ne10_fft_cpx_float32_t* fout,
                               ne10_float32_t* fin,
                               ne10_fft_r2c_cfg_float32_t cfg);
void ne10_fft_c2r_1d_float32_c(ne10_float32_t* fout,
                               ne10_fft_cpx_float32_t* fin,
                               ne10_fft_r2c_cfg_float32_t cfg);
void ne10_fft_destroy_r2c_float32(ne10_fft_r2c_cfg_float32_t cfg);
}

namespace webrtc {
void WebRtc_rdft(size_t n, int isgn, float* a, size_t* ip, float* w);
}

namespace {
void RunOouraFwd(size_t n, const float* in, float* re, float* im) {
  size_t hf = n / 2 + 1;
  static std::vector<size_t> ip(512 / 2);
  static std::vector<float> w(512 / 2);
  static bool init = false;
  if (!init) {
    ip[0] = 0;
    std::vector<float> tmp(512, 0.f);
    webrtc::WebRtc_rdft(512, 1, tmp.data(), ip.data(), w.data());
    init = true;
  }
  std::vector<float> a(in, in + n);
  webrtc::WebRtc_rdft(n, 1, a.data(), ip.data(), w.data());
  im[0] = 0;
  re[0] = a[0];
  im[hf - 1] = 0;
  re[hf - 1] = a[1];
  for (size_t i = 1; i < hf - 1; ++i) {
    re[i] = a[2 * i];
    im[i] = a[2 * i + 1];
  }
}

void RunOouraInv(size_t n, const float* re, const float* im, float* out,
                 float scale_extra) {
  size_t hf = n / 2 + 1;
  static std::vector<size_t> ip(512 / 2);
  static std::vector<float> w(512 / 2);
  static bool init = false;
  if (!init) {
    ip[0] = 0;
    std::vector<float> tmp(512, 0.f);
    webrtc::WebRtc_rdft(512, 1, tmp.data(), ip.data(), w.data());
    init = true;
  }
  std::vector<float> a(n);
  a[0] = re[0];
  a[1] = re[hf - 1];
  for (size_t i = 1; i < hf - 1; ++i) {
    a[2 * i] = re[i];
    a[2 * i + 1] = im[i];
  }
  webrtc::WebRtc_rdft(n, -1, a.data(), ip.data(), w.data());
  for (size_t i = 0; i < n; ++i) a[i] *= scale_extra;
  std::memcpy(out, a.data(), n * sizeof(float));
}

void RunNe10Fwd(size_t n, const float* in, float* re, float* im,
                ne10_fft_r2c_cfg_float32_t cfg, std::vector<float>& tin,
                std::vector<float>& tout) {
  size_t hf = n / 2 + 1;
  std::memcpy(tin.data(), in, n * sizeof(float));
  ne10_fft_r2c_1d_float32_c(
      reinterpret_cast<ne10_fft_cpx_float32_t*>(tout.data()), tin.data(), cfg);
  auto* fout = reinterpret_cast<ne10_fft_cpx_float32_t*>(tout.data());
  im[0] = 0;
  re[0] = fout[0].r;
  im[hf - 1] = 0;
  re[hf - 1] = fout[n / 2].r;
  for (size_t i = 1; i < hf - 1; ++i) {
    re[i] = fout[i].r;
    im[i] = fout[i].i;
  }
}

void RunNe10Inv(size_t n, const float* re, const float* im, float* out,
                float scale_extra, ne10_fft_r2c_cfg_float32_t cfg,
                std::vector<float>& tin, std::vector<float>& tout) {
  size_t hf = n / 2 + 1;
  auto* fin = reinterpret_cast<ne10_fft_cpx_float32_t*>(tout.data());
  fin[0].r = re[0];
  fin[0].i = 0;
  for (size_t i = 1; i < hf - 1; ++i) {
    fin[i].r = re[i];
    fin[i].i = im[i];
  }
  fin[n / 2].r = re[hf - 1];
  fin[n / 2].i = 0;
  ne10_fft_c2r_1d_float32_c(tin.data(), fin, cfg);
  for (size_t i = 0; i < n; ++i) out[i] = tin[i] * scale_extra;
}

double MaxAbsDiff(const std::vector<float>& a, const std::vector<float>& b) {
  double m = 0;
  for (size_t i = 0; i < a.size(); ++i)
    m = std::max(m, std::abs((double)a[i] - b[i]));
  return m;
}

void Test(size_t n, unsigned seed) {
  size_t hf = n / 2 + 1;
  std::mt19937 rng(seed);
  std::uniform_real_distribution<float> dist(-1.f, 1.f);
  std::vector<float> x(n);
  for (auto& v : x) v = dist(rng);

  std::vector<float> o_re(hf), o_im(hf), n_re(hf), n_im(hf);
  RunOouraFwd(n, x.data(), o_re.data(), o_im.data());
  auto cfg = ne10_fft_alloc_r2c_float32(n);
  std::vector<float> tin(n), tout(2 * hf);
  RunNe10Fwd(n, x.data(), n_re.data(), n_im.data(), cfg, tin, tout);

  // Q1: forward FFT: NE10 vs Ooura (real part should match, imag sign flips).
  double fwd_re = 0, fwd_im = 0;
  for (size_t i = 0; i < hf; ++i) {
    fwd_re = std::max(fwd_re, std::abs((double)o_re[i] - n_re[i]));
    fwd_im = std::max(fwd_im, std::abs((double)o_im[i] + n_im[i]));
  }

  // Q2: round-trip: Ooura fwd + inv(scale 2/n)  -> should be 2*x
  std::vector<float> o_rt(n);
  RunOouraInv(n, o_re.data(), o_im.data(), o_rt.data(), 2.f / n);
  std::vector<float> x2(x);
  for (auto& v : x2) v *= 2.f;
  double o_rt_err = MaxAbsDiff(o_rt, x2);

  // Q3: round-trip: NE10 fwd + inv(scale k) for k in {1, 2, 2/n} -> compare vs 2*x
  std::vector<float> n_rt_1(n), n_rt_2(n), n_rt_2n(n);
  RunNe10Inv(n, n_re.data(), n_im.data(), n_rt_1.data(), 1.f, cfg, tin, tout);
  RunNe10Inv(n, n_re.data(), n_im.data(), n_rt_2.data(), 2.f, cfg, tin, tout);
  RunNe10Inv(n, n_re.data(), n_im.data(), n_rt_2n.data(), 2.f / n, cfg, tin, tout);
  double rt_1 = MaxAbsDiff(n_rt_1, x2);
  double rt_2 = MaxAbsDiff(n_rt_2, x2);
  double rt_2n = MaxAbsDiff(n_rt_2n, x2);

  // Q4: cross-feed: NE10 inv fed with Ooura spectrum (what ns_fft would get if
  // sign convention ignored) - check what scale matches Ooura inv.
  std::vector<float> x_rt(n);
  RunNe10Inv(n, o_re.data(), o_im.data(), x_rt.data(), 2.f, cfg, tin, tout);
  std::vector<float> o_ref(n);
  RunOouraInv(n, o_re.data(), o_im.data(), o_ref.data(), 2.f / n);
  double cross_2 = MaxAbsDiff(x_rt, o_ref);

  printf("n=%zu  FWD re diff=%g  im diff(conj)=%g\n", n, fwd_re, fwd_im);
  printf("  Ooura rt(2/n) err vs 2x = %g\n", o_rt_err);
  printf("  NE10  rt(*1)   err vs 2x = %g\n", rt_1);
  printf("  NE10  rt(*2)   err vs 2x = %g\n", rt_2);
  printf("  NE10  rt(*2/n) err vs 2x = %g\n", rt_2n);
  printf("  cross: NE10 inv(*2) on Ooura spectrum vs Ooura inv = %g\n", cross_2);
  printf("  samples: ooura[0]=%.6f ne10(*2)[0]=%.6f x[0]=%.6f\n",
         o_ref[0], x_rt[0], x[0]);
  ne10_fft_destroy_r2c_float32(cfg);
}
}  // namespace

int main() {
  printf("== 256 ==\n");
  Test(256, 42);
  printf("== 512 ==\n");
  Test(512, 42);
  return 0;
}
