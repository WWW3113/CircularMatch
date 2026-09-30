#include "cm/dtm.hpp"

#include <Eigen/Dense>
#include <algorithm>
#include <cmath>
#include <limits>
#include <stdexcept>
#include <tuple>

namespace cm {

namespace {

struct Sample {
  double x, y, z;
  bool valid = false;
};

// Least-squares plane z = a + b(x-cx) + c(y-cy); false if < 3 points or degenerate.
bool fitPlane(const std::vector<const Sample*>& s, double cx, double cy, double* a, double* b, double* c) {
  if (s.size() < 3) return false;
  Eigen::MatrixXd A(s.size(), 3);
  Eigen::VectorXd z(s.size());
  for (std::size_t k = 0; k < s.size(); ++k) {
    A(k, 0) = 1.0;
    A(k, 1) = s[k]->x - cx;
    A(k, 2) = s[k]->y - cy;
    z(k) = s[k]->z;
  }
  Eigen::ColPivHouseholderQR<Eigen::MatrixXd> qr(A);
  qr.setThreshold(1e-6);
  if (qr.rank() < 3) return false;
  const Eigen::Vector3d sol = qr.solve(z);
  *a = sol(0);
  *b = sol(1);
  *c = sol(2);
  return std::isfinite(*a) && std::isfinite(*b) && std::isfinite(*c);
}

}  // namespace

Dtm Dtm::build(const Cloud& cloud, const DtmParams& p) {
  if (cloud.empty()) throw std::runtime_error("DTM: empty cloud");
  if (!(p.cell > 0)) throw std::runtime_error("DTM: cell must be > 0");
  Dtm d;
  d.cell_ = p.cell;
  double minx = std::numeric_limits<double>::infinity(), miny = minx, maxx = -minx, maxy = -minx;
  for (const auto& q : cloud) {
    minx = std::min(minx, double(q.x)); maxx = std::max(maxx, double(q.x));
    miny = std::min(miny, double(q.y)); maxy = std::max(maxy, double(q.y));
  }
  d.x0_ = std::floor(minx / p.cell) * p.cell;
  d.y0_ = std::floor(miny / p.cell) * p.cell;
  d.nx_ = static_cast<int>(std::floor((maxx - d.x0_) / p.cell)) + 1;
  d.ny_ = static_cast<int>(std::floor((maxy - d.y0_) / p.cell)) + 1;
  if (static_cast<double>(d.nx_) * d.ny_ > 4e8) throw std::runtime_error("DTM: grid too large; increase dtm.cell");
  const int nx = d.nx_, ny = d.ny_;
  d.stats_.nx = nx;
  d.stats_.ny = ny;

  // 1. bucket points by cell (sorted, deterministic), pick k-th percentile.
  std::vector<std::pair<std::int64_t, int>> keyed(cloud.size());
  for (std::size_t i = 0; i < cloud.size(); ++i) {
    const int ix = std::min(nx - 1, static_cast<int>(std::floor((cloud[i].x - d.x0_) / p.cell)));
    const int iy = std::min(ny - 1, static_cast<int>(std::floor((cloud[i].y - d.y0_) / p.cell)));
    keyed[i] = {static_cast<std::int64_t>(iy) * nx + ix, static_cast<int>(i)};
  }
  std::sort(keyed.begin(), keyed.end());
  std::vector<Sample> samples(static_cast<std::size_t>(nx) * ny);
  // (z, x, y, index): ties in z (common with quantised files) are broken by
  // coordinates, so the chosen sample does not depend on input order.
  std::vector<std::tuple<float, float, float, int>> zs;
  for (std::size_t b = 0; b < keyed.size();) {
    std::size_t e = b;
    while (e < keyed.size() && keyed[e].first == keyed[b].first) ++e;
    if (static_cast<int>(e - b) >= p.min_points) {
      zs.clear();
      for (std::size_t k = b; k < e; ++k) {
        const auto& q = cloud[keyed[k].second];
        zs.emplace_back(q.z, q.x, q.y, keyed[k].second);
      }
      std::size_t kth = static_cast<std::size_t>(std::floor(p.percentile / 100.0 * double(zs.size() - 1)));
      if (p.ground_select == "supported_lowest") {
        // Lowest point that has >= support_count points within support_dz above it:
        // drops isolated below-ground noise without depending on how many
        // non-ground (stem/canopy) points share the cell.
        std::sort(zs.begin(), zs.end());
        for (std::size_t k = 0; k < zs.size(); ++k) {
          std::size_t above = 0;
          for (std::size_t m = k + 1; m < zs.size() && std::get<0>(zs[m]) <= std::get<0>(zs[k]) + p.support_dz; ++m)
            ++above;
          if (int(above) >= p.support_count) { kth = k; break; }
        }
      } else if (p.ground_select != "percentile") {
        throw std::runtime_error("dtm.ground_select must be percentile or supported_lowest");
      } else {
        std::nth_element(zs.begin(), zs.begin() + kth, zs.end());
      }
      const auto& q = cloud[std::get<3>(zs[kth])];
      samples[keyed[b].first] = Sample{q.x, q.y, q.z, true};
      ++d.stats_.cells_with_sample;
    }
    b = e;
  }

  auto gather = [&](int i, int j, int r, bool skip_self, std::vector<const Sample*>& out) {
    out.clear();
    for (int jj = std::max(0, j - r); jj <= std::min(ny - 1, j + r); ++jj)
      for (int ii = std::max(0, i - r); ii <= std::min(nx - 1, i + r); ++ii) {
        if (skip_self && ii == i && jj == j) continue;
        const Sample& s = samples[static_cast<std::size_t>(jj) * nx + ii];
        if (s.valid) out.push_back(&s);
      }
  };

  // 2. reject samples far above their neighbours' plane (single pass on the original samples).
  std::vector<const Sample*> nb;
  std::vector<char> reject(samples.size(), 0);
  for (int j = 0; j < ny; ++j)
    for (int i = 0; i < nx; ++i) {
      const Sample& s = samples[static_cast<std::size_t>(j) * nx + i];
      if (!s.valid) continue;
      gather(i, j, p.fit_radius, true, nb);
      double a, b, c;
      if (!fitPlane(nb, s.x, s.y, &a, &b, &c)) continue;
      if (s.z - a > p.outlier_above) reject[static_cast<std::size_t>(j) * nx + i] = 1;
    }
  for (std::size_t k = 0; k < samples.size(); ++k)
    if (reject[k]) {
      samples[k].valid = false;
      ++d.stats_.samples_rejected;
    }

  // 3. local plane per cell.
  d.planes_.assign(samples.size(), Plane{});
  for (int j = 0; j < ny; ++j)
    for (int i = 0; i < nx; ++i) {
      const double cx = d.x0_ + (i + 0.5) * p.cell, cy = d.y0_ + (j + 0.5) * p.cell;
      Plane& pl = d.planes_[static_cast<std::size_t>(j) * nx + i];
      for (int r = p.fit_radius; r <= std::max(p.fit_radius, p.fill_max_radius); ++r) {
        gather(i, j, r, false, nb);
        if (fitPlane(nb, cx, cy, &pl.a, &pl.b, &pl.c)) {
          pl.valid = true;
          break;
        }
      }
      if (pl.valid) {
        ++d.stats_.cells_valid;
        if (!samples[static_cast<std::size_t>(j) * nx + i].valid) ++d.stats_.cells_filled;
      }
    }
  return d;
}

double Dtm::eval(int i, int j, double x, double y) const {
  const Plane& pl = planes_[static_cast<std::size_t>(j) * nx_ + i];
  const double cx = x0_ + (i + 0.5) * cell_, cy = y0_ + (j + 0.5) * cell_;
  return pl.a + pl.b * (x - cx) + pl.c * (y - cy);
}

std::optional<double> Dtm::height(double x, double y) const {
  const int ci = static_cast<int>(std::floor((x - x0_) / cell_));
  const int cj = static_cast<int>(std::floor((y - y0_) / cell_));
  if (ci < 0 || cj < 0 || ci >= nx_ || cj >= ny_) return std::nullopt;
  if (!planes_[static_cast<std::size_t>(cj) * nx_ + ci].valid) return std::nullopt;
  const double gx = (x - x0_) / cell_ - 0.5, gy = (y - y0_) / cell_ - 0.5;
  const int i0 = static_cast<int>(std::floor(gx)), j0 = static_cast<int>(std::floor(gy));
  const double fx = gx - i0, fy = gy - j0;
  double sum = 0, wsum = 0;
  for (int dj = 0; dj <= 1; ++dj)
    for (int di = 0; di <= 1; ++di) {
      const int i = i0 + di, j = j0 + dj;
      if (i < 0 || j < 0 || i >= nx_ || j >= ny_) continue;
      if (!planes_[static_cast<std::size_t>(j) * nx_ + i].valid) continue;
      const double w = (di ? fx : 1 - fx) * (dj ? fy : 1 - fy);
      sum += w * eval(i, j, x, y);
      wsum += w;
    }
  if (wsum <= 1e-12) return eval(ci, cj, x, y);
  return sum / wsum;
}

std::vector<int> heightFilter(const Cloud& cloud, const Dtm& dtm, const HeightParams& p, std::size_t* no_dtm) {
  std::vector<int> out;
  std::size_t missing = 0;
  for (std::size_t i = 0; i < cloud.size(); ++i) {
    const auto h = dtm.height(cloud[i].x, cloud[i].y);
    if (!h) { ++missing; continue; }
    const double dz = cloud[i].z - *h;
    if (dz >= p.zmin - p.neg_tol && dz <= p.zmax) out.push_back(static_cast<int>(i));
  }
  if (no_dtm) *no_dtm = missing;
  return out;
}

}  // namespace cm
