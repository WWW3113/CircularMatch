// CN descriptor matching, paper Section II-B and Algorithm 1.
//
//  1. Gatekeeper (D3): the three gatekeeper bits are compared bit by bit
//     (source bit k vs target bit k); a pair is discarded only if ALL three
//     differ by >= threshold ("completely mismatched").
//  2. DC score: number of descriptor dimensions 3..N-1 where both values are
//     non-empty and differ by < threshold (D15: gatekeeper dims not counted;
//     they are used for the filter above). Each matching dimension yields an
//     index pair (source keypoint, target keypoint).
//  3. For each source descriptor the target with the highest DC (Alg. 1, l. 3-14);
//     for each target the best of its candidates (l. 15-16); kept if DC >= min_dc
//     (D4: text says pairs with DC "below 3" are filtered, Algorithm 1 writes
//     "> 3"; min_dc is a parameter, default 3).
//  4. FM matrix (D5): for every kept candidate pair, each index pair of its
//     matching dimensions is counted once (UpdateFMscore, l. 18). FMscore of a
//     pair (i, j) = FM[i][j].
//  5. MatchScore = w * FM + DC (eq. 7); pairs sorted in descending order. If the
//     first (best) pair scores < th_score: triangle bit comparison on it
//     (l. 26-27); otherwise every pair with score > th_score is a match (l. 28-29).
//  6. Triangle bit comparison (D6): candidate point sets = indices of all matching
//     dimensions of the best pair (source / target). Triangles over each set; a
//     source and a target triangle match if all three side lengths (2-D) agree
//     within threshold under some correspondence of the vertices (order
//     permutable, all three must agree). Vertices of matching triangles give
//     point pairs (D13: conflicts resolved by vote count, one-to-one).
#pragma once
#include <Eigen/Core>
#include <utility>
#include <vector>

#include "cm/cn_descriptor.hpp"

namespace cm {

struct CnMatchResult {
  std::vector<std::pair<int, int>> pairs;  // (source keypoint, target keypoint)
  bool used_triangle = false;
  std::size_t gate_passed = 0;    // descriptor pairs passing the gatekeeper
  std::size_t candidates = 0;     // candidate pairs after DC >= min_dc
  double best_score = 0;          // MatchScore of the first pair
  int best_dc = 0, best_fm = 0;
  std::size_t triangle_set_src = 0, triangle_set_tgt = 0, triangle_matches = 0;
};

CnMatchResult matchCn(const std::vector<CnDescriptor>& src, const std::vector<CnDescriptor>& tgt,
                      const std::vector<Eigen::Vector3d>& src_kp, const std::vector<Eigen::Vector3d>& tgt_kp,
                      const CnParams& p);

// Exposed for tests.
bool cnGatekeeperPass(const CnDescriptor& a, const CnDescriptor& b, double threshold);
int cnDcScore(const CnDescriptor& a, const CnDescriptor& b, double threshold,
              std::vector<std::pair<int, int>>* index_pairs = nullptr);
std::vector<std::pair<int, int>> cnTriangleMatch(const std::vector<int>& src_set, const std::vector<int>& tgt_set,
                                                 const std::vector<Eigen::Vector3d>& src_kp,
                                                 const std::vector<Eigen::Vector3d>& tgt_kp, double threshold,
                                                 std::size_t* n_triangle_matches = nullptr);

}  // namespace cm
