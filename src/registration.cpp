#include "cm/registration.hpp"

#include <Eigen/Dense>
#include <algorithm>
#include <cmath>

#include "cm/retention.hpp"  // splitmix64

namespace cm {

std::optional<RigidTransform> kabsch(const std::vector<Eigen::Vector3d>& src, const std::vector<Eigen::Vector3d>& dst) {
  const std::size_t n = src.size();
  if (n < 3 || dst.size() != n) return std::nullopt;
  Eigen::Vector3d cs = Eigen::Vector3d::Zero(), cd = Eigen::Vector3d::Zero();
  for (std::size_t i = 0; i < n; ++i) {
    cs += src[i];
    cd += dst[i];
  }
  cs /= double(n);
  cd /= double(n);
  Eigen::Matrix3d H = Eigen::Matrix3d::Zero();
  for (std::size_t i = 0; i < n; ++i) H += (src[i] - cs) * (dst[i] - cd).transpose();
  Eigen::JacobiSVD<Eigen::Matrix3d> svd(H, Eigen::ComputeFullU | Eigen::ComputeFullV);
  // Degenerate (collinear / coincident) configurations leave rotation undetermined.
  if (svd.singularValues()(1) < 1e-9 * std::max(1.0, svd.singularValues()(0))) return std::nullopt;
  Eigen::Matrix3d D = Eigen::Matrix3d::Identity();
  if ((svd.matrixV() * svd.matrixU().transpose()).determinant() < 0) D(2, 2) = -1;
  RigidTransform T;
  T.R = svd.matrixV() * D * svd.matrixU().transpose();
  T.t = cd - T.R * cs;
  return T;
}

RansacResult kabschRansac(const std::vector<Eigen::Vector3d>& src, const std::vector<Eigen::Vector3d>& dst,
                          const RansacParams& p) {
  RansacResult best;
  const std::size_t n = src.size();
  if (n < 3) return best;
  std::uint64_t s = p.seed;
  auto rnd = [&](std::size_t m) {
    s = splitmix64(s);
    return static_cast<std::size_t>(s % m);
  };
  auto inliersOf = [&](const RigidTransform& T) {
    std::vector<std::size_t> in;
    for (std::size_t i = 0; i < n; ++i)
      if ((T.apply(src[i]) - dst[i]).norm() < p.inlier_dist) in.push_back(i);
    return in;
  };
  std::vector<std::size_t> best_in;
  for (int it = 0; it < p.iterations; ++it) {
    const std::size_t a = rnd(n), b = rnd(n), c = rnd(n);
    if (a == b || b == c || a == c) continue;
    const auto T = kabsch({src[a], src[b], src[c]}, {dst[a], dst[b], dst[c]});
    if (!T) continue;
    auto in = inliersOf(*T);
    if (in.size() > best_in.size()) best_in = std::move(in);
  }
  if (best_in.size() < 3) return best;
  std::vector<Eigen::Vector3d> s2, d2;
  for (auto i : best_in) {
    s2.push_back(src[i]);
    d2.push_back(dst[i]);
  }
  best.T = kabsch(s2, d2);
  best.inliers = best.T ? inliersOf(*best.T).size() : 0;
  return best;
}

double rotationErrorDeg(const Eigen::Matrix3d& R_est, const Eigen::Matrix3d& R_gt) {
  const double c = std::clamp(((R_gt.transpose() * R_est).trace() - 1.0) / 2.0, -1.0, 1.0);
  return std::acos(c) * 180.0 / M_PI;
}

double pointError(const RigidTransform& est, const RigidTransform& gt, const std::vector<Eigen::Vector3d>& pts) {
  if (pts.empty()) return std::nan("");
  const Eigen::Matrix3d dR = est.R - gt.R;
  const Eigen::Vector3d dt = est.t - gt.t;
  double sum = 0;
  for (const auto& q : pts) sum += (dR * q + dt).norm();
  return sum / double(pts.size());
}

RegistrationError evaluate(const RigidTransform& est, const RigidTransform& gt, const std::vector<Eigen::Vector3d>& pts,
                           double max_rot, double max_trans) {
  RegistrationError e;
  e.rot_deg = rotationErrorDeg(est.R, gt.R);
  e.trans_m = (est.t - gt.t).norm();
  e.e_p = pointError(est, gt, pts);
  e.success = e.rot_deg < max_rot && e.trans_m < max_trans;
  return e;
}

}  // namespace cm
