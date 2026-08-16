#include "dereverberation/mel_filterbank.h"

#include <algorithm>
#include <cmath>
#include <numeric>
#include <vector>

namespace dereverberation {

namespace {

// Result of integrating triangle products between output posts (fout) and
// input posts (fin). All indices are 1-based (mirroring the MATLAB code).
struct TriIntegrateResult {
  std::vector<int> iout;  // output post index of each triangle pair
  std::vector<int> iin;   // input post index of each triangle pair
  std::vector<double> w;   // integral weight of each triangle pair
};

// Direct port of the triangle-product integration shared by v_filtbankm and
// interpofiltbankm. fout and fin are the (ascending) output and input post
// frequencies in Hz; both may contain dummy end points.
// If ffact_override is non-empty it supplies the triangle gains directly
// (interpofiltbankm style); otherwise unit gains are used (v_filtbankm style).
TriIntegrateResult TriIntegrate(const std::vector<double>& fout,
                                const std::vector<double>& fin,
                                const std::vector<double>& ffact_override = {}) {
  const int nfout = static_cast<int>(fout.size());
  const int nfin = static_cast<int>(fin.size());
  const int nfall = nfout + nfin;

  // 1-based arrays (index 0 unused) so formulas match MATLAB exactly.
  std::vector<double> foutin(nfall + 1);
  for (int i = 0; i < nfout; ++i) foutin[i + 1] = fout[i];
  for (int i = 0; i < nfin; ++i) foutin[nfout + 1 + i] = fin[i];

  std::vector<double> wleft(nfall + 1), wright(nfall + 1), ffact(nfall + 1);
  wleft[1] = 0.;
  for (int i = 2; i <= nfout; ++i) wleft[i] = fout[i - 1] - fout[i - 2];
  wleft[nfout + 1] = 0.;
  for (int i = 2; i <= nfin; ++i) wleft[nfout + i] = fin[i - 1] - fin[i - 2];
  for (int i = 1; i < nfall; ++i) wright[i] = wleft[i + 1];
  wright[nfall] = 0.;

  if (ffact_override.empty()) {
    ffact[1] = 0.;
    for (int i = 2; i <= nfout - 1; ++i) ffact[i] = 1.;
    ffact[nfout] = 0.;
    ffact[nfout + 1] = 0.;
    for (int i = nfout + 2; i <= nfall - 1; ++i) ffact[i] = 1.;
    ffact[nfall] = 0.;
  } else {
    for (int i = 1; i <= nfall; ++i) ffact[i] = ffact_override[i];
  }
  for (int i = 1; i <= nfall; ++i) {
    if (wleft[i] + wright[i] == 0.) ffact[i] = 0.;
  }

  // Stable sort of foutin: ifall[t] = 1-based index of the t-th smallest.
  std::vector<int> ifall(nfall + 1);
  for (int i = 0; i < nfall; ++i) ifall[i + 1] = i;
  std::stable_sort(ifall.begin() + 1, ifall.end(), [&](int a, int b) {
    return foutin[a + 1] < foutin[b + 1];
  });
  std::vector<int> jfall(nfall + 1);
  for (int t = 1; t <= nfall; ++t) jfall[ifall[t] + 1] = t;

  // Zap input nodes that lie outside the output filters.
  const int a = std::max(jfall[1], jfall[nfout + 1]);
  const int b = std::min(jfall[nfout], jfall[nfall]);
  for (int t = 1; t <= a - 2; ++t) ffact[ifall[t] + 1] = 0.;
  for (int t = b + 2; t <= nfall; ++t) ffact[ifall[t] + 1] = 0.;

  // Next output/input post to the right of each node (1-based positions).
  std::vector<int> nxto(nfall + 1), nxti(nfall + 1);
  int co = 0, ci = 0;
  for (int t = 1; t <= nfall; ++t) {
    if (ifall[t] < nfout) {
      ++co;
    } else {
      ++ci;
    }
    nxto[t] = co;
    nxti[t] = ci;
  }
  std::vector<int> nxtr_sorted(nfall + 1);
  for (int t = 1; t <= nfall; ++t) {
    nxtr_sorted[t] = std::min(nxti[t] + 1 + nfout, nfall);
    if (ifall[t] >= nfout) nxtr_sorted[t] = 1 + nxto[t];
  }
  std::vector<int> nxtr(nfall + 1);
  for (int i = 1; i <= nfall; ++i) nxtr[i] = nxtr_sorted[jfall[i]];

  auto msk0 = [&](int i) { return ffact[i] > 0.; };

  // Integrate product of lower triangles.
  std::vector<int> ix1, jx1;
  std::vector<double> wx1;
  for (int i = 1; i <= nfall; ++i) {
    if (!msk0(i) || !msk0(nxtr[i])) continue;
    ix1.push_back(i);
    jx1.push_back(nxtr[i]);
    const double vfgx = foutin[i] - foutin[nxtr[i] - 1];
    const double yx = std::min(wleft[i], vfgx);
    const double den = wleft[i] * wleft[nxtr[i]] + (yx == 0. ? 1. : 0.);
    wx1.push_back(ffact[i] * ffact[nxtr[i]] * yx *
                  (wleft[i] * vfgx - yx * (0.5 * (wleft[i] + vfgx) - yx / 3.)) /
                  den);
  }

  // Integrate product of upper triangles.
  // nxtu = max([nxtr(2:end)-1 0], 1)
  std::vector<int> nxtu(nfall + 1);
  for (int i = 1; i < nfall; ++i) nxtu[i] = std::max(nxtr[i + 1] - 1, 1);
  nxtu[nfall] = 1;
  std::vector<int> ix2, jx2;
  std::vector<double> wx2;
  for (int i = 1; i <= nfall; ++i) {
    if (!msk0(i) || !msk0(nxtu[i])) continue;
    const double vfgx = foutin[i + 1] - foutin[nxtu[i]];
    double yx = std::min(wright[i], vfgx);
    if (foutin[nxtu[i] + 1] < foutin[i + 1]) yx = 0.;
    const double den = wright[i] * wright[nxtu[i]] + (yx == 0. ? 1. : 0.);
    wx2.push_back(ffact[i] * ffact[nxtu[i]] * yx * yx *
                  (0.5 * (wright[nxtu[i]] - vfgx) + yx / 3.) / den);
    ix2.push_back(i);
    jx2.push_back(nxtu[i]);
  }

  // Integrate lower triangle and upper triangle ending to its right.
  std::vector<int> nxtu2(nfall + 1);
  for (int i = 1; i <= nfall; ++i) nxtu2[i] = std::max(nxtr[i] - 1, 1);
  std::vector<int> ix3, jx3;
  std::vector<double> wx3;
  for (int i = 1; i <= nfall; ++i) {
    if (!msk0(i) || !msk0(nxtu2[i])) continue;
    const double vfgx = foutin[i] - foutin[nxtu2[i]];
    double yx = std::min(wleft[i], vfgx);
    if (foutin[nxtu2[i] + 1] < foutin[i]) yx = 0.;
    const double den = wleft[i] * wright[nxtu2[i]] + (yx == 0. ? 1. : 0.);
    wx3.push_back(ffact[i] * ffact[nxtu2[i]] * yx *
                  (wleft[i] * (wright[nxtu2[i]] - vfgx) +
                   yx * (0.5 * (wleft[i] - wright[nxtu2[i]] + vfgx) - yx / 3.)) /
                  den);
    ix3.push_back(i);
    jx3.push_back(nxtu2[i]);
  }

  // Integrate upper triangle and lower triangle starting to its right.
  std::vector<int> nxtu3(nfall + 1);
  for (int i = 1; i < nfall; ++i) nxtu3[i] = nxtr[i + 1];
  nxtu3[nfall] = 1;
  std::vector<int> ix4, jx4;
  std::vector<double> wx4;
  for (int i = 1; i <= nfall; ++i) {
    if (!msk0(i) || !msk0(nxtu3[i])) continue;
    const double vfgx = foutin[i + 1] - foutin[nxtu3[i] - 1];
    const double yx = std::min(wright[i], vfgx);
    const double den = wright[i] * wleft[nxtu3[i]] + (yx == 0. ? 1. : 0.);
    wx4.push_back(ffact[i] * ffact[nxtu3[i]] * yx * yx * (0.5 * vfgx - yx / 3.) /
                  den);
    ix4.push_back(i);
    jx4.push_back(nxtu3[i]);
  }

  // Assemble and sort triangle pairs per column (iox = sort of the 2xN stack).
  TriIntegrateResult res;
  auto add_pair = [&](int i, int j, double w) {
    int lo = std::min(i, j), hi = std::max(i, j);
    res.iout.push_back(lo);
    res.iin.push_back(hi);
    res.w.push_back(w);
  };
  for (size_t t = 0; t < ix1.size(); ++t) add_pair(ix1[t], jx1[t], wx1[t]);
  for (size_t t = 0; t < ix2.size(); ++t) add_pair(ix2[t], jx2[t], wx2[t]);
  for (size_t t = 0; t < ix3.size(); ++t) add_pair(ix3[t], jx3[t], wx3[t]);
  for (size_t t = 0; t < ix4.size(); ++t) add_pair(ix4[t], jx4[t], wx4[t]);

  // Map negative input frequencies to positive ones.
  const double half = static_cast<double>(nfall + nfout) * 0.5;
  for (size_t t = 0; t < res.iin.size(); ++t) {
    if (res.iin[t] <= half) res.iin[t] = (nfall + nfout + 1) - res.iin[t];
  }
  return res;
}

}  // namespace

Eigen::MatrixXd ComputeMelFilterBank(int p, int n, double fs) {
  const int nf = 1 + n / 2;
  const double df = fs / static_cast<double>(n);
  const double melrng = HzToMel(0.5 * fs) - HzToMel(0.);
  const double melinc = melrng / static_cast<double>(p + 1);

  // Centre frequencies in mel including dummy ends, then back to Hz.
  std::vector<double> fout(p + 2);
  for (int i = 0; i <= p + 1; ++i) {
    double cf = static_cast<double>(i) * melinc;
    if (i > 0) cf = std::max(cf, 0.);
    fout[i] = std::min(MelToHz(cf), 0.5 * fs);
  }

  // Two-sided input frequencies: (-nf:nf)*df.
  std::vector<double> fin(2 * nf + 1);
  for (int i = -nf; i <= nf; ++i) fin[i + nf] = static_cast<double>(i) * df;

  TriIntegrateResult r = TriIntegrate(fout, fin);
  const int nfout = static_cast<int>(fout.size());

  Eigen::MatrixXd x = Eigen::MatrixXd::Zero(p, nf);
  for (size_t t = 0; t < r.w.size(); ++t) {
    // MATLAB 1-based row = iox(1)-1 -> 0-based row = iox(1)-2.
    const int row = r.iout[t] - 2;  // 0..p-1 (dummy ends never appear)
    const int col = std::max(r.iin[t] - nfout - nf, 1) - 1;
    if (row >= 0 && row < p && col >= 0 && col < nf) {
      x(row, col) += r.w[t];
    }
  }

  // Input gains (uniform grid -> 1/df everywhere): x = x * diag(gind).
  x *= 1. / df;
  return x;
}

Eigen::MatrixXd ComputeMelInterpMatrix(int p, int n, double fs) {
  const int nf0 = 1 + n / 2;  // number of input frequency bins
  const double df = fs / static_cast<double>(n);

  // Input bins including one dummy below DC: cf = f1 + (0:nf0)*df.
  std::vector<double> cf(nf0 + 2);
  cf[0] = 0. - df;  // cf(1)-df
  for (int i = 1; i <= nf0 + 1; ++i) cf[i] = static_cast<double>(i - 1) * df;

  // Filter centre frequencies in mel -> Hz (including dummy ends).
  const double melrng = HzToMel(0.5 * fs) - HzToMel(0.);
  const double melinc = melrng / static_cast<double>(p + 1);
  std::vector<double> fin0(p + 2);
  for (int i = 0; i <= p + 1; ++i) {
    double c = static_cast<double>(i) * melinc;
    if (i > 0) c = std::max(c, 0.);
    fin0[i] = MelToHz(c);
  }

  // Two-sided filter frequencies: [-fin0(end:-1:2), fin0].
  std::vector<double> fin2;
  fin2.reserve(2 * (p + 2) - 1);
  for (int i = static_cast<int>(fin0.size()) - 1; i >= 1; --i) {
    fin2.push_back(-fin0[i]);
  }
  for (double f : fin0) fin2.push_back(f);

  // Triangle gains as in interpofiltbankm: gout = ones for the output side,
  // gin = 2/(width) for the input (mel filter) side.
  // ffact = [0 gout 0 0 gin(1:min(nf,nfin-nf-2)) zeros(1,max(nfin-2*nf-2,0))
  //          gin(nfin-nf-1:nfin-2) 0]
  const int mfout = static_cast<int>(cf.size());
  const int nfin2 = static_cast<int>(fin2.size());
  const int nf2 = (nfin2 - 3) / 2;  // = p
  std::vector<double> ffact2(mfout + nfin2 + 1, 0.);
  for (int i = 2; i <= mfout - 1; ++i) ffact2[i] = 1.;  // gout
  const int g1 = std::min(nf2, nfin2 - nf2 - 2);
  const int g0 = std::max(nfin2 - 2 * nf2 - 2, 0);
  for (int k = 1; k <= g1; ++k) {
    const double d = fin2[k + 1] - fin2[k - 1];
    ffact2[mfout + 1 + k] = (d == 0.) ? 0. : 2. / d;  // gin(1..g1)
  }
  const int g2 = nfin2 - nf2 - 1;  // gin(g2 .. nfin2-2)
  for (int k = g2; k <= nfin2 - 2; ++k) {
    const double d = fin2[k + 1] - fin2[k - 1];
    ffact2[mfout + 2 + g1 + g0 + (k - g2)] = (d == 0.) ? 0. : 2. / d;
  }

  TriIntegrateResult r = TriIntegrate(cf, fin2, ffact2);
  const int nfout = static_cast<int>(cf.size());  // mfout
  const int nfall = nfout + static_cast<int>(fin2.size());
  const int nf = (static_cast<int>(fin2.size()) - 3) / 2;  // = p

  Eigen::MatrixXd x = Eigen::MatrixXd::Zero(nf0, nf);
  for (size_t t = 0; t < r.w.size(); ++t) {
    // MATLAB 1-based row = iox(1)-1-lowex (lowex==0) -> 0-based row = iox(1)-2.
    const int row = r.iout[t] - 2;
    const int col = std::max(r.iin[t] - nfall + nf + 1, 1) - 1;
    if (row >= 0 && row < nf0 && col >= 0 && col < nf) {
      x(row, col) += r.w[t];
    }
  }
  return x;
}

}  // namespace dereverberation
