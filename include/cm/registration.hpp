// Rigid transform from matched keypoints and its evaluation.
//
// The paper only says "calculate the Euclidean transform" from the matched
// pairs; the solver is NOT specified (D7). Two solvers are provided and
// reported separately:
//   svd    : Kabsch / SVD on all matched pairs (3-D keypoint positions)
//   ransac : Kabsch inside RANSAC over the matched pairs (minimal sample 3,
//            inlier distance and iterations are our parameters), refit on inliers
//
// Evaluation (D10, D11; coarse transform only, no ICP):
//   rotation error  = angle of R_gt^T R_est [deg]
//   translation err = |t_est - t_gt| [m]
//   e_p (paper eq. 8) = mean over ALL points p of the source scan of
//                       |R_est p + t_est - (R_gt p + t_gt)|
//   success        = rotation error < 2 deg AND translation error < 0.5 m
#pragma once
#include <Eigen/Core>
#include <cstdint>
#include <optional>
#include <utility>
#include <vector>

namespace cm {

struct RigidTransform {
  Eigen::Matrix3d R = Eigen::Matrix3d::Identity();
  Eigen::Vector3d t = Eigen::Vector3d::Zero();
  Eigen::Vector3d apply(const Eigen::Vector3d& p) const { return R * p + t; }
};

// Least-squares rigid transform src -> dst; nullopt if < 3 pairs or degenerate.
std::optional<RigidTransform> kabsch(const std::vector<Eigen::Vector3d>& src, const std::vector<Eigen::Vector3d>& dst);

struct RansacParams {
  int iterations = 1000;
  double inlier_dist = 0.3;  // [m], 3-D
  std::uint64_t seed = 1;
};

struct RansacResult {
  std::optional<RigidTransform> T;
  std::size_t inliers = 0;
};

RansacResult kabschRansac(const std::vector<Eigen::Vector3d>& src, const std::vector<Eigen::Vector3d>& dst,
                          const RansacParams& p);

struct RegistrationError {
  double rot_deg = 0, trans_m = 0, e_p = 0;
  bool success = false;
};

double rotationErrorDeg(const Eigen::Matrix3d& R_est, const Eigen::Matrix3d& R_gt);
// e_p over all points (eq. 8).
double pointError(const RigidTransform& est, const RigidTransform& gt, const std::vector<Eigen::Vector3d>& pts);
RegistrationError evaluate(const RigidTransform& est, const RigidTransform& gt, const std::vector<Eigen::Vector3d>& pts,
                           double max_rot_deg = 2.0, double max_trans_m = 0.5);

}  // namespace cm
