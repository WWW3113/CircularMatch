#include "cm/matching.hpp"

#include <cmath>
#include <limits>
#include <stdexcept>

namespace cm {

double MatchResult::mean_error() const {
  if (errors.empty()) return std::nan("");
  double s = 0;
  for (double e : errors) s += e;
  return s / double(errors.size());
}

double MatchResult::rmse() const {
  if (errors.empty()) return std::nan("");
  double s = 0;
  for (double e : errors) s += e * e;
  return std::sqrt(s / double(errors.size()));
}

// Classic O(n^2 m) potentials-based Hungarian algorithm (1-indexed internally).
std::vector<int> hungarian(const std::vector<std::vector<double>>& a) {
  const int n = static_cast<int>(a.size());
  if (n == 0) return {};
  const int m = static_cast<int>(a[0].size());
  if (m < n) throw std::runtime_error("hungarian: rows must be <= cols");
  const double INF = std::numeric_limits<double>::infinity();
  std::vector<double> u(n + 1, 0), v(m + 1, 0);
  std::vector<int> p(m + 1, 0), way(m + 1, 0);
  for (int i = 1; i <= n; ++i) {
    p[0] = i;
    int j0 = 0;
    std::vector<double> minv(m + 1, INF);
    std::vector<char> used(m + 1, 0);
    do {
      used[j0] = 1;
      const int i0 = p[j0];
      double delta = INF;
      int j1 = 0;
      for (int j = 1; j <= m; ++j)
        if (!used[j]) {
          const double cur = a[i0 - 1][j - 1] - u[i0] - v[j];
          if (cur < minv[j]) { minv[j] = cur; way[j] = j0; }
          if (minv[j] < delta) { delta = minv[j]; j1 = j; }
        }
      for (int j = 0; j <= m; ++j)
        if (used[j]) { u[p[j]] += delta; v[j] -= delta; }
        else minv[j] -= delta;
      j0 = j1;
    } while (p[j0] != 0);
    do {
      const int j1 = way[j0];
      p[j0] = p[j1];
      j0 = j1;
    } while (j0);
  }
  std::vector<int> ans(n, -1);
  for (int j = 1; j <= m; ++j)
    if (p[j]) ans[p[j] - 1] = j - 1;
  return ans;
}

MatchResult matchPositions(const std::vector<Eigen::Vector2d>& det, const std::vector<Eigen::Vector2d>& ref,
                           double radius) {
  MatchResult r;
  r.n_detected = det.size();
  r.n_reference = ref.size();
  if (det.empty() || ref.empty()) return r;
  const bool transpose = det.size() > ref.size();
  const auto& rows = transpose ? ref : det;
  const auto& cols = transpose ? det : ref;
  // Penalty larger than any possible sum of admissible distances -> maximises
  // the number of admissible pairs first, then minimises total distance.
  const double big = (double(rows.size()) + 1.0) * radius + 1.0;
  std::vector<std::vector<double>> cost(rows.size(), std::vector<double>(cols.size()));
  for (std::size_t i = 0; i < rows.size(); ++i)
    for (std::size_t j = 0; j < cols.size(); ++j) {
      const double d = (rows[i] - cols[j]).norm();
      cost[i][j] = d <= radius ? d : big;
    }
  const auto asg = hungarian(cost);
  for (std::size_t i = 0; i < asg.size(); ++i) {
    const int j = asg[i];
    if (j < 0 || cost[i][j] > radius) continue;
    const int di = transpose ? j : int(i), ri = transpose ? int(i) : j;
    r.pairs.emplace_back(di, ri);
    r.errors.push_back(cost[i][j]);
  }
  return r;
}

}  // namespace cm
