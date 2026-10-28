#pragma once
#include <limits>
#include "cm/cn_matching.hpp"

namespace cm {

// Optional experiments beyond the paper's score-only triangle fallback.
enum class CnRecoveryMode { Off, Triangles, EvidenceConsensus };
struct CnRecoveryParams {
  CnRecoveryMode mode = CnRecoveryMode::Off;
  std::size_t trigger_matches = 3;
  std::size_t top_candidates = 3;
  std::size_t max_evidence_pairs = 60;
  std::size_t min_support = 6;
  double inlier_distance = 0.30;  // full 3-D residual [m]
};
struct CnRecoveryResult {
  std::vector<std::pair<int,int>> pairs;
  bool triggered = false, accepted = false;
  std::size_t hypotheses = 0, support = 0;
  double median_residual = std::numeric_limits<double>::quiet_NaN();
};

// Only descriptors, extracted trees and fixed parameters enter inference.
// No scan poses, ground truth, raw-cloud ICP or reduced-DOF solver are used.
CnRecoveryResult recoverCn(const CnMatchResult& initial,
                          const std::vector<Eigen::Vector3d>& source,
                          const std::vector<Eigen::Vector3d>& target,
                          const CnParams& cn, const CnRecoveryParams& params);

}  // namespace cm
