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
  std::size_t n_points = 0, n_inliers = 0;
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
