#include "cm/cn_descriptor.hpp"

#include <algorithm>
#include <cmath>
#include <numeric>
#include <stdexcept>

namespace cm {

namespace {
constexpr double kCoincident = 1e-9;  // 2-D distance below which two keypoints have no defined angle
double dist2d(const Eigen::Vector3d& a, const Eigen::Vector3d& b) { return std::hypot(a.x() - b.x(), a.y() - b.y()); }
}  // namespace

double cnAngle(const Eigen::Vector2d& a, const Eigen::Vector2d& v) {
  const double c = std::clamp(a.dot(v) / (a.norm() * v.norm()), -1.0, 1.0);
  const double ac = std::acos(c);
  const double cross_z = a.x() * v.y() - a.y() * v.x();  // (p0p1 x p0pi) . z
  return cross_z >= 0 ? ac : 2.0 * M_PI - ac;
}

CnFeatureMatrix cnFeatureMatrix(const std::vector<Eigen::Vector3d>& kp, int center, const std::array<int, 3>& nb,
                                const CnParams& p, CnStats* stats) {
  const int S = p.N - 3;
  if (S < 1) throw std::runtime_error("CN descriptor: N must be > 3");
  CnFeatureMatrix m;
  const Eigen::Vector2d p0 = kp[center].head<2>();
  for (int e = 0; e < 3; ++e) {
    m.dist[e].assign(S, 0.0);
    m.index[e].assign(S, -1);
    const Eigen::Vector2d a = kp[nb[e]].head<2>() - p0;
    for (int q = 0; q < static_cast<int>(kp.size()); ++q) {
      if (q == center) continue;
      const Eigen::Vector2d v = kp[q].head<2>() - p0;
      const double d = v.norm();
      if (d < kCoincident) {
        if (stats && e == 0) ++stats->coincident_skipped;
        continue;
      }
      int s = static_cast<int>(std::floor(cnAngle(a, v) / (2.0 * M_PI / S)));
      s = std::clamp(s, 0, S - 1);
      // closest point per sector; ties -> lower index (deterministic)
      if (m.index[e][s] < 0 || d < m.dist[e][s]) {
        m.dist[e][s] = d;
        m.index[e][s] = q;
      }
    }
  }
  if (stats) {
    if (m.index[1][0] == nb[0]) ++stats->enc2_sector0_is_p1;
    if (m.index[2][0] == nb[0] || m.index[2][0] == nb[1]) ++stats->enc3_sector0_is_p1_or_p2;
  }
  return m;
}

std::vector<CnDescriptor> buildCnDescriptors(const std::vector<Eigen::Vector3d>& kp, const CnParams& p,
                                             CnStats* stats) {
  const int n = static_cast<int>(kp.size());
  std::vector<CnDescriptor> out(n);
  for (int i = 0; i < n; ++i) {
    CnDescriptor& D = out[i];
    D.center = i;
    D.val.assign(p.N, 0.0);
    D.idx.assign(p.N, -1);
    // three nearest keypoints (2-D), excluding coincident ones; ties -> lower index
    std::vector<std::pair<double, int>> nn;
    for (int q = 0; q < n; ++q) {
      if (q == i) continue;
      const double d = dist2d(kp[i], kp[q]);
      if (d >= kCoincident) nn.emplace_back(d, q);
    }
    if (nn.size() < 3) continue;
    std::partial_sort(nn.begin(), nn.begin() + 3, nn.end());
    for (int k = 0; k < 3; ++k) {
      D.nb[k] = nn[k].second;
      D.val[k] = nn[k].first;  // gatekeeper bits: distances to p1, p2, p3 (D3)
      D.idx[k] = nn[k].second;
    }
    const CnFeatureMatrix m = cnFeatureMatrix(kp, i, D.nb, p, stats);
    for (int s = 0; s < p.N - 3; ++s)
      for (int e = 0; e < 3; ++e)
        if (m.index[e][s] >= 0) {  // priority encode1 > encode2 > encode3 (Fig. 3)
          D.val[3 + s] = m.dist[e][s];
          D.idx[3 + s] = m.index[e][s];
          break;
        }
    D.valid = true;
    if (stats) ++stats->descriptors;
  }
  return out;
}

std::vector<Eigen::Vector3d> mergeKeypoints(const std::vector<Eigen::Vector3d>& kp, double radius) {
  if (radius <= 0) return kp;
  const int n = static_cast<int>(kp.size());
  std::vector<int> parent(n);
  std::iota(parent.begin(), parent.end(), 0);
  auto find = [&](int x) {
    while (parent[x] != x) x = parent[x] = parent[parent[x]];
    return x;
  };
  for (int i = 0; i < n; ++i)
    for (int j = i + 1; j < n; ++j)
      if (dist2d(kp[i], kp[j]) < radius) parent[find(i)] = find(j);
  std::vector<Eigen::Vector3d> sum(n, Eigen::Vector3d::Zero());
  std::vector<int> cnt(n, 0);
  for (int i = 0; i < n; ++i) {
    sum[find(i)] += kp[i];
    ++cnt[find(i)];
  }
  std::vector<Eigen::Vector3d> out;
  for (int i = 0; i < n; ++i)
    if (cnt[i]) out.push_back(sum[i] / cnt[i]);
  return out;
}

}  // namespace cm
