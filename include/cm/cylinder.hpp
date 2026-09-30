// RANSAC cylinder fit (PCL SampleConsensusModelCylinder + RandomSampleConsensus)
// with LM refit and post-hoc checks.
//
// Seeding (verified against the installed PCL 1.14 headers, see README):
//  - SampleConsensusModel seeds its protected boost::mt19937 rng_alg_ with the
//    constant 12345 (random=false) - reproducible but never varies. rng_gen_
//    references rng_alg_, so re-seeding rng_alg_ in a subclass takes effect.
//  - RandomSampleConsensus draws all samples through the model's generator;
//    threads_ defaults to -1 (no OpenMP); we set it to -1 explicitly.
// SeededCylinderModel::reseed() therefore gives per-cluster control. The
// sampling order also depends on the order of indices, which is fixed here.
//
// Inliers: RANSAC/LM use PCL's normal-weighted distance
//   w * angle(normal, surface normal) + (1 - w) * |euclidean distance|,
// but the reported n_inliers and the inlier-ratio check use the purely
// GEOMETRIC point-to-surface distance ||q - axis| - r| <= dist_threshold.
// (With w = 0.1 an 11-degree normal error alone exceeds 2 cm, and normals from
// a 10 cm neighbourhood on thin stems are that noisy, so the weighted count
// would reject correct fits for a reason unrelated to the fit.)
//
// Inlier counting and the LM refit on geometric inliers are IMPLEMENTATION
// CHOICES (the paper does not specify them), not the paper's method.
//
// Two post-fit checks are OUR additions (not in the paper), each can be
// disabled (cylinder.check_normal_consistency / check_arc_coverage); their
// metrics are always computed and reported. Motivation, from the synthetic scene: the verticality filter (> 0.9) leaves only narrow strips of
// tilted stems, and RANSAC fits wrong cylinders to such strips.
//  - normal consistency: >= min_normal_ratio of the points have a normal within
//    normal_max_angle_deg of the fitted surface normal (rejects tiny-radius
//    fits to fragments; correct fits scored 0.89-1.00, fragments 0.15-0.65);
//  - arc coverage: geometric inliers must span >= min_arc_deg around the axis
//    (a narrow arc does not constrain the radius; correct fits 120-195 deg,
//    wrong strip fits 5-80 deg).
//
// Radius / tilt limits are NOT passed to RANSAC (PCL would silently discard
// such models and they would only show up as "no model"); they are checked
// after the fit so that every failure has a specific reason.
#pragma once
#include <Eigen/Core>
#include <cstdint>
#include <string>
#include <vector>

#include "cm/config.hpp"
#include "cm/types.hpp"

namespace cm {

enum class FitFail : int {
  None = 0,
  TooFewPoints,       // 點數不足
  NoModel,            // RANSAC 無解
  LowInlierRatio,     // inlier 比例過低
  RadiusOutOfRange,   // 半徑超出範圍
  TiltTooLarge,       // 軸傾角過大
  NormalInconsistent, // 點法向量與擬合面不一致 (our addition; rejects fits to strips/fragments)
  ArcCoverageLow,     // inlier 繞軸覆蓋角過小 (our addition; radius unobservable from a narrow arc)
  NoDtmIntersection,  // 軸與 DTM 無交點 (set by the pipeline)
  Count
};
const char* toString(FitFail f);

struct CylinderFit {
  FitFail fail = FitFail::None;  // first failed check, in enum order
  unsigned fail_mask = 0;        // all failed checks (bit = enum value)
  Eigen::Vector3d axis_point = Eigen::Vector3d::Zero();
  Eigen::Vector3d axis_dir = Eigen::Vector3d::UnitZ();  // unit, dir.z >= 0
  double radius = 0, tilt_deg = 0;
  std::size_t n_points = 0;
  std::size_t n_inliers = 0;      // geometric inliers (used for the ratio check)
  std::size_t n_inliers_sac = 0;  // PCL normal-weighted inliers after refit
  double arc_deg = 0;             // angular coverage of geometric inliers around the axis (5-degree bins)
  std::size_t n_normal_ok = 0;    // points whose normal is within normal_max_angle_deg of the fitted surface normal
  Eigen::Vector3d centroid = Eigen::Vector3d::Zero();
  std::uint32_t ransac_seed = 0;
};

// Fits a cylinder to cloud[idx] with normals[idx].
CylinderFit fitCylinder(const Cloud& cloud, const NormalCloud& normals, const std::vector<int>& idx,
                        const CylinderParams& p, std::uint32_t ransac_seed);

// Seed for one cluster: hash(base seed, quantised cluster centroid) -> independent of cluster order.
std::uint32_t clusterSeed(std::uint64_t base, const Cloud& cloud, const std::vector<int>& idx);

// Test hook: returns the first `n` RANSAC sample index sets drawn by a
// SeededCylinderModel on `cloud` after reseed(seed).
std::vector<std::vector<int>> drawSamplesForTest(const Cloud& cloud, const NormalCloud& normals, std::uint32_t seed,
                                                 int n);

}  // namespace cm
